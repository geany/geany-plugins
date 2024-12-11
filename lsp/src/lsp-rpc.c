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

#include "lsp-rpc.h"
#include "lsp-server.h"
#include "lsp-diagnostics.h"
#include "lsp-progress.h"
#include "lsp-log.h"
#include "lsp-utils.h"
#include "lsp-workspace-folders.h"

#include <jsonrpc-glib.h>
#include <stdio.h>


typedef struct
{
	gchar *method_name;
	gpointer user_data;
	LspRpcCallback callback;
	GDateTime *req_time;
	gboolean cb_on_startup_shutdown;
} CallbackData;


struct LspRpc
{
	JsonrpcClient *client;
};


extern GeanyData *geany_data;

GHashTable *client_table;


static void log_message(GVariant *params)
{
	gint64 type;
	const gchar *msg;
	gboolean success;

	success = JSONRPC_MESSAGE_PARSE(params,
		"type", JSONRPC_MESSAGE_GET_INT64(&type),
		"message", JSONRPC_MESSAGE_GET_STRING(&msg));

	if (success)
	{
		const gchar *type_str;
		gchar *stripped_msg = g_strdup(msg);

		switch (type)
		{
			case 1:
				type_str = "Error";
				break;
			case 2:
				type_str = "Warning";
				break;
			case 3:
				type_str = "Info";
				break;
			case 4:
				type_str = "Log";
				break;
			default:
				type_str = "Debug";
				break;
		}

		g_strstrip(stripped_msg);
		msgwin_status_add("%s: %s", type_str, stripped_msg);
		g_free(stripped_msg);
	}
}


static void log_trace(GVariant *params)
{
	const gchar *msg, *verbose;
	gboolean success;

	success = JSONRPC_MESSAGE_PARSE(params,
		"message", JSONRPC_MESSAGE_GET_STRING(&msg));

	JSONRPC_MESSAGE_PARSE(params,
		"verbose", JSONRPC_MESSAGE_GET_STRING(&verbose));

	if (success)
	{
		gchar *stripped_msg = g_strdup(msg);
		g_strstrip(stripped_msg);

		if (verbose)
		{
			gchar *stripped_verbose = g_strdup(verbose);
			g_strstrip(stripped_verbose);
			printf("%s: %s", stripped_msg, stripped_verbose);
			g_free(stripped_verbose);
		}
		else
			printf("%s", stripped_msg);

		g_free(stripped_msg);
	}
}


static void handle_notification(JsonrpcClient *client, gchar *method, GVariant *params,
	gpointer user_data)
{
	LspServer *srv = g_hash_table_lookup(client_table, client);

	if (!srv)
		return;

	lsp_log(srv->log, LspLogServerNotificationSent, method, params, NULL, NULL);

	if (g_strcmp0(method, "textDocument/publishDiagnostics") == 0)
		lsp_diagnostics_received(srv, params);
	else if (g_strcmp0(method, "window/logMessage") == 0 ||
		g_strcmp0(method, "window/showMessage") == 0)
	{
		log_message(params);
	}
	else if (g_strcmp0(method, "telemetry/event") == 0)
	{
		if (srv->config.enable_telemetry)
		{
			gchar *s = lsp_utils_json_pretty_print(params);
			printf("%s\n", s);
			g_free(s);
		}
	}
	else if (g_strcmp0(method, "$/progress") == 0)
	{
		lsp_progress_process_notification(srv, params);
	}
	else if (g_strcmp0(method, "$/logTrace") == 0)
	{
		log_trace(params);
	}
	else
	{
		//printf("\n\nNOTIFICATION FROM SERVER: %s\n", method);
		//printf("params:\n%s\n\n\n", lsp_utils_json_pretty_print(params));
	}
}


static void reply_async(LspServer *srv, const gchar *method, JsonrpcClient *client,
	GVariant *id, GVariant *result)
{
	jsonrpc_client_reply_async(client, id, result, NULL, NULL, NULL);
	lsp_log(srv->log, LspLogServerMessageReceived, method, result, NULL, NULL);
}


