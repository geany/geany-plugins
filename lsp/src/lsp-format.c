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

#include "lsp-format.h"
#include "lsp-server.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"

#include <jsonrpc-glib.h>


typedef struct {
	GeanyDocument *doc;
	LspCallback callback;
	gpointer user_data;
} FormatData;


static void format_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	FormatData *data = user_data;
	GeanyDocument *doc = data->doc;

	if (!error && DOC_VALID(doc) && g_variant_is_of_type(return_value, G_VARIANT_TYPE_ARRAY))
	{
		GPtrArray *edits;
		GVariantIter iter;

		g_variant_iter_init(&iter, return_value);
		edits = lsp_utils_parse_text_edits(&iter);

		sci_start_undo_action(doc->editor->sci);
		lsp_utils_apply_text_edits(doc->editor->sci, NULL, edits, FALSE);
		sci_end_undo_action(doc->editor->sci);

		g_ptr_array_free(edits, TRUE);

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));
	}

	if (data->callback)
		data->callback(data->user_data);

	g_free(data);
}


void lsp_format_perform(GeanyDocument *doc, gboolean force_whole_doc, LspCallback callback, gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);
	ScintillaObject *sci;
	const gchar *method;
	GVariant *node = NULL;
	gchar *doc_uri;
	FormatData *data;
	GVariant *options;

	if (!srv)
		return;

	sci = doc->editor->sci;
	doc_uri = lsp_utils_get_doc_uri(doc);

	options = lsp_utils_parse_json_file_as_variant(srv->config.formatting_options_file, srv->config.formatting_options);

	if ((sci_has_selection(sci) || !srv->config.document_formatting_enable) &&
		srv->config.range_formatting_enable)
	{
		LspRange range;
		gint sel_start;
		gint sel_end;

		if (!sci_has_selection(sci) || force_whole_doc)
		{
			sel_start = 0;
			sel_end = sci_get_length(sci);
		}
		else
		{
			sel_start = sci_get_selection_start(sci);
			sel_end = sci_get_selection_end(sci);
		}

		range.start = lsp_utils_scintilla_pos_to_lsp(sci, sel_start);
		range.end = lsp_utils_scintilla_pos_to_lsp(sci, sel_end);

		node = JSONRPC_MESSAGE_NEW (
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}",
			"range", "{",
				"start", "{",
					"line", JSONRPC_MESSAGE_PUT_INT32(range.start.line),
					"character", JSONRPC_MESSAGE_PUT_INT32(range.start.character),
				"}",
				"end", "{",
					"line", JSONRPC_MESSAGE_PUT_INT32(range.end.line),
					"character", JSONRPC_MESSAGE_PUT_INT32(range.end.character),
				"}",
			"}",
			"options", "{",
				JSONRPC_MESSAGE_PUT_VARIANT(options),
			"}"
		);
		method = "textDocument/rangeFormatting";
	}
	else if (srv->config.document_formatting_enable)
	{
		node = JSONRPC_MESSAGE_NEW (
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}",
			"options", "{",
				JSONRPC_MESSAGE_PUT_VARIANT(options),
			"}"
		);
		method = "textDocument/formatting";
	}

	if (node)
	{
		//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

		data = g_new0(FormatData, 1);
		data->doc = doc;
		data->callback = callback;
		data->user_data = user_data;

		lsp_rpc_call(srv, method, node, format_cb, data);

		g_variant_unref(node);
	}

	g_free(doc_uri);
}
