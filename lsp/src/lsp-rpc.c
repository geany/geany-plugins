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


static void handle_notification(JsonrpcClient *client, gchar *method, GVariant *params,
	gpointer user_data)
{
	LspServer *srv = g_hash_table_lookup(client_table, client);

	if (!srv)
		return;

	lsp_log(srv->log, LspLogServerNotificationSent, method, params, NULL, NULL);

	if (g_strcmp0(method, "textDocument/publishDiagnostics") == 0)
		lsp_diagnostics_received(params);
	else if (g_strcmp0(method, "window/logMessage") == 0 ||
		g_strcmp0(method, "window/showMessage") == 0)
	{
		log_message(params);
	}
	else if (g_str_has_prefix(method, "$/"))
	{
		lsp_progress_process_notification(srv, params);
	}
	else
	{
		//printf("\n\nNOTIFICATION FROM SERVER: %s\n", method);
		//printf("params:\n%s\n\n\n", lsp_utils_json_pretty_print(params));
	}
}


static gboolean handle_call(JsonrpcClient *client, gchar* method, GVariant *id, GVariant *params,
	gpointer user_data)
{
	LspServer *srv = g_hash_table_lookup(client_table, client);
	gboolean ret = FALSE;
	GVariant *variant;
	JsonNode *node;

	if (!srv)
		return ret;

	node = json_from_string("{}", NULL);
	variant = json_gvariant_deserialize(node, NULL, NULL);

	lsp_log(srv->log, LspLogServerMessageSent, method, params, NULL, NULL);

	//printf("\n\nREQUEST FROM SERVER: %s\n", method);
	//printf("params:\n%s\n\n\n", lsp_utils_json_pretty_print(params));

	if (g_strcmp0(method, "window/workDoneProgress/create") == 0)
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

		jsonrpc_client_reply_async(client, id, NULL, NULL, NULL, NULL);
		ret = TRUE;
	}
	else if (g_strcmp0(method, "workspace/applyEdit") == 0)
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

		jsonrpc_client_reply_async(client, id, msg, NULL, NULL, NULL);

		g_variant_unref(msg);
		if (edit)
			g_variant_unref(edit);
		ret = TRUE;
	}

	lsp_log(srv->log, LspLogServerMessageReceived, method, variant, NULL, NULL);
	g_variant_unref(variant);
	json_node_free(node);

	return ret;
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

	if (!params)
	{
		params = JSONRPC_MESSAGE_NEW("gopls_bug_workarond",
			JSONRPC_MESSAGE_PUT_STRING("https://github.com/golang/go/issues/57459"));
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