static GVariant *create_progress(LspServer *srv, GVariant *params)
{
	gboolean have_token = FALSE;
	const gchar *token_str = NULL;
	gint64 token_int = 0;

	have_token = JSONRPC_MESSAGE_PARSE(params,
		"token", JSONRPC_MESSAGE_GET_STRING(&token_str)
	);
	if (!have_token)
	{
		have_token = JSONRPC_MESSAGE_PARSE(params,
			"token", JSONRPC_MESSAGE_GET_INT64(&token_int)
		);
	}

	if (srv && have_token)
	{
		LspProgressToken token = {token_int, (gchar *)token_str};
		lsp_progress_create(srv, token);
	}

	return NULL;
}


static GVariant *apply_edit(LspServer *srv, GVariant *params)
{
	GVariant *edit, *msg;
	gboolean success;

	success = JSONRPC_MESSAGE_PARSE(params,
		"edit", JSONRPC_MESSAGE_GET_VARIANT(&edit)
	);

	if (success)
		success = lsp_utils_apply_workspace_edit(edit);

	msg = JSONRPC_MESSAGE_NEW(
		"applied", JSONRPC_MESSAGE_PUT_BOOLEAN(success)
	);

	if (edit)
		g_variant_unref(edit);

	return msg;
}


static GVariant *workspace_configuration(LspServer *srv, GVariant *params)
{
	GVariantIter *iter = NULL;
	GVariant *res = NULL;

	JSONRPC_MESSAGE_PARSE(params,
		"items", JSONRPC_MESSAGE_GET_ITER(&iter)
	);

	if (iter)
	{
		JsonNode *settings = lsp_utils_parse_json_file(srv->config.initialization_options_file,
			srv->config.initialization_options);
		JsonNode *empty_elem = json_from_string("{}", NULL);
		JsonBuilder *builder = json_builder_new();
		GVariant *member = NULL;
		JsonNode *res_json = NULL;

		json_builder_begin_array(builder);

		while (g_variant_iter_loop(iter, "v", &member))
		{
			const gchar *section = NULL;
			gboolean added = FALSE;

			if (JSONRPC_MESSAGE_PARSE(member, "section", JSONRPC_MESSAGE_GET_STRING(&section)))
			{
				gchar *path = g_strconcat("$.", section, NULL);
				JsonNode *matched = json_path_query(path, settings, NULL);

				if (matched)
				{
					JsonArray *arr = json_node_get_array(matched);
					if (arr && json_array_get_length(arr) > 0)
					{
						json_builder_add_value(builder, json_array_dup_element(arr, 0));
						added = TRUE;
					}
					json_node_unref(matched);
				}

				g_free(path);
			}

			if (!added)
				json_builder_add_value(builder, json_node_ref(empty_elem));
		}

		json_builder_end_array(builder);
		res_json = json_builder_get_root(builder);
		res = g_variant_take_ref(json_gvariant_deserialize(res_json, NULL, NULL));

		g_variant_iter_free(iter);
		json_node_free(settings);
		json_node_free(res_json);
		g_object_unref(builder);
		json_node_unref(empty_elem);
	}

	return res;
}


static GVariant *show_document(LspServer *srv, GVariant *params)
{
	const gchar *uri = NULL;
	gboolean external = FALSE;
	gboolean success = FALSE;
	GVariant *msg;

	JSONRPC_MESSAGE_PARSE(params,
		"uri", JSONRPC_MESSAGE_GET_STRING(&uri)
	);

	JSONRPC_MESSAGE_PARSE(params,
		"external", JSONRPC_MESSAGE_GET_BOOLEAN(&external)
	);

	if (uri)
	{
		if (external || !g_str_has_prefix(uri, "file://"))
		{
			utils_open_browser(uri);
			success = TRUE;
		}
		else if (g_str_has_prefix(uri, "file://"))
		{
			gchar *fname = lsp_utils_get_real_path_from_uri_locale(uri);
			if (fname)
			{
				document_open_file(fname, FALSE, NULL, NULL);
				g_free(fname);
				success = TRUE;
			}
		}
	}

	msg = JSONRPC_MESSAGE_NEW(
		"success", JSONRPC_MESSAGE_PUT_BOOLEAN(success)
	);

	return msg;
}


