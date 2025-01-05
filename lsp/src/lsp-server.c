/*
 * Copyright 2023 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lsp-server.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"
#include "lsp-sync.h"
#include "lsp-diagnostics.h"
#include "lsp-log.h"
#include "lsp-semtokens.h"
#include "lsp-progress.h"
#include "lsp-symbols.h"
#include "lsp-symbol-kinds.h"
#include "lsp-highlight.h"
#include "lsp-workspace-folders.h"

#include "spawn/spawn.h"

#include <jsonrpc-glib.h>

#ifdef G_OS_UNIX
# include <gio/gunixinputstream.h>
# include <gio/gunixoutputstream.h>
#else
# include "spawn/lspunixinputstream.h"
# include "spawn/lspunixoutputstream.h"
#endif


static void start_lsp_server(LspServer *server);
static LspServer *lsp_server_init(gint ft);


extern GeanyData *geany_data;
extern GeanyPlugin *geany_plugin;
extern LspProjectConfigurationType project_configuration_type;

static GPtrArray *lsp_servers = NULL;
static GPtrArray *servers_in_shutdown = NULL;

static LspServerInitializedCallback lsp_server_initialized_cb;


static void free_config(LspServerConfig *cfg)
{
	g_free(cfg->cmd);
	g_strfreev(cfg->env);
	g_free(cfg->ref_lang);
	g_strfreev(cfg->autocomplete_trigger_sequences);
	g_strfreev(cfg->semantic_tokens_types);
	g_free(cfg->command_on_save_regex);
	g_free(cfg->semantic_tokens_type_style);
	g_free(cfg->autocomplete_hide_after_words);
	g_free(cfg->diagnostics_disable_for);
	g_free(cfg->diagnostics_error_style);
	g_free(cfg->diagnostics_warning_style);
	g_free(cfg->diagnostics_info_style);
	g_free(cfg->diagnostics_hint_style);
	g_free(cfg->highlighting_style);
	g_free(cfg->trace_value);
	g_free(cfg->code_lens_style);
	g_free(cfg->formatting_options_file);
	g_free(cfg->formatting_options);
	g_free(cfg->initialization_options_file);
	g_free(cfg->initialization_options);
	g_free(cfg->word_chars);
	g_free(cfg->document_symbols_tab_label);
	g_free(cfg->rpc_log);
	g_strfreev(cfg->lang_id_mappings);
	g_ptr_array_free(cfg->command_regexes, TRUE);
	g_strfreev(cfg->project_root_marker_patterns);
}


static void free_server(LspServer *s)
{
	if (s->rpc)
		lsp_rpc_destroy(s->rpc);
	if (s->stream)
		g_object_unref(s->stream);
	lsp_log_stop(s->log);
	lsp_sync_free(s);
	lsp_diagnostics_free(s);
	lsp_workspace_folders_free(s);

	g_free(s->autocomplete_trigger_chars);
	g_free(s->signature_trigger_chars);
	g_free(s->initialize_response);
	lsp_progress_free_all(s);

	free_config(&s->config);

	g_free(s);
}


static gboolean free_server_after_delay(gpointer user_data)
{
	free_server((LspServer *)user_data);

	return G_SOURCE_REMOVE;
}


static gboolean is_dead(LspServer *server)
{
	return server->restarts >= 10;
}


static void process_stopped(GPid pid, gint status, gpointer data)
{
	LspServer *s = data;

	g_spawn_close_pid(pid);
	s->pid = 0;

	// normal shutdown
	if (g_ptr_array_find(servers_in_shutdown, s, NULL))
	{
		msgwin_status_add(_("LSP server %s stopped"), s->config.cmd);
		g_ptr_array_remove_fast(servers_in_shutdown, s);
	}
	else  // crash
	{
		gint restarts = s->restarts;
		gint ft = s->filetype;

		msgwin_status_add(_("LSP server %s stopped unexpectedly, restarting"), s->config.cmd);

		// it seems that calls/notifications get delivered to the plugin
		// from the server even after the process is stopped in unnormal
		// conditions like server crash and if we free the server immediately,
		// the RPC call gets invalid server. Wait for a while until such
		// calls get performed
		plugin_timeout_add(geany_plugin, 300, free_server_after_delay, s);

		if (lsp_servers)  // NULL on plugin unload
		{
			s = lsp_server_init(ft);
			s->restarts = restarts + 1;
			lsp_servers->pdata[ft] = s;
			if (is_dead(s))
				msgwin_status_add(_("LSP server %s terminated %d times, giving up"), s->config.cmd, s->restarts);
			else
				start_lsp_server(s);
		}
	}
}


static void kill_server(LspServer *srv)
{
	if (srv->pid > 0)
	{
		GError *error = NULL;
		if (!spawn_kill_process(srv->pid, &error))
		{
			msgwin_status_add(_("Failed to send SIGTERM to server: %s"), error->message);
			g_error_free(error);
		}
	}
}


static gboolean kill_cb(gpointer user_data)
{
	LspServer *srv = user_data;

	if (g_ptr_array_find(servers_in_shutdown, srv, NULL))
	{
		msgwin_status_add(_("Force terminating LSP server %s"), srv->config.cmd);
		kill_server(srv);
	}

	return G_SOURCE_REMOVE;
}


static void shutdown_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	LspServer *srv = user_data;

	if (!g_ptr_array_find(servers_in_shutdown, srv, NULL))
		return;

	if (!error)
	{
		msgwin_status_add(_("Sending exit notification to LSP server %s"), srv->config.cmd);
		lsp_rpc_notify(srv, "exit", NULL, NULL, srv);
	}
	else
	{
		msgwin_status_add(_("Force terminating LSP server %s"), srv->config.cmd);
		kill_server(srv);
	}

	plugin_timeout_add(geany_plugin, 2000, kill_cb, srv);
}


static void stop_process(LspServer *s)
{
	if (g_ptr_array_find(servers_in_shutdown, s, NULL))
		return;

	s->startup_shutdown = TRUE;
	g_ptr_array_add(servers_in_shutdown, s);

	if (lsp_servers)  // NULL on plugin unload
		lsp_servers->pdata[s->filetype] = lsp_server_init(s->filetype);

	msgwin_status_add(_("Sending shutdown request to LSP server %s"), s->config.cmd);
	lsp_rpc_call_startup_shutdown(s, "shutdown", NULL, shutdown_cb, s);

	// should not be performed if server behaves correctly
	plugin_timeout_add(geany_plugin, 4000, kill_cb, s);
}


static void stop_and_free_server(LspServer *s)
{
	if (s->pid)
		stop_process(s);
	else
		free_server(s);
}


static gchar *get_autocomplete_trigger_chars(GVariant *node)
{
	GVariantIter *iter = NULL;
	GString *str = g_string_new("");

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"completionProvider", "{",
				"triggerCharacters", JSONRPC_MESSAGE_GET_ITER(&iter),
			"}",
		"}");

	if (iter)
	{
		GVariant *val = NULL;
		while (g_variant_iter_loop(iter, "v", &val))
			g_string_append(str, g_variant_get_string(val, NULL));
		g_variant_iter_free(iter);
	}

	return g_string_free(str, FALSE);
}


static guint64 get_semantic_token_mask(LspServer *srv, GVariant *node)
{
	guint64 mask = 0;
	guint64 index = 1;
	GVariantIter *iter = NULL;

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"semanticTokensProvider", "{",
				"legend", "{",
					"tokenTypes", JSONRPC_MESSAGE_GET_ITER(&iter),
				"}",
			"}",
		"}");

	if (iter && srv->config.semantic_tokens_types)
	{
		GVariant *val = NULL;
		while (g_variant_iter_loop(iter, "v", &val))
		{
			const gchar *str = g_variant_get_string(val, NULL);
			gchar **token_ptr;

			foreach_strv(token_ptr, srv->config.semantic_tokens_types)
			{
				if (g_strcmp0(str, *token_ptr) == 0)
				{
					mask |= index;
					break;
				}
			}

			index <<= 1;
		}
		g_variant_iter_free(iter);
	}

	return mask;
}


static gchar *get_signature_trigger_chars(GVariant *node)
{
	GVariantIter *iter = NULL;
	GVariantIter *iter2 = NULL;
	GString *str = g_string_new("");

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"signatureHelpProvider", "{",
				"triggerCharacters", JSONRPC_MESSAGE_GET_ITER(&iter),
			"}",
		"}");

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"signatureHelpProvider", "{",
				"retriggerCharacters", JSONRPC_MESSAGE_GET_ITER(&iter2),
			"}",
		"}");

	if (iter)
	{
		GVariant *val = NULL;
		while (g_variant_iter_loop(iter, "v", &val))
			g_string_append(str, g_variant_get_string(val, NULL));
		g_variant_iter_free(iter);
	}

	if (iter2)
	{
		GVariant *val = NULL;
		while (g_variant_iter_loop(iter2, "v", &val))
		{
			const gchar *chr = g_variant_get_string(val, NULL);
			if (!strstr(str->str, chr))
				g_string_append(str, chr);
		}
		g_variant_iter_free(iter2);
	}

	return g_string_free(str, FALSE);
}


static gboolean use_incremental_sync(GVariant *node)
{
	gint64 val;

	gboolean success = JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"textDocumentSync", "{",
				"change", JSONRPC_MESSAGE_GET_INT64(&val),
			"}",
		"}");

	if (!success)
	{
		success = JSONRPC_MESSAGE_PARSE(node,
			"capabilities", "{",
				"textDocumentSync", JSONRPC_MESSAGE_GET_INT64(&val),
			"}");
	}

	// not supporting "0", i.e. no sync - not sure if any server uses it and how
	// Geany could work with it
	return success && val == 2;
}


static gboolean use_workspace_folders(GVariant *node)
{
	gboolean change_notifications = FALSE;
	const gchar *notif_id = NULL;
	gboolean supported = FALSE;
	gboolean success;

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"workspace", "{",
				"workspaceFolders", "{",
					"supported", JSONRPC_MESSAGE_GET_BOOLEAN(&supported),
				"}",
			"}",
		"}");

	if (!supported)
		return FALSE;

	success = JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"workspace", "{",
				"workspaceFolders", "{",
					"changeNotifications", JSONRPC_MESSAGE_GET_BOOLEAN(&change_notifications),
				"}",
			"}",
		"}");

	if (!success)  // can also be string
	{
		JSONRPC_MESSAGE_PARSE(node,
			"capabilities", "{",
				"workspace", "{",
					"workspaceFolders", "{",
						"changeNotifications", JSONRPC_MESSAGE_GET_STRING(&notif_id),
					"}",
				"}",
			"}");
	}

	return change_notifications || notif_id;
}


static gboolean has_capability(GVariant *variant, const gchar *key1, const gchar *key2, const gchar *key3)
{
	gboolean val = FALSE;
	GVariant *var = NULL;
	gboolean success;

	if (key2 && key3)
		success = JSONRPC_MESSAGE_PARSE(variant,
			"capabilities", "{",
				key1, "{",
					key2, "{",
						key3, JSONRPC_MESSAGE_GET_BOOLEAN(&val),
					"}",
				"}",
			"}");
	else if (key2)
		success = JSONRPC_MESSAGE_PARSE(variant,
			"capabilities", "{",
				key1, "{",
					key2, JSONRPC_MESSAGE_GET_BOOLEAN(&val),
				"}",
			"}");
	else
		success = JSONRPC_MESSAGE_PARSE(variant,
			"capabilities", "{",
				key1, JSONRPC_MESSAGE_GET_BOOLEAN(&val),
			"}");

	// explicit TRUE, FALSE
	if (success)
		return val;

	// dict (possibly just empty) which also indicates TRUE
	if (key2 && key3)
		JSONRPC_MESSAGE_PARSE(variant,
			"capabilities", "{",
				key1, "{",
					key2, "{",
						key3, JSONRPC_MESSAGE_GET_VARIANT(&var),
					"}",
				"}",
			"}");
	else if (key2)
		JSONRPC_MESSAGE_PARSE(variant,
			"capabilities", "{",
				key1, "{",
					key2, JSONRPC_MESSAGE_GET_VARIANT(&var),
				"}",
			"}");
	else
		JSONRPC_MESSAGE_PARSE(variant,
			"capabilities", "{",
				key1, JSONRPC_MESSAGE_GET_VARIANT(&var),
			"}");

	if (var)
	{
		g_variant_unref(var);
		return TRUE;
	}

	return FALSE;
}


static void update_config(GVariant *variant, gboolean *option, const gchar *key)
{
	*option = *option && has_capability(variant, key, NULL, NULL);
}


static void send_did_change_configuration(LspServer *srv)
{
	JsonNode *settings = lsp_utils_parse_json_file(srv->config.initialization_options_file,
		srv->config.initialization_options);
	GVariant *res = g_variant_take_ref(json_gvariant_deserialize(settings, NULL, NULL));
	GVariant *msg = JSONRPC_MESSAGE_NEW( "settings", "{",
		JSONRPC_MESSAGE_PUT_VARIANT(res),
	"}");

	lsp_rpc_notify(srv, "workspace/didChangeConfiguration", msg, NULL, NULL);

	json_node_free(settings);
	g_variant_unref(res);
	g_variant_unref(msg);
}


static void initialize_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	LspServer *s = user_data;

	if (!error)
	{
		gboolean supports_semantic_token_range, supports_semantic_token_full;

		g_free(s->autocomplete_trigger_chars);
		s->autocomplete_trigger_chars = get_autocomplete_trigger_chars(return_value);

		g_free(s->signature_trigger_chars);
		s->signature_trigger_chars = get_signature_trigger_chars(return_value);
		if (EMPTY(s->signature_trigger_chars))
			s->config.signature_enable = FALSE;

		update_config(return_value, &s->config.autocomplete_enable, "completionProvider");
		update_config(return_value, &s->config.hover_enable, "hoverProvider");
		update_config(return_value, &s->config.hover_available, "hoverProvider");
		update_config(return_value, &s->config.goto_enable, "definitionProvider");
		update_config(return_value, &s->config.document_symbols_enable, "documentSymbolProvider");
		update_config(return_value, &s->config.document_symbols_available, "documentSymbolProvider");
		update_config(return_value, &s->config.highlighting_enable, "documentHighlightProvider");
		update_config(return_value, &s->config.code_lens_enable, "codeLensProvider");
		update_config(return_value, &s->config.goto_declaration_enable, "declarationProvider");
		update_config(return_value, &s->config.goto_definition_enable, "definitionProvider");
		update_config(return_value, &s->config.goto_implementation_enable, "implementationProvider");
		update_config(return_value, &s->config.goto_references_enable, "referencesProvider");
		update_config(return_value, &s->config.goto_type_definition_enable, "typeDefinitionProvider");
		update_config(return_value, &s->config.document_formatting_enable, "documentFormattingProvider");
		update_config(return_value, &s->config.range_formatting_enable, "documentRangeFormattingProvider");
		update_config(return_value, &s->config.execute_command_enable, "executeCommandProvider");
		update_config(return_value, &s->config.code_action_enable, "codeActionProvider");
		update_config(return_value, &s->config.rename_enable, "renameProvider");
		update_config(return_value, &s->config.selection_range_enable, "selectionRangeProvider");

		s->supports_completion_resolve = has_capability(return_value, "completionProvider", "resolveProvider", NULL);

		s->supports_workspace_symbols = TRUE;
		update_config(return_value, &s->supports_workspace_symbols, "workspaceSymbolProvider");

		s->use_incremental_sync = use_incremental_sync(return_value);
		s->send_did_save = has_capability(return_value, "textDocumentSync", "save", NULL);
		s->include_text_on_save = has_capability(return_value, "textDocumentSync", "save", "includeText");
		s->use_workspace_folders = use_workspace_folders(return_value);

		s->initialize_response = lsp_utils_json_pretty_print(return_value);

		supports_semantic_token_range = has_capability(return_value, "semanticTokensProvider", "range", NULL);
		supports_semantic_token_full = has_capability(return_value, "semanticTokensProvider", "full", NULL);
		s->config.semantic_tokens_supports_delta = has_capability(return_value,
			"semanticTokensProvider", "full", "delta");

		s->config.semantic_tokens_enable = s->config.semantic_tokens_enable &&
			(supports_semantic_token_full || supports_semantic_token_range);
		s->config.semantic_tokens_range_only = !supports_semantic_token_full &&
			supports_semantic_token_range;

		s->semantic_token_mask = get_semantic_token_mask(s, return_value);

		msgwin_status_add(_("LSP server %s initialized"), s->config.cmd);

		lsp_rpc_notify(s, "initialized", NULL, NULL, NULL);
		s->startup_shutdown = FALSE;

		lsp_semtokens_init(s->filetype);

		// Duplicate request to add project root to workspace folders - this
		// was already done in the initialize request but the pyright server
		// requires adding them dynamically - hopefully alright for other servers
		// too
		lsp_workspace_folders_add_project_root(s);

		// e.g. vscode-json-languageserver requires this instead of static
		// configuration during initialize
		if (s->config.send_did_change_configuration)
			send_did_change_configuration(s);

		if (lsp_server_initialized_cb)
			lsp_server_initialized_cb(s);
	}
	else
	{
		msgwin_status_add(_("Initialize request failed for LSP server %s: %s"), s->config.cmd, error->message);
		msgwin_status_add(_("Force terminating %s"), s->config.cmd);

		// force exit the server - since the handshake didn't perform, the
		// server may be in some strange state and normal "exit" may not work
		// (happens with haskell server)
		kill_server(s);
	}
}


static const gchar *get_trace_level(LspServer *srv)
{
	if (g_strcmp0(srv->config.trace_value, "messages") == 0 ||
		g_strcmp0(srv->config.trace_value, "verbose") == 0)
		return srv->config.trace_value;

	return "off";
}


static void perform_initialize(LspServer *server)
{
	GeanyDocument *doc = document_get_current();
	gchar *project_base = lsp_utils_get_project_base_path();
	GVariant *workspace_folders = NULL;
	GVariant *node, *capabilities, *info;
	gchar *project_base_uri = NULL;
	GVariantDict dct;

	if (!project_base && doc && server->config.project_root_marker_patterns)
		project_base = lsp_utils_find_project_root(doc, &server->config);

	if (!project_base && doc)
		project_base = g_path_get_dirname(doc->real_path);

	if (project_base)
		project_base_uri = g_filename_to_uri(project_base, NULL, NULL);

	capabilities = JSONRPC_MESSAGE_NEW(
		"window", "{",
			"workDoneProgress", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			"showDocument", "{",
				"support", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			"}",
		"}",
		"textDocument", "{",
			"synchronization", "{",
				"didSave", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			"}",
			"completion", "{",
				"completionItem", "{",
					"snippetSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(server->config.autocomplete_use_snippets),
					"documentationFormat", "[",
						"plaintext",
					"]",
				"}",
				"completionItemKind", "{",
					"valueSet", "[",
						LSP_COMPLETION_KINDS,
					"]",
				"}",
				"contextSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			"}",
			"hover", "{",
				"contentFormat", "[",
					"plaintext",
				"]",
			"}",
			"documentSymbol", "{",
				"symbolKind", "{",
					"valueSet", "[",
						LSP_SYMBOL_KINDS,
					"]",
				"}",
				"hierarchicalDocumentSymbolSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			"}",
			"publishDiagnostics", "{",  // zls requires this to publish diagnostics
			"}",
			"codeAction", "{",
				"resolveSupport", "{",
					"properties", "[",
						"edit", "command",
					"]",
				"}",
				"dataSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
				"codeActionLiteralSupport", "{",
					"codeActionKind", "{",
						"valueSet", "[",
							"",
							"quickfix",
							"refactor",
							"refactor.extract",
							"refactor.inline",
							"refactor.rewrite",
							"source",
							"source.organizeImports",
							"source.fixAll",
						"]",
					"}",
				"}",
			"}",
			"semanticTokens", "{",
				"requests", "{",
					"full", "{",
						"delta", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
					"}",
				"}",
				"tokenTypes", "[",
					// specify all possible token types - gopls returns incorrect offsets without it
					// TODO: investigate more and possibly report upstream
					"namespace",
					"type",
					"class",
					"enum",
					"interface",
					"struct",
					"typeParameter",
					"parameter",
					"variable",
					"property",
					"enumMember",
					"event",
					"function",
					"method",
					"macro",
					"keyword",
					"modifier",
					"comment",
					"string",
					"number",
					"regexp",
					"operator",
					"decorator",
				"]",
				"tokenModifiers", "[",
				"]",
				"formats", "[",
					"relative",
				"]",
				"augmentsSyntaxTokens", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			"}",
		"}",
		"workspace", "{",
			"applyEdit", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			"symbol", "{",
				"symbolKind", "{",
					"valueSet", "[",
						LSP_SYMBOL_KINDS,
					"]",
				"}",
			"}",
			"workspaceFolders", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			// possibly enable in the future - we have support for this
			//"configuration", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
		"}"
	);

	info = JSONRPC_MESSAGE_NEW(
		"clientInfo", "{",
			"name", JSONRPC_MESSAGE_PUT_STRING("Geany LSP Client Plugin"),
			"version", JSONRPC_MESSAGE_PUT_STRING(VERSION),
		"}",
		"processId", JSONRPC_MESSAGE_PUT_INT64(getpid()),
		"locale", JSONRPC_MESSAGE_PUT_STRING("en"),
		"trace", JSONRPC_MESSAGE_PUT_STRING(get_trace_level(server)),
		"rootPath", JSONRPC_MESSAGE_PUT_STRING(project_base),
		"rootUri", JSONRPC_MESSAGE_PUT_STRING(project_base_uri)
	);

	if (project_base)
	{
		workspace_folders = JSONRPC_MESSAGE_NEW_ARRAY(
			"{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(project_base_uri),
				"name", JSONRPC_MESSAGE_PUT_STRING(project_base),
			"}");
	}

	g_variant_dict_init(&dct, info);

	if (workspace_folders)
		g_variant_dict_insert_value(&dct, "workspaceFolders", workspace_folders);
	g_variant_dict_insert_value(&dct, "initializationOptions",
		lsp_utils_parse_json_file_as_variant(server->config.initialization_options_file, server->config.initialization_options));
	g_variant_dict_insert_value(&dct, "capabilities", capabilities);

	node = g_variant_take_ref(g_variant_dict_end(&dct));

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	msgwin_status_add(_("Sending initialize request to LSP server %s"), server->config.cmd);

	server->startup_shutdown = TRUE;
	lsp_rpc_call_startup_shutdown(server, "initialize", node, initialize_cb, server);

	g_free(project_base);
	g_free(project_base_uri);
	g_variant_unref(node);
	g_variant_unref(info);
	g_variant_unref(capabilities);
	if (workspace_folders)
		g_variant_unref(workspace_folders);
}


static GKeyFile *read_keyfile(const gchar *config_file)
{
	GError *error = NULL;
	GKeyFile *kf = g_key_file_new();

	if (!g_key_file_load_from_file(kf, config_file, G_KEY_FILE_NONE, &error))
	{
		msgwin_status_add(_("Failed to load LSP configuration file with message %s"), error->message);
		g_error_free(error);
	}

	return kf;
}


static void stderr_cb(GString *string, GIOCondition condition, gpointer data)
{
	LspServer *srv = data;

	if (srv->config.show_server_stderr)
		fprintf(stderr, "%s", string->str);
}


static void start_lsp_server(LspServer *server)
{
	GInputStream *input_stream;
	GOutputStream *output_stream;
	GError *error = NULL;
	gint stdin_fd = -1;
	gint stdout_fd = -1;
	gboolean success;
	GString *cmd = g_string_new(server->config.cmd);

#ifdef G_OS_UNIX
	// command itself
	if (g_str_has_prefix(cmd->str, "~/"))
		utils_string_replace_first(cmd, "~", g_get_home_dir());
	// arguments such as config files
	gchar *replacement = g_strconcat(" ", g_get_home_dir(), "/", NULL);
	utils_string_replace_all(cmd, " ~/", replacement);
	g_free(replacement);
#endif

	msgwin_status_add(_("Starting LSP server %s"), cmd->str);

	success = lsp_spawn_with_pipes_and_stderr_callback(NULL, cmd->str, NULL,
		server->config.env,
		&stdin_fd, &stdout_fd, stderr_cb, server, 0,
		process_stopped, server, &server->pid, &error);

	if (!success)
	{
		msgwin_status_add(_("LSP server process %s failed to start, giving up: %s"), cmd->str, error->message);
		server->restarts = 100;  // don't retry - probably missing executable
		g_error_free(error);
		g_string_free(cmd, TRUE);
		return;
	}

#ifdef G_OS_UNIX
	input_stream = g_unix_input_stream_new(stdout_fd, TRUE);
	output_stream = g_unix_output_stream_new(stdin_fd, TRUE);
#else
	// GWin32InputStream / GWin32OutputStream use windows handle-based file
	// API and we need fd-based API. Use our copy of unix input/output streams
	// on Windows
	input_stream = lsp_unix_input_stream_new(stdout_fd, TRUE);
	output_stream = lsp_unix_output_stream_new(stdin_fd, TRUE);
#endif
	server->stream = g_simple_io_stream_new(input_stream, output_stream);

	server->log = lsp_log_start(&server->config);
	server->rpc = lsp_rpc_new(server, server->stream);

	perform_initialize(server);
	g_string_free(cmd, TRUE);
}


static void get_bool(gboolean *dest, GKeyFile *kf, const gchar *section, const gchar *key)
{
	GError *error = NULL;
	gboolean bool_val = g_key_file_get_boolean(kf, section, key, &error);

	if (!error)
		*dest = bool_val;
	else
		g_error_free(error);
}


static void get_str(gchar **dest, GKeyFile *kf, const gchar *section, const gchar *key)
{
	gchar *str_val = g_key_file_get_string(kf, section, key, NULL);

	if (str_val)
	{
		g_strstrip(str_val);
		g_free(*dest);
		*dest = str_val;
	}
}


static void get_strv(gchar ***dest, GKeyFile *kf, const gchar *section, const gchar *key)
{
	gchar **strv_val = g_key_file_get_string_list(kf, section, key, NULL, NULL);

	if (strv_val)
	{
		g_strfreev(*dest);
		*dest = strv_val;
	}
}


static void get_int(gint *dest, GKeyFile *kf, const gchar *section, const gchar *key)
{
	GError *error = NULL;
	gint int_val = g_key_file_get_integer(kf, section, key, &error);

	if (!error)
		*dest = int_val;
	else
		g_error_free(error);
}


static void load_config(GKeyFile *kf, const gchar *section, LspServer *s)
{
	gint i;

	get_bool(&s->config.use_outside_project_dir, kf, section, "use_outside_project_dir");
	get_bool(&s->config.use_without_project, kf, section, "use_without_project");
	get_bool(&s->config.rpc_log_full, kf, section, "rpc_log_full");
	get_str(&s->config.word_chars, kf, section, "extra_identifier_characters");
	get_bool(&s->config.send_did_change_configuration, kf, section, "send_did_change_configuration");

	get_bool(&s->config.autocomplete_enable, kf, section, "autocomplete_enable");

	get_strv(&s->config.autocomplete_trigger_sequences, kf, section, "autocomplete_trigger_sequences");
	get_strv(&s->config.project_root_marker_patterns, kf, section, "project_root_marker_patterns");

	get_int(&s->config.autocomplete_window_max_entries, kf, section, "autocomplete_window_max_entries");
	get_int(&s->config.autocomplete_window_max_displayed, kf, section, "autocomplete_window_max_displayed");
	get_int(&s->config.autocomplete_window_max_width, kf, section, "autocomplete_window_max_width");
	get_str(&s->config.autocomplete_hide_after_words, kf, section, "autocomplete_hide_after_words");

	get_bool(&s->config.autocomplete_use_label, kf, section, "autocomplete_use_label");
	get_bool(&s->config.autocomplete_apply_additional_edits, kf, section, "autocomplete_apply_additional_edits");
	get_bool(&s->config.diagnostics_enable, kf, section, "diagnostics_enable");
	get_bool(&s->config.autocomplete_use_snippets, kf, section, "autocomplete_use_snippets");
	get_bool(&s->config.autocomplete_in_strings, kf, section, "autocomplete_in_strings");
	get_bool(&s->config.autocomplete_show_documentation, kf, section, "autocomplete_show_documentation");
	get_int(&s->config.diagnostics_statusbar_severity, kf, section, "diagnostics_statusbar_severity");
	get_str(&s->config.diagnostics_disable_for, kf, section, "diagnostics_disable_for");

	get_str(&s->config.diagnostics_error_style, kf, section, "diagnostics_error_style");
	get_str(&s->config.diagnostics_warning_style, kf, section, "diagnostics_warning_style");
	get_str(&s->config.diagnostics_info_style, kf, section, "diagnostics_info_style");
	get_str(&s->config.diagnostics_hint_style, kf, section, "diagnostics_hint_style");

	get_bool(&s->config.hover_enable, kf, section, "hover_enable");
	get_int(&s->config.hover_popup_max_lines, kf, section, "hover_popup_max_lines");
	get_int(&s->config.hover_popup_max_paragraphs, kf, section, "hover_popup_max_paragraphs");
	get_bool(&s->config.signature_enable, kf, section, "signature_enable");
	get_bool(&s->config.goto_enable, kf, section, "goto_enable");
	get_bool(&s->config.show_server_stderr, kf, section, "show_server_stderr");

	get_bool(&s->config.document_symbols_enable, kf, section, "document_symbols_enable");

	get_bool(&s->config.semantic_tokens_enable, kf, section, "semantic_tokens_enable");
	get_bool(&s->config.semantic_tokens_force_full, kf, section, "semantic_tokens_force_full");
	get_strv(&s->config.semantic_tokens_types, kf, section, "semantic_tokens_types");
	get_int(&s->config.semantic_tokens_lexer_kw_index, kf, section, "semantic_tokens_lexer_kw_index");
	get_str(&s->config.semantic_tokens_type_style, kf, section, "semantic_tokens_type_style");

	get_bool(&s->config.highlighting_enable, kf, section, "highlighting_enable");
	get_str(&s->config.highlighting_style, kf, section, "highlighting_style");

	get_bool(&s->config.code_lens_enable, kf, section, "code_lens_enable");
	get_str(&s->config.code_lens_style, kf, section, "code_lens_style");

	get_str(&s->config.formatting_options_file, kf, section, "formatting_options_file");
	get_str(&s->config.formatting_options, kf, section, "formatting_options");

	get_bool(&s->config.format_on_save, kf, section, "format_on_save");
	get_str(&s->config.command_on_save_regex, kf, section, "command_on_save_regex");

	get_bool(&s->config.progress_bar_enable, kf, section, "progress_bar_enable");
	get_bool(&s->config.swap_header_source_enable, kf, section, "swap_header_source_enable");

	get_str(&s->config.trace_value, kf, section, "trace_value");
	get_bool(&s->config.enable_telemetry, kf, section, "telemetry_notifications");

	// create for the first time, then just update
	if (!s->config.command_regexes)
		s->config.command_regexes = g_ptr_array_new_full(s->config.command_keybinding_num, g_free);

	g_ptr_array_set_size(s->config.command_regexes, s->config.command_keybinding_num);

	for (i = 0; i < s->config.command_keybinding_num; i++)
	{
		gchar *key = g_strdup_printf("command_%d_regex", i + 1);

		get_str((gchar **)&s->config.command_regexes->pdata[i], kf, section, key);
		if (!s->config.command_regexes->pdata[i])
			s->config.command_regexes->pdata[i] = g_strdup("");

		g_free(key);
	}

	s->config.goto_declaration_enable = TRUE;
	s->config.goto_definition_enable = TRUE;
	s->config.goto_implementation_enable = TRUE;
	s->config.goto_references_enable = TRUE;
	s->config.goto_type_definition_enable = TRUE;
	s->config.document_formatting_enable = TRUE;
	s->config.range_formatting_enable = TRUE;
	s->config.execute_command_enable = TRUE;
	s->config.code_action_enable = TRUE;
	s->config.rename_enable = TRUE;
	s->config.selection_range_enable = TRUE;

	s->config.hover_available = TRUE;
	s->config.document_symbols_available = TRUE;
}


static void load_all_section_only_config(GKeyFile *kf, const gchar *section, LspServer *s)
{
	get_bool(&s->config.enable_by_default, kf, section, "enable_by_default");

	get_int(&s->config.command_keybinding_num, kf, section, "command_keybinding_num");
	s->config.command_keybinding_num = CLAMP(s->config.command_keybinding_num, 1, 1000);

	get_str(&s->config.document_symbols_tab_label, kf, section, "document_symbols_tab_label");
}


static void load_filetype_only_config(GKeyFile *kf, const gchar *section, LspServer *s)
{
	gchar *cmd = NULL;
	gchar *use = NULL;

	get_str(&cmd, kf, section, "cmd");
	get_str(&use, kf, section, "use");
	if (!EMPTY(cmd) || !EMPTY(use))
	{
		// make sure 'use' from global config file gets overridden by 'cmd' from user config file
		// and that not both of them are set
		SETPTR(s->config.cmd, cmd);
		SETPTR(s->config.ref_lang, use);
	}

	get_strv(&s->config.env, kf, section, "env");
	get_str(&s->config.rpc_log, kf, section, "rpc_log");
	get_str(&s->config.initialization_options_file, kf, section, "initialization_options_file");
	get_str(&s->config.initialization_options, kf, section, "initialization_options");
	get_strv(&s->config.lang_id_mappings, kf, section, "lang_id_mappings");
}


static LspServer *server_get_or_start_for_ft(GeanyFiletype *ft, gboolean launch_server)
{
	LspServer *s, *s2 = NULL;

	if (!ft || !lsp_servers || lsp_utils_is_lsp_disabled_for_project())
		return NULL;

	s = lsp_servers->pdata[ft->id];
	if (s->referenced)
		s = s->referenced;

	if (s->startup_shutdown)
		return NULL;

	if (s->pid)
		return s;

	if (s->not_used)
		return NULL;

	if (is_dead(s))
		return NULL;

	if (!launch_server)
		return NULL;

	if (s->config.ref_lang)
	{
		GeanyFiletype *ref_ft = filetypes_lookup_by_name(s->config.ref_lang);

		if (ref_ft)
		{
			s2 = g_ptr_array_index(lsp_servers, ref_ft->id);
			s->referenced = s2;
			if (s2->pid)
				return s2;
		}
	}

	if (s2)
		s = s2;

	if (s->config.cmd)
		g_strstrip(s->config.cmd);
	if (EMPTY(s->config.cmd))
	{
		g_free(s->config.cmd);
		s->config.cmd = NULL;
		s->not_used = TRUE;
	}
	else
	{
		start_lsp_server(s);
	}

	// the server isn't initialized when running for the first time because the async
	// handshake with the server hasn't completed yet
	return NULL;
}


LspServer *lsp_server_get_for_ft(GeanyFiletype *ft)
{
	return server_get_or_start_for_ft(ft, TRUE);
}


static gboolean is_lsp_valid_for_doc(LspServerConfig *cfg, GeanyDocument *doc)
{
	gchar *base_path, *real_path, *rel_path;
	gboolean inside_project;

	if (!doc || !doc->real_path)
		return FALSE;

	if (EMPTY(cfg->cmd))
		return FALSE;

	if (cfg->project_root_marker_patterns)
	{
		gchar *project_root = lsp_utils_find_project_root(doc, cfg);
		if (project_root)
		{
			g_free(project_root);
			return TRUE;
		}
		g_free(project_root);
	}

	if (!cfg->use_without_project && !geany_data->app->project)
		return FALSE;

	if (cfg->use_outside_project_dir || !geany_data->app->project)
		return TRUE;

	base_path = lsp_utils_get_project_base_path();
	real_path = utils_get_utf8_from_locale(doc->real_path);
	rel_path = lsp_utils_get_relative_path(base_path, real_path);

	inside_project = rel_path && !g_str_has_prefix(rel_path, "..");

	g_free(rel_path);
	g_free(real_path);
	g_free(base_path);

	return inside_project;
}


#if ! GLIB_CHECK_VERSION(2, 70, 0)
#define g_pattern_spec_match_string g_pattern_match_string
#endif

GeanyFiletype *lsp_server_get_ft(GeanyDocument *doc, gchar **lsp_lang_id)
{
	LspServer *srv;
	guint i;

	if (!lsp_servers || (!doc->real_path || doc->file_type->id != GEANY_FILETYPES_NONE))
	{
		if (lsp_lang_id)
			*lsp_lang_id = lsp_utils_get_lsp_lang_id(doc);
		return doc->file_type;
	}

	foreach_ptr_array(srv, i, lsp_servers)
	{
		gint j = 0;
		gchar **val;
		gchar *lang_id = NULL;

		if (!srv->config.lang_id_mappings || EMPTY(srv->config.cmd))
			continue;

		foreach_strv(val, srv->config.lang_id_mappings)
		{
			if (j % 2 == 0)
				lang_id = *val;
			else
			{
				GPatternSpec *spec = g_pattern_spec_new(*val);
				gchar *fname = g_path_get_basename(doc->file_name);
				GeanyFiletype *ret = NULL;

				if (g_pattern_spec_match_string(spec, fname))
					ret = filetypes_index(i);

				g_pattern_spec_free(spec);
				g_free(fname);

				if (ret)
				{
					if (lsp_lang_id)
						*lsp_lang_id = g_strdup(lang_id);
					return ret;
				}
			}

			j++;
		}
	}

	if (lsp_lang_id)
		*lsp_lang_id = lsp_utils_get_lsp_lang_id(doc);

	return doc->file_type;
}


static LspServer *server_get_configured_for_doc(GeanyDocument *doc)
{
	GeanyFiletype *ft;
	LspServer *s;

	if (!doc || !lsp_servers || lsp_utils_is_lsp_disabled_for_project())
		return NULL;

	ft = lsp_server_get_ft(doc, NULL);
	s = lsp_servers->pdata[ft->id];

	if (s->config.ref_lang)
	{
		ft = filetypes_lookup_by_name(s->config.ref_lang);

		if (ft)
			s = lsp_servers->pdata[ft->id];
		else
			return NULL;
	}

	if (!s)
		return NULL;

	if (!is_lsp_valid_for_doc(&s->config, doc))
		return NULL;

	return s;
}


static LspServer *server_get_for_doc(GeanyDocument *doc, gboolean launch_server)
{
	GeanyFiletype *ft;

	if (server_get_configured_for_doc(doc) == NULL)
		return NULL;

	ft = lsp_server_get_ft(doc, NULL);
	return server_get_or_start_for_ft(ft, launch_server);
}


LspServer *lsp_server_get(GeanyDocument *doc)
{
	return server_get_for_doc(doc, TRUE);
}


LspServer *lsp_server_get_if_running(GeanyDocument *doc)
{
	return server_get_for_doc(doc, FALSE);
}


LspServerConfig *lsp_server_get_all_section_config(void)
{
	// hack - the assumption is that nobody will ever configure anything for
	// ABC so its settings are identical to the [all] section
	GeanyFiletype *ft = filetypes_index(GEANY_FILETYPES_ABC);
	LspServer *s = lsp_servers->pdata[ft->id];

	return &s->config;
}


gboolean lsp_server_is_usable(GeanyDocument *doc)
{
	LspServer *s = server_get_configured_for_doc(doc);

	if (!s)
		return FALSE;

	return !s->not_used && !is_dead(s);
}


void lsp_server_stop_all(gboolean wait)
{
	GPtrArray *lsp_servers_tmp = lsp_servers;

	// set to NULL now - inside free functions of g_ptr_array_free() we need
	// to check if lsp_servers is NULL in this case
	lsp_servers = NULL;
	if (lsp_servers_tmp)
		g_ptr_array_free(lsp_servers_tmp, TRUE);

	if (wait)
	{
		GMainContext *main_context = g_main_context_ref_thread_default();

		// this runs the main loop and blocks - otherwise gio won't return async results
		while (servers_in_shutdown->len > 0)
			g_main_context_iteration(main_context, TRUE);

		g_main_context_unref(main_context);
	}
}


static LspServer *lsp_server_new(GKeyFile *kf_global, GKeyFile *kf, GeanyFiletype *ft)
{
	LspServer *s = g_new0(LspServer, 1);
	GString *wc = g_string_new(GEANY_WORDCHARS);
	guint i, word_chars_len;

	s->filetype = ft->id;

	load_all_section_only_config(kf_global, "all", s);
	load_all_section_only_config(kf, "all", s);

	load_config(kf_global, "all", s);
	load_config(kf, "all", s);

	load_config(kf_global, ft->name, s);
	load_config(kf, ft->name, s);

	load_filetype_only_config(kf_global, ft->name, s);
	load_filetype_only_config(kf, ft->name, s);

	word_chars_len = s->config.word_chars ? strlen(s->config.word_chars) : 0;
	for (i = 0; i < word_chars_len; i++)
	{
		gchar c = s->config.word_chars[i];
		if (!strchr(wc->str, c))
			g_string_append_c(wc, c);
	}
	g_free(s->config.word_chars);
	s->config.word_chars = g_string_free(wc, FALSE);

	lsp_sync_init(s);
	lsp_diagnostics_init(s);
	lsp_workspace_folders_init(s);

	return s;
}


static LspServer *lsp_server_init(gint ft)
{
	GKeyFile *kf_global = read_keyfile(lsp_utils_get_global_config_filename());
	GKeyFile *kf = read_keyfile(lsp_utils_get_config_filename());
	GeanyFiletype *filetype = filetypes_index(ft);
	LspServer *s = lsp_server_new(kf_global, kf, filetype);

	g_key_file_free(kf);
	g_key_file_free(kf_global);

	return s;
}


void lsp_server_init_all(void)
{
	GKeyFile *kf_global = read_keyfile(lsp_utils_get_global_config_filename());
	GKeyFile *kf = read_keyfile(lsp_utils_get_config_filename());
	GeanyFiletype *ft;
	guint i;

	if (lsp_servers)
		lsp_server_stop_all(FALSE);

	if (!servers_in_shutdown)
		servers_in_shutdown = g_ptr_array_new_full(0, (GDestroyNotify)free_server);

	lsp_servers = g_ptr_array_new_full(0, (GDestroyNotify)stop_and_free_server);

	for (i = 0; (ft = filetypes_index(i)); i++)
	{
		LspServer *s = lsp_server_new(kf_global, kf, ft);
		g_ptr_array_add(lsp_servers, s);
	}

	g_key_file_free(kf);
	g_key_file_free(kf_global);
}


gboolean lsp_server_uses_init_file(gchar *path)
{
	guint i;

	if (!lsp_servers)
		return FALSE;

	for (i = 0; i < lsp_servers->len; i++)
	{
		LspServer *s = lsp_servers->pdata[i];

		if (s->config.initialization_options_file)
		{
			gboolean found = FALSE;
			gchar *p1 = utils_get_real_path(path);
			gchar *p2 = utils_get_real_path(s->config.initialization_options_file);

			found = g_strcmp0(p1, p2) == 0;

			g_free(p1);
			g_free(p2);

			if (found)
				return TRUE;
		}
	}

	return FALSE;
}


gchar *lsp_server_get_initialize_responses(void)
{
	gboolean first = TRUE;
	GString *str;
	guint i;

	if (!lsp_servers)
		return FALSE;

	str = g_string_new("{");

	for (i = 0; i < lsp_servers->len; i++)
	{
		LspServer *s = lsp_servers->pdata[i];

		if (s->config.cmd && s->initialize_response)
		{
			if (!first)
				g_string_append(str, "\n\n\"############################################################\": \"next server\",");
			first = FALSE;
			g_string_append(str, "\n\n\"");
			g_string_append(str, s->config.cmd);
			g_string_append(str, "\":\n");
			g_string_append(str, s->initialize_response);
			g_string_append_c(str, ',');
		}
	}
	if (g_str_has_suffix(str->str, ","))
		g_string_erase(str, str->len-1, 1);
	g_string_append(str, "\n}");

	return g_string_free(str, FALSE);
}


void lsp_server_set_initialized_cb(LspServerInitializedCallback cb)
{
	lsp_server_initialized_cb = cb;
}
