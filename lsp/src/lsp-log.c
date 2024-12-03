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

#include "lsp-log.h"
#include "lsp-utils.h"

#include <glib.h>


static void log_print(LspLogInfo log, const gchar *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	if (log.type == STDOUT_FILENO)
		vprintf(fmt, args);
	else if (log.type == STDERR_FILENO)
		vfprintf(stderr, fmt, args);
	else
		g_output_stream_vprintf(G_OUTPUT_STREAM(log.stream), NULL, NULL, NULL, fmt, args);

	va_end(args);
}


LspLogInfo lsp_log_start(LspServerConfig *config)
{
	LspLogInfo info = {0, TRUE, NULL};
	GFile *fp;

	if (!config->rpc_log)
		return info;

	info.full = config->rpc_log_full;

	if (g_strcmp0(config->rpc_log, "stdout") == 0)
		info.type = STDOUT_FILENO;
	else if (g_strcmp0(config->rpc_log, "stderr") == 0)
		info.type = STDERR_FILENO;
	else
	{
		fp = g_file_new_for_path(config->rpc_log);
		g_file_delete(fp, NULL, NULL);
		info.stream = g_file_create(fp, G_FILE_CREATE_NONE, NULL, NULL);

		if (!info.stream)
			msgwin_status_add(_("Failed to create log file: %s"), config->rpc_log);

		g_object_unref(fp);
	}

	if (info.full)
		log_print(info, "{\n");

	return info;
}


void lsp_log_stop(LspLogInfo log)
{
	if (log.type == 0 && !log.stream)
		return;

	if (log.full)
		log_print(log, "\n\n\"log end\": \"\"\n}\n");

	if (log.stream)
		g_output_stream_close(G_OUTPUT_STREAM(log.stream), NULL, NULL);
	log.stream = NULL;
	log.type = 0;
}


void lsp_log(LspLogInfo log, LspLogType type, const gchar *method, GVariant *params,
	GError *error, GDateTime *req_time)
{
	gchar *json_msg, *time_str;
	const gchar *title = "";
	GDateTime *time;
	gint time_str_len;
	gchar *delta_str = NULL;
	gchar *err_msg;

	if (log.type == 0 && !log.stream)
		return;

	err_msg = error ? g_strdup_printf("\n  ^-- %s", error->message) : g_strdup("");

	time = g_date_time_new_now_local();
	if (req_time)
	{
		GTimeSpan delta = g_date_time_difference(time, req_time);
		delta_str = g_strdup_printf(" (%ld ms)", delta / 1000);
	}
	else
		delta_str = g_strdup("");
	time_str = g_date_time_format(time, "\%H:\%M:\%S.\%f");
	time_str_len = strlen(time_str);
	if (time_str_len > 3)
		time_str[time_str_len-3] = '\0';
	g_date_time_unref(time);

	if (!method)
		method = "";

	switch (type)
	{
		case LspLogClientMessageSent:
			title = "C --> S  req:  ";
			break;
		case LspLogClientMessageReceived:
			title = "C <-- S  resp: ";
			break;
		case LspLogClientNotificationSent:
			title = "C --> S  notif:";
			break;
		case LspLogServerMessageSent:
			title = "C <-- S  req:  ";
			break;
		case LspLogServerMessageReceived:
			title = "C --> S  resp: ";
			break;
		case LspLogServerNotificationSent:
			title = "C <-- S  notif:";
			break;
	}

	if (log.full)
	{
		if (!params)
			json_msg = g_strdup("null");
		else
			json_msg = lsp_utils_json_pretty_print(params);

		log_print(log, "\n\n\"[%s] %s %s%s\":\n%s,\n", time_str, title, method, delta_str, json_msg);
		g_free(json_msg);
	}
	else
		log_print(log, "[%s] %s %s%s%s\n", time_str, title, method, delta_str, err_msg);

	g_free(time_str);
	g_free(err_msg);
	g_free(delta_str);
}
