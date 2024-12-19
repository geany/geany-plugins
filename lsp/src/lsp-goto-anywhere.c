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

#include "lsp-goto-anywhere.h"
#include "lsp-goto-panel.h"
#include "lsp-symbols.h"
#include "lsp-utils.h"
#include "lsp-symbol.h"

#include <gtk/gtk.h>
#include <geanyplugin.h>


typedef struct
{
	GeanyDocument *doc;
	gchar *query;
} DocQueryData;


extern GeanyData *geany_data;


static void workspace_symbol_cb(GPtrArray *symbols, gpointer user_data)
{
	lsp_goto_panel_fill(symbols);
}


static void doc_symbol_cb(gpointer user_data)
{
	DocQueryData *data = user_data;
	GeanyDocument *doc = document_get_current();
	gchar *text = data->query;
	GPtrArray *symbols;
	GPtrArray *filtered;

	if (doc != data->doc)
		return;

	symbols = lsp_symbols_doc_get_cached(doc);

	filtered = lsp_goto_panel_filter(symbols, text[0] ? text + 1 : text);
	lsp_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
	g_free(data->query);
	g_free(data);
}


static void goto_line(GeanyDocument *doc, const gchar *line_str)
{
	GPtrArray *arr = g_ptr_array_new_full(0, (GDestroyNotify)lsp_symbol_unref);
	gint lineno = atoi(line_str);
	gint linenum = sci_get_line_count(doc->editor->sci);
	guint i;

	for (i = 0; i < 4; i++)
	{
		LspSymbol *sym;
		gchar *file_name = utils_get_utf8_from_locale(doc->real_path);
		TMIcon icon = TM_ICON_OTHER;
		const gchar *name = "";
		gint line = 0;

		switch (i)
		{
			case 0:
				/* For translators: Item in a list which, when selected, navigates
				 * to the line typed in the entry above the list */
				name = _("line typed above");
				if (lineno == 0)
					line = sci_get_current_line(doc->editor->sci) + 1;
				else if (lineno > linenum)
					line = linenum;
				else
					line = lineno;
				break;

			case 1:
				/* For translators: Item in a list which, when selected, navigates
				 * to the beginning of the current document */
				name = _("beginning");
				line = 1;
				break;

			case 2:
				/* For translators: Item in a list which, when selected, navigates
				 * to the middle of the current document */
				name = _("middle");
				line = linenum / 2;
				break;

			case 3:
				/* For translators: Item in a list which, when selected, navigates
				 * to the end of the current document */
				name = _("end");
				line = linenum;
				break;
		}

		sym = lsp_symbol_new(name, "", "", file_name, 0, 0, line, 0, icon);

		g_ptr_array_add(arr, sym);

		g_free(file_name);
	}

	lsp_goto_panel_fill(arr);

	g_ptr_array_free(arr, TRUE);
}


static void goto_file(const gchar *file_str)
{
	GPtrArray *arr = g_ptr_array_new_full(0, (GDestroyNotify)lsp_symbol_unref);
	GPtrArray *filtered;
	guint i;

	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];
		gchar *file_name, *name;
		LspSymbol *sym;

		if (!doc->real_path)
			continue;

		name = g_path_get_basename(doc->real_path);
		file_name = utils_get_utf8_from_locale(doc->real_path);
		sym = lsp_symbol_new(name, "", "", file_name, 0, 0, 0, 0, TM_ICON_OTHER);

		g_ptr_array_add(arr, sym);

		g_free(name);
		g_free(file_name);
	}

	filtered = lsp_goto_panel_filter(arr, file_str);
	lsp_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
	g_ptr_array_free(arr, TRUE);
}


static void goto_tm_symbol(const gchar *query, GPtrArray *tags, TMParserType lang)
{
	GPtrArray *converted = g_ptr_array_new_full(0, (GDestroyNotify)lsp_symbol_unref);
	GPtrArray *filtered;
	TMTag *tag;
	guint i;

	if (tags)
	{
		foreach_ptr_array(tag, i, tags)
		{
			if (tag->lang == lang && tag->type != tm_tag_local_var_t && tag->file)
			{
				gchar *file_name, *name;
				LspSymbol *sym;

				name = g_strdup(tag->name);
				file_name = utils_get_utf8_from_locale(tag->file->file_name);

				sym = lsp_symbol_new(name, "", "", file_name, 0, 0, tag->line, 0,
					lsp_symbol_kinds_get_symbol_icon(lsp_symbol_kinds_tm_to_lsp(tag->type)));

				g_ptr_array_add(converted, sym);

				g_free(name);
				g_free(file_name);
			}
		}
	}

	filtered = lsp_goto_panel_filter(converted, query);
	lsp_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
	g_ptr_array_free(converted, TRUE);
}


static void perform_lookup(const gchar *query)
{
	GeanyDocument *doc = document_get_current();
	const gchar *query_str = query ? query : "";
	LspServer *srv = lsp_server_get(doc);

	if (g_str_has_prefix(query_str, "#"))
	{
		if (srv && srv->supports_workspace_symbols)
			lsp_symbols_workspace_request(doc, query_str+1, workspace_symbol_cb, NULL);
		else if (doc)
			// TODO: possibly improve performance by binary searching the start and the end point
			goto_tm_symbol(query_str+1, geany_data->app->tm_workspace->tags_array, doc->file_type->lang);
	}
	else if (g_str_has_prefix(query_str, "@"))
	{
		if (srv && srv->config.document_symbols_available)
		{
			DocQueryData *data = g_new0(DocQueryData, 1);
			data->query = g_strdup(query_str);
			data->doc = doc;
			lsp_symbols_doc_request(doc, doc_symbol_cb, data);
		}
		else if (doc)
		{
			GPtrArray *tags = doc->tm_file ? doc->tm_file->tags_array : g_ptr_array_new();
			goto_tm_symbol(query_str+1, tags, doc->file_type->lang);
			if (!doc->tm_file)
				g_ptr_array_free(tags, TRUE);
		}
	}
	else if (g_str_has_prefix(query_str, ":"))
	{
		if (doc && doc->real_path)
			goto_line(doc, query_str+1);
	}
	else
		goto_file(query_str);
}


static void goto_panel_query(const gchar *query_type, gboolean prefill)
{
	GeanyDocument *doc = document_get_current();
	gchar *query = NULL;

	if (prefill && doc)
	{
		LspServer *srv = lsp_server_get_if_running(doc);
		gint pos = sci_get_current_position(doc->editor->sci);
		query = lsp_utils_get_current_iden(doc, pos, srv ? srv->config.word_chars : GEANY_WORDCHARS);
	}
	if (!query)
		query = g_strdup("");
	SETPTR(query, g_strconcat(query_type, query, NULL));

	lsp_goto_panel_show(query, perform_lookup);

	g_free(query);
}


void lsp_goto_anywhere_for_workspace(void)
{
	goto_panel_query("#", TRUE);
}


void lsp_goto_anywhere_for_doc(void)
{
	goto_panel_query("@", TRUE);
}


void lsp_goto_anywhere_for_line(void)
{
	goto_panel_query(":", FALSE);
}


void lsp_goto_anywhere_for_file(void)
{
	goto_panel_query("", FALSE);
}
