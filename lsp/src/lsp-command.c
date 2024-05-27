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


static GPtrArray *code_actions;


void lsp_command_free(LspCommand *cmd)
{
	g_free(cmd->title);
	g_free(cmd->command);
	g_variant_unref(cmd->arguments);
	g_free(cmd);
}


void lsp_command_send_code_action_destroy(void)
{
	if (code_actions)
		g_ptr_array_free(code_actions, TRUE);
	code_actions = NULL;
}


void lsp_command_send_code_action_init(void)
{
	lsp_command_send_code_action_destroy();
	code_actions = g_ptr_array_new_full(0, (GDestroyNotify)lsp_command_free);
}


GPtrArray *lsp_command_get_resolved_code_actions(void)
{
	return code_actions;
}


static void command_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));
	}
}


void lsp_command_send_request(LspServer *server, const gchar *cmd, GVariant *arguments)
{
	GVariant *node;

	if (arguments)
	{
		GVariantDict dict;

		g_variant_dict_init(&dict, NULL);
		g_variant_dict_insert_value(&dict, "command", g_variant_new_string(cmd));
		g_variant_dict_insert_value(&dict, "arguments", arguments);
		node = g_variant_take_ref(g_variant_dict_end(&dict));
	}
	else
	{
		node = JSONRPC_MESSAGE_NEW (
			"command", JSONRPC_MESSAGE_PUT_STRING(cmd)
		);
	}

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_rpc_call(server, "workspace/executeCommand", node,
		command_cb, NULL);

	g_variant_unref(node);
}


static void code_action_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error && g_variant_is_of_type(return_value, G_VARIANT_TYPE_ARRAY))
	{
		GCallback callback = user_data;
		GVariant *code_action = NULL;
		GVariantIter iter;

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		g_variant_iter_init(&iter, return_value);

		while (g_variant_iter_loop(&iter, "v", &code_action))
		{
			const gchar *title = NULL;
			const gchar *command = NULL;
			GVariant *arguments = NULL;
			LspCommand *cmd;

			if (!JSONRPC_MESSAGE_PARSE(code_action,
					"title", JSONRPC_MESSAGE_GET_STRING(&title),
					"command", JSONRPC_MESSAGE_GET_STRING(&command)))
			{
				continue;
			}

			JSONRPC_MESSAGE_PARSE (code_action,
				"arguments", JSONRPC_MESSAGE_GET_VARIANT(&arguments)
			);

			cmd = g_new0(LspCommand, 1);
			cmd->title = g_strdup(title);
			cmd->command = g_strdup(command);
			cmd->arguments = arguments;

			g_ptr_array_add(code_actions, cmd);
		}

		callback();
	}
}


void lsp_command_send_code_action_request(gint pos, GCallback actions_resolved_cb)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get_if_running(doc);
	GVariant *diag_raw = lsp_diagnostics_get_diag_raw(pos);
	GVariant *node, *diagnostics, *diags_dict;
	ScintillaObject *sci;
	LspPosition lsp_pos;
	GVariantDict dict;
	GPtrArray *arr;
	gchar *doc_uri;

	lsp_command_send_code_action_init();

	if (!srv || !diag_raw)
	{
		actions_resolved_cb();
		return;
	}

	sci = doc->editor->sci;
	lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);

	doc_uri = lsp_utils_get_doc_uri(doc);
	arr = g_ptr_array_new_full(1, (GDestroyNotify) g_variant_unref);

	// TODO: works with current position only, support selection range in the future
	g_ptr_array_add(arr, g_variant_ref(diag_raw));
	diagnostics = g_variant_new_array(G_VARIANT_TYPE_VARDICT,
		(GVariant **)(gpointer)arr->pdata, arr->len);

	g_variant_dict_init(&dict, NULL);
	g_variant_dict_insert_value(&dict, "diagnostics", diagnostics);
	diags_dict = g_variant_take_ref(g_variant_dict_end(&dict));

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"range", "{",
			"start", "{",
				"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
				"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
			"}",
			"end", "{",
				"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
				"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
			"}",
		"}",
		"context", "{",
			JSONRPC_MESSAGE_PUT_VARIANT(diags_dict),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_rpc_call(srv, "textDocument/codeAction", node, code_action_cb,
		actions_resolved_cb);

	g_variant_unref(node);
	g_variant_unref(diags_dict);
	g_free(doc_uri);
}