static GVariant *workspace_folders(LspServer *srv, GVariant *params)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(geany_data->main_widgets->sidebar_notebook);
	gint num = gtk_notebook_get_n_pages(notebook);
	GPtrArray *folders = lsp_workspace_folders_get(srv);
	GVariant *msg = NULL;

	if (num > 1)  // non-single-open document variant
	{
		GPtrArray *arr = g_ptr_array_new_full(1, (GDestroyNotify) g_variant_unref);
		gchar *folder;
		guint i;

		foreach_ptr_array(folder, i, folders)
		{
			gchar *uri = g_filename_to_uri(folder, NULL, NULL);
			GVariant *folder_variant;

			folder_variant = JSONRPC_MESSAGE_NEW(
				"uri", JSONRPC_MESSAGE_PUT_STRING(uri),
				"name", JSONRPC_MESSAGE_PUT_STRING(folder)
			);
			g_ptr_array_add(arr, folder_variant);

			g_free(uri);
		}

		msg = g_variant_take_ref(g_variant_new_array(G_VARIANT_TYPE_VARDICT,
			(GVariant **)arr->pdata, arr->len));

		g_ptr_array_free(arr, TRUE);
	}

	g_ptr_array_free(folders, TRUE);

	return msg;
}


static gboolean handle_call(JsonrpcClient *client, gchar* method, GVariant *id, GVariant *params,
	gpointer user_data)
{
	LspServer *srv = g_hash_table_lookup(client_table, client);
	gboolean handled = FALSE;
	GVariant *msg = NULL;

	if (!srv)
		return FALSE;

	lsp_log(srv->log, LspLogServerMessageSent, method, params, NULL, NULL);

	//printf("\n\nREQUEST FROM SERVER: %s\n", method);
	//printf("params:\n%s\n\n\n", lsp_utils_json_pretty_print(params));

	if (g_strcmp0(method, "window/workDoneProgress/create") == 0)
	{
		msg = create_progress(srv, params);
		handled = TRUE;
	}
	else if (g_strcmp0(method, "workspace/applyEdit") == 0)
	{
		msg = apply_edit(srv, params);
		handled = TRUE;
	}
	else if (g_strcmp0(method, "workspace/configuration") == 0)
	{
		// we officially don't support this (not advertised in initialize's
		// workspace/configuration) but some servers ask for it anyway so do our
		// best in this case
		msg = workspace_configuration(srv, params);
		handled = TRUE;
	}
	else if (g_strcmp0(method, "client/registerCapability") == 0)
	{
		// not supported at all, HTML/CSS servers sending the request despite
		// no indication of support from the client. Just to suppress warnings
		// from the servers
		msg = NULL;
		handled = TRUE;
	}
	else if (g_strcmp0(method, "workspace/semanticTokens/refresh") == 0)
	{
		// not supported at all - Kate seems to do the same as some servers
		// require a successful reply
		msg = NULL;
		handled = TRUE;
	}
	else if (g_strcmp0(method, "window/showDocument") == 0)
	{
		msg = show_document(srv, params);
		handled = TRUE;
	}
	else if (g_strcmp0(method, "workspace/workspaceFolders") == 0)
	{
		msg = workspace_folders(srv, params);
		handled = TRUE;
	}

	if (handled)
	{
		reply_async(srv, method, client, id, msg);
		if (msg)
			g_variant_unref(msg);
		return TRUE;
	}
	else
	{
		GVariant *variant;
		JsonNode *node;

		node = json_from_string("{}", NULL);
		variant = json_gvariant_deserialize(node, NULL, NULL);
		lsp_log(srv->log, LspLogServerMessageReceived, method, variant, NULL, NULL);
		g_variant_unref(variant);
		json_node_free(node);
	}

	return FALSE;
}


