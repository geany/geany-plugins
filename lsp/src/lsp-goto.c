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

#include "lsp-goto.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"
#include "lsp-goto-panel.h"
#include "lsp-symbol.h"

#include <jsonrpc-glib.h>


typedef struct {
	GeanyDocument *doc;
	gboolean show_in_msgwin;
} GotoData;


extern GeanyData *geany_data;


// TODO: free on plugin unload
GPtrArray *last_result;


static void goto_location(GeanyDocument *old_doc, LspLocation *loc)
{
	gchar *fname = lsp_utils_get_real_path_from_uri_locale(loc->uri);
	GeanyDocument *doc = NULL;

	if (fname)
		doc = document_open_file(fname, FALSE, NULL, NULL);

	if (doc)
		navqueue_goto_line(old_doc, doc, loc->range.start.line + 1);

	g_free(fname);
}


static void filter_symbols(const gchar *filter)
{
	GPtrArray *filtered;

	if (!last_result)
		return;

	filtered = lsp_goto_panel_filter(last_result, filter);
	lsp_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
}


static void show_in_msgwin(LspLocation *loc, GHashTable *sci_table)
{
	ScintillaObject *sci = NULL;
	gint lineno = loc->range.start.line;
	gchar *fname, *base_path;
	GeanyDocument *doc;
	gchar *line_str;

	fname = lsp_utils_get_real_path_from_uri_utf8(loc->uri);
	if (!fname)
		return;

	doc = document_find_by_filename(fname);
	base_path = lsp_utils_get_project_base_path();

	if (doc)
		sci = doc->editor->sci;
	else
	{
		if (sci_table)
			sci = g_hash_table_lookup(sci_table, fname);
		if (!sci)
		{
			sci = lsp_utils_new_sci_from_file(fname);
			if (sci && sci_table)
				g_hash_table_insert(sci_table, g_strdup(fname), g_object_ref_sink(sci));
		}
	}

	line_str = sci ? sci_get_line(sci, lineno) : g_strdup("");
	g_strstrip(line_str);

	if (base_path)
	{
		gchar *rel_path = lsp_utils_get_relative_path(base_path, fname);
		gchar *locale_base_path = utils_get_locale_from_utf8(base_path);

		if (rel_path && !g_str_has_prefix(rel_path, ".."))
			SETPTR(fname, g_strdup(rel_path));

		msgwin_set_messages_dir(locale_base_path);

		g_free(locale_base_path);
		g_free(rel_path);
	}
	msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s:%d:  %s", fname, lineno + 1, line_str);

	g_free(line_str);
	g_free(fname);
	g_free(base_path);
	if (!sci_table && !doc)
	{
		g_object_ref_sink(sci);
		g_object_unref(sci);
	}
}


static void goto_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		GotoData *data = user_data;

		if (DOC_VALID(data->doc))
		{
			if (data->show_in_msgwin)
			{
				msgwin_clear_tab(MSG_MESSAGE);
				msgwin_switch_tab(MSG_MESSAGE, TRUE);
			}

			// single location

			/* check G_VARIANT_TYPE_DICTIONARY ("a{?*}") before
			   G_VARIANT_TYPE_ARRAY ("a*") as dictionary is apparently a
			   subset of array :-( */
			if (g_variant_is_of_type(return_value, G_VARIANT_TYPE_DICTIONARY))
			{
				LspLocation *loc = lsp_utils_parse_location(return_value);

				if (loc)
				{
					if (data->show_in_msgwin)
						show_in_msgwin(loc, NULL);
					else
						goto_location(data->doc, loc);
				}

				lsp_utils_free_lsp_location(loc);
			}
			// array of locations
			else if (g_variant_is_of_type(return_value, G_VARIANT_TYPE_ARRAY))
			{
				GPtrArray *locations = NULL;
				GVariantIter iter;

				g_variant_iter_init(&iter, return_value);

				locations = lsp_utils_parse_locations(&iter);

				if (locations && locations->len > 0)
				{
					if (data->show_in_msgwin)
					{
						GHashTable *sci_table = g_hash_table_new_full(g_str_hash,
							g_str_equal, g_free, (GDestroyNotify)g_object_unref);
						LspLocation *loc;
						guint j;

						foreach_ptr_array(loc, j, locations)
						{
							show_in_msgwin(loc, sci_table);
						}

						g_hash_table_destroy(sci_table);
					}
					else if (locations->len == 1)
						goto_location(data->doc, locations->pdata[0]);
					else
					{
						LspLocation *loc;
						guint j;

						if (last_result)
							g_ptr_array_free(last_result, TRUE);

						last_result = g_ptr_array_new_full(0, (GDestroyNotify)lsp_symbol_unref);

						foreach_ptr_array(loc, j, locations)
						{
							gchar *file_name, *name;
							LspSymbol *sym;

							file_name = lsp_utils_get_real_path_from_uri_utf8(loc->uri);
							if (!file_name)
								continue;

							name = g_path_get_basename(file_name);

							sym = lsp_symbol_new(name, "", "", file_name, 0, 0, loc->range.start.line + 1, 0,
								TM_ICON_OTHER);

							g_ptr_array_add(last_result, sym);

							g_free(name);
							g_free(file_name);
						}

						lsp_goto_panel_show("", filter_symbols);
					}
				}

				g_ptr_array_free(locations, TRUE);
			}
		}

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));
	}

	g_free(user_data);
}


static void perform_goto(LspServer *server, GeanyDocument *doc, gint pos, const gchar *request,
	gboolean show_in_msgwin)
{
	GVariant *node;
	ScintillaObject *sci = doc->editor->sci;
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);
	GotoData *data = g_new0(GotoData, 1);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"position", "{",
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}",
		"context", "{",  // only for textDocument/references
			"includeDeclaration", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
		"}"
	);

	data->doc = doc;
	data->show_in_msgwin = show_in_msgwin;
	lsp_rpc_call(server, request, node, goto_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_goto_definition(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!doc || !srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/definition", FALSE);
}


void lsp_goto_declaration(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!doc || !srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/declaration", FALSE);
}


void lsp_goto_type_definition(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!doc || !srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/typeDefinition", FALSE);
}


void lsp_goto_implementations(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!doc || !srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/implementation", TRUE);
}


void lsp_goto_references(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!doc || !srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/references", TRUE);
}
