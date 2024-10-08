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


typedef struct {
	GeanyDocument *doc;
	gint pos;
	gchar *identifier;
	gboolean highlight;
} LspHighlightData;


static gint indicator;
static gboolean dirty;


void lsp_highlight_clear(GeanyDocument *doc)
{
	if (dirty)
	{
		ScintillaObject *sci = doc->editor->sci;

		if (indicator > 0)
			sci_indicator_set(sci, indicator);
		sci_indicator_clear(sci, 0, sci_get_length(sci));
		dirty = FALSE;
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
		dirty = TRUE;
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
	dirty = TRUE;
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
	}
	else
		lsp_highlight_clear(doc);

	g_free(selection);
	g_free(iden);
	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_highlight_send_request(LspServer *server, GeanyDocument *doc)
{
	gint pos = sci_get_current_position(doc->editor->sci);

	if (!doc || !doc->real_path)
		return;

	send_request(server, doc, pos, TRUE);
}


void lsp_highlight_rename(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!srv || !doc->real_path)
		return;

	send_request(srv, doc, pos, FALSE);
}
