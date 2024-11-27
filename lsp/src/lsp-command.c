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

#include "lsp-command.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"
#include "lsp-diagnostics.h"

#include <jsonrpc-glib.h>


typedef struct
{
	LspCallback callback;
	gpointer user_data;
	GeanyDocument *doc;
} CommandData;


typedef struct
{
	CodeActionCallback callback;
	gpointer user_data;
} CodeActionData;


void lsp_command_free(LspCommand *cmd)
{
	g_free(cmd->title);
	g_free(cmd->command);
	if (cmd->arguments)
		g_variant_unref(cmd->arguments);
	if (cmd->edit)
		g_variant_unref(cmd->edit);
	if (cmd->data)
		g_variant_unref(cmd->data);
	g_free(cmd);
}


static LspCommand *parse_code_action(GVariant *code_action)
{
	const gchar *title = NULL;
	const gchar *command = NULL;
	GVariant *arguments = NULL;
	GVariant *edit = NULL;
	GVariant *data = NULL;
	gboolean is_command;
	LspCommand *cmd;

	// Can either be Command or CodeAction:
	//   Command {title: string; command: string; arguments?: LSPAny[];}
	//   CodeAction {title: string; edit?: WorkspaceEdit; command?: Command; data?: LSPAny[];}

	JSONRPC_MESSAGE_PARSE(code_action,
		"title", JSONRPC_MESSAGE_GET_STRING(&title)
	);

	if (!title)
		return NULL;

	is_command = JSONRPC_MESSAGE_PARSE(code_action,
		"command", JSONRPC_MESSAGE_GET_STRING(&command)
	);

	if (is_command)
	{
		JSONRPC_MESSAGE_PARSE(code_action,
			"arguments", JSONRPC_MESSAGE_GET_VARIANT(&arguments)
		);
	}
	else
	{
		JSONRPC_MESSAGE_PARSE(code_action,
			"command", "{",
				"command", JSONRPC_MESSAGE_GET_STRING(&command),
			"}"
		);

		JSONRPC_MESSAGE_PARSE(code_action,
			"command", "{",
				"arguments", JSONRPC_MESSAGE_GET_VARIANT(&arguments),
			"}"
		);

		JSONRPC_MESSAGE_PARSE(code_action,
			"edit", JSONRPC_MESSAGE_GET_VARIANT(&edit)
		);

		JSONRPC_MESSAGE_PARSE(code_action,
			"data", JSONRPC_MESSAGE_GET_VARIANT(&data)
		);
	}

	cmd = g_new0(LspCommand, 1);
	cmd->title = g_strdup(title);
	cmd->command = g_strdup(command);
	cmd->arguments = arguments;
	cmd->edit = edit;
	cmd->data = data;

	return cmd;
}


static void resolve_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	CommandData *data = user_data;
	gboolean performed = FALSE;

	if (!error && data->doc == document_get_current())
	{
		LspServer *server = lsp_server_get_if_running(data->doc);

		if (server)
		{
			LspCommand *cmd = parse_code_action(return_value);

			if (cmd && (cmd->command || cmd->edit))
			{
				lsp_command_perform(server, cmd, data->callback, data->user_data);
				performed = TRUE;
			}
		}

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));
	}

	if (!performed)
		data->callback(data->user_data);

	g_free(data);
}


static void resolve_code_action(LspServer *server, LspCommand *cmd, LspCallback callback, gpointer user_data)
{
	GVariant *node;
	CommandData *data;

	if (cmd->data)
	{
		GVariantDict dict;

		g_variant_dict_init(&dict, NULL);
		g_variant_dict_insert_value(&dict, "title", g_variant_new_string(cmd->title));
		g_variant_dict_insert_value(&dict, "data", cmd->data);
		node = g_variant_take_ref(g_variant_dict_end(&dict));
	}
	else
	{
		node = JSONRPC_MESSAGE_NEW(
			"title", JSONRPC_MESSAGE_PUT_STRING(cmd->title)
		);
	}

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	data = g_new0(CommandData, 1);
	data->callback = callback;
	data->user_data = user_data;
	data->doc = document_get_current();
	lsp_rpc_call(server, "codeAction/resolve", node, resolve_cb, data);

	g_variant_unref(node);
}


static void command_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	CommandData *data = user_data;

	if (data->callback)
		data->callback(data->user_data);
	g_free(data);
}


