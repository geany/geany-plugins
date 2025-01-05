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

#include "lsp-hover.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"

#include <jsonrpc-glib.h>


typedef struct {
	GeanyDocument *doc;
	gint pos;
} LspHoverData;


static ScintillaObject *calltip_sci;


static void show_calltip(GeanyDocument *doc, gint pos, const gchar *calltip)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	gchar *s, *p;
	gboolean quit = FALSE;
	gboolean start = TRUE;
	gint paragraph_no = 0;
	guint i;

	if (!srv)
		return;

	s = g_strdup(calltip);
	p = s;

	lsp_utils_wrap_string(s, -1);
	for (i = 0; p && !quit && i < srv->config.hover_popup_max_lines; i++)
	{
		gchar *q;

		if (!start)
			p++;
		start = FALSE;

		q = strchr(p, '\n');

		if (q)
		{
			gchar *line;

			*q = '\0';
			line = g_strdup(p);
			g_strstrip(line);
			if (line[0] == '\0')
				paragraph_no++;
			if (paragraph_no == srv->config.hover_popup_max_paragraphs)
				quit = TRUE;
			*q = '\n';

			g_free(line);
		}

		p = q;
	}

	if (p)
	{
		*p = '\0';
		g_strstrip(s);
		SETPTR(s, g_strconcat(s, "\n...", NULL));
	}

	calltip_sci = doc->editor->sci;
	SSM(calltip_sci, SCI_CALLTIPSHOW, pos, (sptr_t) s);
	g_free(s);
}


static void hover_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		GeanyDocument *doc = document_get_current();
		LspHoverData *data = user_data;

		if (doc == data->doc && gtk_widget_has_focus(GTK_WIDGET(doc->editor->sci)))
		{
			const gchar *str = NULL;

			JSONRPC_MESSAGE_PARSE(return_value, 
				"contents", "{",
					"value", JSONRPC_MESSAGE_GET_STRING(&str),
				"}");

			//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

			if (str && strlen(str) > 0)
				show_calltip(doc, data->pos, str);
		}
	}

	g_free(user_data);
}


void lsp_hover_send_request(LspServer *server, GeanyDocument *doc, gint pos)
{
	GVariant *node;
	ScintillaObject *sci = doc->editor->sci;
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);
	LspHoverData *data = g_new0(LspHoverData, 1);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"position", "{",
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	data->doc = doc;
	data->pos = pos;

	lsp_rpc_call(server, "textDocument/hover", node,
		hover_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_hover_hide_calltip(GeanyDocument *doc)
{
	if (doc->editor->sci == calltip_sci)
	{
		SSM(doc->editor->sci, SCI_CALLTIPCANCEL, 0, 0);
		calltip_sci = NULL;
	}
}