static void call_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	JsonrpcClient *client = (JsonrpcClient *)source_object;
	LspServer *srv = g_hash_table_lookup(client_table, client);
	CallbackData *data = user_data;
	GVariant *return_value = NULL;
	GError *error = NULL;
	gboolean is_startup_shutdown = TRUE;

	jsonrpc_client_call_finish(client, res, &return_value, &error);

	if (srv)
	{
		lsp_log(srv->log, LspLogClientMessageReceived, data->method_name,
			return_value, error, data->req_time);
		is_startup_shutdown = srv->startup_shutdown;
	}

	if (data->callback && (!is_startup_shutdown || data->cb_on_startup_shutdown))
		data->callback(return_value, error, data->user_data);

	if (return_value)
		g_variant_unref(return_value);

	if (error)
		g_error_free(error);

	g_date_time_unref(data->req_time);
	g_free(data->method_name);
	g_free(data);
}


static void call_full(LspServer *srv, const gchar *method, GVariant *params,
	LspRpcCallback callback, gboolean cb_on_startup_shutdown, gpointer user_data)
{
	CallbackData *data = g_new0(CallbackData, 1);

	data->method_name = g_strdup(method);
	data->user_data = user_data;
	data->callback = callback;
	data->req_time = g_date_time_new_now_local();
	data->cb_on_startup_shutdown = cb_on_startup_shutdown;

	lsp_log(srv->log, LspLogClientMessageSent, method, params, NULL, NULL);

	jsonrpc_client_call_async(srv->rpc->client, method, params, NULL, call_cb, data);
}


void lsp_rpc_call(LspServer *srv, const gchar *method, GVariant *params,
	LspRpcCallback callback, gpointer user_data)
{
	call_full(srv, method, params, callback, FALSE, user_data);
}


void lsp_rpc_call_startup_shutdown(LspServer *srv, const gchar *method, GVariant *params,
	LspRpcCallback callback, gpointer user_data)
{
	call_full(srv, method, params, callback, TRUE, user_data);
}


static void notify_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	JsonrpcClient *client = (JsonrpcClient *)source_object;
	CallbackData *data = user_data;
	GVariant *return_value = NULL;
	GError *error = NULL;

	jsonrpc_client_send_notification_finish(client, res, &error);

	if (data->callback)
		data->callback(return_value, error, data->user_data);

	if (return_value)
		g_variant_unref(return_value);

	if (error)
		g_error_free(error);

	g_free(data);
}


void lsp_rpc_notify(LspServer *srv, const gchar *method, GVariant *params,
	LspRpcCallback callback, gpointer user_data)
{
	gboolean params_added = FALSE;
	CallbackData *data = g_new0(CallbackData, 1);

	data->user_data = user_data;
	data->callback = callback;

	lsp_log(srv->log, LspLogClientNotificationSent,
		method, params, NULL, NULL);

	/* Two hacks in one:
	 * 1. some servers (e.g. gopls) require that the params member is present
	 *    (jsonrpc-glib removes it when there are no parameters which is jsonrpc
	 *    compliant)
	 * 2. haskell-language-server or nil require that the "exit" notification
	 *    has no params member
	 */
	if (!params && g_strcmp0(method, "exit") != 0)
	{
		GVariantDict dict;

		g_variant_dict_init(&dict, NULL);
		params = g_variant_take_ref(g_variant_dict_end(&dict));

		params_added = TRUE;
	}

	jsonrpc_client_send_notification_async(srv->rpc->client, method, params, NULL, notify_cb, data);

	if (params_added)
		g_variant_unref(params);
}


LspRpc *lsp_rpc_new(LspServer *srv, GIOStream *stream)
{
	LspRpc *c = g_new0(LspRpc, 1);

	if (!client_table)
		client_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

	c->client = jsonrpc_client_new(stream);
	g_hash_table_insert(client_table, c->client, srv);
	g_signal_connect(c->client, "handle-call", G_CALLBACK(handle_call), NULL);
	g_signal_connect(c->client, "notification", G_CALLBACK(handle_notification), NULL);
	jsonrpc_client_start_listening(c->client);

	return c;
}


void lsp_rpc_destroy(LspRpc *rpc)
{
	g_hash_table_remove(client_table, rpc->client);
	jsonrpc_client_close(rpc->client, NULL, NULL);
	g_object_unref(rpc->client);
	g_free(rpc);
}
