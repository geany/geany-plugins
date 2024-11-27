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

#include "lsp-highlight.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"

#include <jsonrpc-glib.h>

#define HIGHLIGHT_DIRTY "lsp_highlight_dirty"


typedef struct {
	GeanyDocument *doc;
	gint pos;
	gchar *identifier;
	gboolean highlight;
} LspHighlightData;


extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;

static gint indicator;
static gint64 last_request_time;
static gint request_source;


void lsp_highlight_clear(GeanyDocument *doc)
{
	gboolean dirty = GPOINTER_TO_UINT(plugin_get_document_data(geany_plugin, doc, HIGHLIGHT_DIRTY));
	if (dirty)
	{
		ScintillaObject *sci = doc->editor->sci;

		if (indicator > 0)
			sci_indicator_set(sci, indicator);
		sci_indicator_clear(sci, 0, sci_get_length(sci));
		plugin_set_document_data(geany_plugin, doc, HIGHLIGHT_DIRTY, GUINT_TO_POINTER(FALSE));
	}
}


void lsp_highlight_style_init(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	ScintillaObject *sci;

	if (!srv)
		return;

	sci = doc->editor->sci;

	if (indicator > 0)
	{
		plugin_set_document_data(geany_plugin, doc, HIGHLIGHT_DIRTY, GUINT_TO_POINTER(TRUE));
		lsp_highlight_clear(doc);
	}
	indicator = lsp_utils_set_indicator_style(sci, srv->config.highlighting_style);
	if (indicator > 0)
		SSM(sci, SCI_INDICSETUNDER, indicator, TRUE);
}


static void highlight_range(GeanyDocument *doc, LspRange range)
{
	ScintillaObject *sci = doc->editor->sci;
	gint start_pos = lsp_utils_lsp_pos_to_scintilla(sci, range.start);
	gint end_pos = lsp_utils_lsp_pos_to_scintilla(sci, range.end);

	if (indicator > 0)
		editor_indicator_set_on_range(doc->editor, indicator, start_pos, end_pos);
	plugin_set_document_data(geany_plugin, doc, HIGHLIGHT_DIRTY, GUINT_TO_POINTER(TRUE));
}


static void highlight_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	LspHighlightData *data = user_data;

	if (!error)
	{
		GeanyDocument *doc = document_get_current();

		if (doc == data->doc)
			lsp_highlight_clear(doc);

		if (doc == data->doc && g_variant_is_of_type(return_value, G_VARIANT_TYPE_ARRAY))
		{
			GVariant *member = NULL;
			GVariantIter iter;
			gint sel_id = 0;
			gint main_sel_id = 0;
			gboolean first_sel = TRUE;

			//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

			g_variant_iter_init(&iter, return_value);

			while (g_variant_iter_loop(&iter, "v", &member))
			{
				GVariant *range = NULL;

				JSONRPC_MESSAGE_PARSE(member,
					"range", JSONRPC_MESSAGE_GET_VARIANT(&range)
					);

				if (range)
				{
					LspRange r = lsp_utils_parse_range(range);
					gint start_pos = lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, r.start);
					gint end_pos = lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, r.end);
					gchar *ident = sci_get_contents_range(doc->editor->sci, start_pos, end_pos);

					//clangd returns highlight for 'editor' in 'doc-|>editor' where
					//'|' is the caret position and also other cases, which is strange
					//restrict to identifiers only
					if (g_strcmp0(ident, data->identifier) == 0)
					{
						if (data->highlight)
							highlight_range(doc, r);
						else
						{
							SSM(doc->editor->sci, first_sel ? SCI_SETSELECTION : SCI_ADDSELECTION,
								start_pos, end_pos);
							if (data->pos >= start_pos && data->pos <= end_pos)
								main_sel_id = sel_id;

							first_sel = FALSE;
							sel_id++;
						}
					}

					g_free(ident);
					g_variant_unref(range);
				}
			}

			if (!data->highlight)
				SSM(doc->editor->sci, SCI_SETMAINSELECTION, main_sel_id, 0);
		}
	}

	g_free(data->identifier);
	g_free(user_data);
}


static void send_request(LspServer *server, GeanyDocument *doc, gint pos, gboolean highlight)
{
	GVariant *node;
	ScintillaObject *sci = doc->editor->sci;
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);
	gchar *iden = lsp_utils_get_current_iden(doc, pos, server->config.word_chars);
	gchar *selection = sci_get_selection_contents(sci);
	gboolean valid_rename;

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

	valid_rename = (!sci_has_selection(sci) && iden) ||
		(sci_has_selection(sci) && g_strcmp0(iden, selection) == 0);

	if ((highlight && !sci_has_selection(sci) && iden) || (!highlight && valid_rename))
	{
		LspHighlightData *data = g_new0(LspHighlightData, 1);

		data->doc = doc;
		data->pos = pos;
		data->identifier = g_strdup(iden);
		data->highlight = highlight;
		lsp_rpc_call(server, "textDocument/documentHighlight", node,
			highlight_cb, data);
		last_request_time = g_get_monotonic_time();
	}
	else
		lsp_highlight_clear(doc);

	g_free(selection);
	g_free(iden);
	g_free(doc_uri);
	g_variant_unref(node);
}


static gboolean request_idle(gpointer data)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv;
	gint pos;

	request_source = 0;

	srv = lsp_server_get_if_running(doc);
	if (!srv)
		return G_SOURCE_REMOVE;

	pos = sci_get_current_position(doc->editor->sci);

	send_request(srv, doc, pos, TRUE);

	return G_SOURCE_REMOVE;
}


void lsp_highlight_schedule_request(GeanyDocument *doc)
{
	gint pos = sci_get_current_position(doc->editor->sci);
	LspServer *srv = lsp_server_get_if_running(doc);
	gchar *iden;

	if (!srv)
		return;

	iden = lsp_utils_get_current_iden(doc, pos, srv->config.word_chars);
	if (!iden)
	{
		lsp_highlight_clear(doc);
		// cancel request because we have an up-to-date information there's nothing
		// to highlight
		if (request_source != 0)
			g_source_remove(request_source);
		request_source = 0;
		return;
	}
	g_free(iden);

	if (request_source != 0)
		g_source_remove(request_source);
	request_source = 0;

	if (last_request_time == 0 || g_get_monotonic_time() > last_request_time + 300000)
		request_idle(NULL);
	else
		request_source = plugin_timeout_add(geany_plugin, 300, request_idle, NULL);
}


void lsp_highlight_rename(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!srv || !doc->real_path)
		return;

	send_request(srv, doc, pos, FALSE);
}
