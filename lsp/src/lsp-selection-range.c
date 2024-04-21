/*
 * Copyright 2024 Jiri Techet <techet@gmail.com>
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

#include "lsp-selection-range.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"

#include <jsonrpc-glib.h>


extern GeanyData *geany_data;

typedef struct {
	GeanyDocument *doc;
	gboolean expand;
} SelectionRangeData;


GPtrArray *selections = NULL;


static gboolean is_within_range(ScintillaObject *sci, LspRange parent, LspRange child)
{
	gint parent_start_pos = lsp_utils_lsp_pos_to_scintilla(sci, parent.start);
	gint parent_end_pos = lsp_utils_lsp_pos_to_scintilla(sci, parent.end);
	gint child_start_pos = lsp_utils_lsp_pos_to_scintilla(sci, child.start);
	gint child_end_pos = lsp_utils_lsp_pos_to_scintilla(sci, child.end);

	return (parent_start_pos < child_start_pos && parent_end_pos >= child_end_pos) ||
		(parent_start_pos <= child_start_pos && parent_end_pos > child_end_pos);
}


static LspRange get_current_selection(ScintillaObject *sci)
{
	LspRange selection;
	selection.start = lsp_utils_scintilla_pos_to_lsp(sci, sci_get_selection_start(sci));
	selection.end = lsp_utils_scintilla_pos_to_lsp(sci, sci_get_selection_end(sci));
	return selection;
}


static gboolean is_max_selection(ScintillaObject *sci)
{
	LspRange selection = get_current_selection(sci);
	LspRange *max_selection;

	if (!selections || selections->len == 0)
		return FALSE;

	max_selection = selections->pdata[selections->len-1];

	return max_selection->start.character == selection.start.character &&
		max_selection->start.line == selection.start.line &&
		max_selection->end.character == selection.end.character &&
		max_selection->end.line == selection.end.line;
}


static void parse_selection(GVariant *val, ScintillaObject *sci, LspRange selection)
{
	GVariant *range_variant = NULL;
	GVariant *parent = NULL;

	JSONRPC_MESSAGE_PARSE(val,
		"range", JSONRPC_MESSAGE_GET_VARIANT(&range_variant));

	if (range_variant)
	{
		LspRange parsed_range = lsp_utils_parse_range(range_variant);

		if (is_within_range(sci, parsed_range, selection))
		{
			LspRange *range = g_new0(LspRange, 1);
			*range = parsed_range;
			g_ptr_array_add(selections, range);
		}

		g_variant_unref(range_variant);
	}

	JSONRPC_MESSAGE_PARSE(val,
		"parent", JSONRPC_MESSAGE_GET_VARIANT(&parent));

	if (parent)
	{
		parse_selection(parent, sci, selection);
		g_variant_unref(parent);
	}
}


static LspRange *find_selection_range(ScintillaObject *sci, gboolean expand)
{
	LspRange selection_range = get_current_selection(sci);;
	LspRange *found_range = NULL;
	LspRange *range;
	gint i;

	// sorted from the smallest to the biggest
	foreach_ptr_array(range, i, selections)
	{
		if (expand && is_within_range(sci, *range, selection_range))
		{
			found_range = range;
			break;
		}
		else if (!expand && is_within_range(sci, selection_range, *range))
			found_range = range;
	}

	return found_range;
}


static void find_and_select(ScintillaObject *sci, gboolean expand)
{
	LspRange *found_range = find_selection_range(sci, expand);

	if (found_range)
	{
		gint start = lsp_utils_lsp_pos_to_scintilla(sci, found_range->start);
		gint end = lsp_utils_lsp_pos_to_scintilla(sci, found_range->end);
		SSM(sci, SCI_SETSELECTION, start, end);
	}
}


static void goto_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	SelectionRangeData *data = user_data;

	if (!error)
	{
		GeanyDocument *doc = data->doc;

		if (DOC_VALID(doc) && g_variant_is_of_type(return_value, G_VARIANT_TYPE_ARRAY))
		{
			GVariant *val = NULL;
			GVariantIter iter;

			g_variant_iter_init(&iter, return_value);

			while (g_variant_iter_loop(&iter, "v", &val))
			{
				LspRange selection = get_current_selection(doc->editor->sci);
				LspRange *existing_range = g_new0(LspRange, 1);

				*existing_range = selection;
				g_ptr_array_add(selections, existing_range);

				parse_selection(val, doc->editor->sci, selection);
				break;  // for single query just a single result
			}

			find_and_select(doc->editor->sci, data->expand);
		}

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));
	}

	g_free(user_data);
}


void lsp_selection_clear_selections(void)
{
	if (selections)
		g_ptr_array_free(selections, TRUE);
	selections = NULL;
}


static void selection_range_request(gboolean expand)
{
	GeanyDocument *doc = document_get_current();
	LspServer *server = lsp_server_get(doc);
	SelectionRangeData *data;
	LspPosition lsp_pos;
	gchar *doc_uri;
	GVariant *node;
	gint pos;

	if (!server || !server->config.selection_range_enable)
		return;

	if (expand && is_max_selection(doc->editor->sci))
		return;
	else if (sci_has_selection(doc->editor->sci) && selections &&
			find_selection_range(doc->editor->sci, expand))
	{
		find_and_select(doc->editor->sci, expand);
		return;
	}

	pos = sci_get_current_position(doc->editor->sci);
	lsp_pos = lsp_utils_scintilla_pos_to_lsp(doc->editor->sci, pos);
	doc_uri = lsp_utils_get_doc_uri(doc);

	lsp_selection_clear_selections();
	selections = g_ptr_array_new_full(1, g_free);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"positions", "[", "{",  // TODO: support multiple ranges for multiple selections
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}", "]"
	);

	data = g_new0(SelectionRangeData, 1);
	data->doc = doc;
	data->expand = expand;
	lsp_rpc_call(server, "textDocument/selectionRange", node, goto_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_selection_range_expand(void)
{
	selection_range_request(TRUE);
}


void lsp_selection_range_shrink(void)
{
	selection_range_request(FALSE);
}