void lsp_command_perform(LspServer *server, LspCommand *cmd, LspCallback callback, gpointer user_data)
{
	if (!cmd->command && !cmd->edit)
	{
		resolve_code_action(server, cmd, callback, user_data);
		return;
	}

	if (cmd->edit)
		lsp_utils_apply_workspace_edit(cmd->edit);

	if (cmd->command)
	{
		GVariant *node;
		CommandData *data;

		if (cmd->arguments)
		{
			GVariantDict dict;

			g_variant_dict_init(&dict, NULL);
			g_variant_dict_insert_value(&dict, "command", g_variant_new_string(cmd->command));
			g_variant_dict_insert_value(&dict, "arguments", cmd->arguments);
			node = g_variant_take_ref(g_variant_dict_end(&dict));
		}
		else
		{
			node = JSONRPC_MESSAGE_NEW(
				"command", JSONRPC_MESSAGE_PUT_STRING(cmd->command)
			);
		}

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

		data = g_new0(CommandData, 1);
		data->callback = callback;
		data->user_data = user_data;
		lsp_rpc_call(server, "workspace/executeCommand", node,
			command_cb, data);

		g_variant_unref(node);
	}
	else if (callback)
		callback(user_data);  // this was just an edit without command execution
}


static void code_action_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	GPtrArray *code_actions = g_ptr_array_new_full(1, (GDestroyNotify)lsp_command_free);
	CodeActionData *data = user_data;

	if (!error)
	{
		if (g_variant_is_of_type(return_value, G_VARIANT_TYPE_ARRAY))
		{
			GVariant *code_action = NULL;
			GVariantIter iter;

			//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

			g_variant_iter_init(&iter, return_value);

			while (g_variant_iter_loop(&iter, "v", &code_action))
			{
				LspCommand *cmd = parse_code_action(code_action);

				if (cmd)
					g_ptr_array_add(code_actions, cmd);
			}
		}
	}

	if (data->callback(code_actions, data->user_data))
		g_ptr_array_free(code_actions, TRUE);

	g_free(data);
}


void lsp_command_send_code_action_request(GeanyDocument *doc, gint pos, CodeActionCallback actions_resolved_cb, gpointer user_data)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	GVariant *diag_raw = lsp_diagnostics_get_diag_raw(pos);
	GVariant *node, *diagnostics, *diags_dict;
	LspPosition lsp_pos_start, lsp_pos_end;
	gint pos_start, pos_end;
	ScintillaObject *sci;
	GVariantDict dict;
	GPtrArray *arr;
	gchar *doc_uri;
	CodeActionData *data;

	if (!srv)
	{
		GPtrArray *empty = g_ptr_array_new_full(0, (GDestroyNotify)lsp_command_free);
		if (actions_resolved_cb(empty, user_data))
			g_ptr_array_free(empty, TRUE);
		return;
	}

	sci = doc->editor->sci;

	pos_start = sci_get_selection_start(sci);
	pos_end = sci_get_selection_end(sci);

	if (pos_start == pos_end)
		pos_start = pos_end = pos;

	lsp_pos_start = lsp_utils_scintilla_pos_to_lsp(sci, pos_start);
	lsp_pos_end = lsp_utils_scintilla_pos_to_lsp(sci, pos_end);

	arr = g_ptr_array_new_full(1, (GDestroyNotify) g_variant_unref);
	if (diag_raw)
		g_ptr_array_add(arr, g_variant_ref(diag_raw));
	diagnostics = g_variant_new_array(G_VARIANT_TYPE_VARDICT,
		(GVariant **)arr->pdata, arr->len);

	g_variant_dict_init(&dict, NULL);
	g_variant_dict_insert_value(&dict, "diagnostics", diagnostics);
	diags_dict = g_variant_take_ref(g_variant_dict_end(&dict));

	doc_uri = lsp_utils_get_doc_uri(doc);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"range", "{",
			"start", "{",
				"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos_start.line),
				"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos_start.character),
			"}",
			"end", "{",
				"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos_end.line),
				"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos_end.character),
			"}",
		"}",
		"context", "{",
			JSONRPC_MESSAGE_PUT_VARIANT(diags_dict),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	data = g_new0(CodeActionData, 1);
	data->user_data = user_data;
	data->callback = actions_resolved_cb;
	lsp_rpc_call(srv, "textDocument/codeAction", node, code_action_cb, data);

	g_variant_unref(node);
	g_variant_unref(diags_dict);
	g_free(doc_uri);
	g_ptr_array_free(arr, TRUE);
}
