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

#include "lsp-progress.h"

#include <jsonrpc-glib.h>


typedef struct
{
	LspProgressToken token;
	gchar *title;
} LspProgress;


static gint progress_num = 0;


static void progress_free(LspProgress *p)
{
	g_free(p->token.token_str);
	g_free(p->title);
	g_free(p);
}


void lsp_progress_create(LspServer *server, LspProgressToken token)
{
	LspProgress *p;

	p = g_new0(LspProgress, 1);

	p->token.token_str = g_strdup(token.token_str);
	p->token.token_int = token.token_int;

	server->progress_ops = g_slist_prepend(server->progress_ops, p);
}


static gboolean token_equal(LspProgressToken t1, LspProgressToken t2)
{
	if (t1.token_str != NULL || t2.token_str != NULL)
		return g_strcmp0(t1.token_str, t2.token_str) == 0;
	return t1.token_int == t2.token_int;
}


static void progress_begin(LspServer *server, LspProgressToken token, const gchar *title, const gchar *message)
{
	GSList *node;

	foreach_slist(node, server->progress_ops)
	{
		LspProgress *p = node->data;
		if (token_equal(p->token, token))
		{
			p->title = g_strdup(title);
			ui_set_statusbar(FALSE, "%s: %s", p->title, message ? message : "");
			if (progress_num == 0)
			{
				if (server->config.progress_bar_enable)
					ui_progress_bar_start("");
			}
			progress_num++;
			break;
		}
	}
}


static void progress_report(LspServer *server, LspProgressToken token, const gchar *message)
{
	GSList *node;

	foreach_slist(node, server->progress_ops)
	{
		LspProgress *p = node->data;
		if (token_equal(p->token, token))
		{
			ui_set_statusbar(FALSE, "%s: %s", p->title, message ? message : "");
			break;
		}
	}
}


static void progress_end(LspServer *server, LspProgressToken token, const gchar *message)
{
	GSList *node;

	foreach_slist(node, server->progress_ops)
	{
		LspProgress *p = node->data;
		if (token_equal(p->token, token))
		{
			if (progress_num > 0)
				progress_num--;
			if (progress_num == 0)
				ui_progress_bar_stop();

			if (message)
				ui_set_statusbar(FALSE, "%s: %s", p->title, message ? message : "");
			else
				ui_set_statusbar(FALSE, "%s", "");

			server->progress_ops = g_slist_remove_link(server->progress_ops, node);
			g_slist_free_full(node, (GDestroyNotify)progress_free);
			break;
		}
	}
}


void lsp_progress_free_all(LspServer *server)
{
	guint len = g_slist_length(server->progress_ops);

	g_slist_free_full(server->progress_ops, (GDestroyNotify)progress_free);
	server->progress_ops = 0;
	progress_num = MAX(0, progress_num - len);
	if (progress_num == 0)
		ui_progress_bar_stop();
}


void lsp_progress_process_notification(LspServer *srv, GVariant *params)
{
	gboolean have_token = FALSE;
	gint64 token_int = 0;
	const gchar *token_str = NULL;
	const gchar *kind = NULL;
	const gchar *title = NULL;
	const gchar *message = NULL;
	gchar buf[50];

	have_token = JSONRPC_MESSAGE_PARSE(params,
		"token", JSONRPC_MESSAGE_GET_STRING(&token_str)
	);
	if (!have_token)
	{
		have_token = JSONRPC_MESSAGE_PARSE(params,
			"token", JSONRPC_MESSAGE_GET_INT64(&token_int)
		);
	}
	JSONRPC_MESSAGE_PARSE(params,
		"value", "{",
			"kind", JSONRPC_MESSAGE_GET_STRING(&kind),
		"}"
	);
	JSONRPC_MESSAGE_PARSE(params,
		"value", "{",
			"title", JSONRPC_MESSAGE_GET_STRING(&title),
		"}"
	);
	JSONRPC_MESSAGE_PARSE(params,
		"value", "{",
			"message", JSONRPC_MESSAGE_GET_STRING(&message),
		"}"
	);

	if (!message)
	{
		gint64 percentage;
		gboolean have_percentage = JSONRPC_MESSAGE_PARSE(params,
			"value", "{",
				"percentage", JSONRPC_MESSAGE_GET_INT64(&percentage),
			"}"
		);
		if (have_percentage)
		{
			g_snprintf(buf, 30, "%ld%%", percentage);
			message = buf;
		}
	}

	if (srv && have_token && kind)
	{
		LspProgressToken token = {token_int, (gchar *)token_str};
		if (g_strcmp0(kind, "begin") == 0)
			progress_begin(srv, token, title, message);
		else if (g_strcmp0(kind, "report") == 0)
			progress_report(srv, token, message);
		else if (g_strcmp0(kind, "end") == 0)
			progress_end(srv, token, message);
	}
}
