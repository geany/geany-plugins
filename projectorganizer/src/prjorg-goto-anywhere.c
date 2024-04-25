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

#include "prjorg-goto-anywhere.h"
#include "prjorg-goto-panel.h"
#include "prjorg-project.h"

#include <gtk/gtk.h>
#include <geanyplugin.h>


#define SSM(s, m, w, l) scintilla_send_message((s), (m), (w), (l))


typedef struct
{
	GeanyDocument *doc;
	gchar *query;
} DocQueryData;


extern GeanyData *geany_data;


static void goto_line(GeanyDocument *doc, const gchar *line_str)
{
	GPtrArray *arr = g_ptr_array_new_full(0, (GDestroyNotify)prjorg_goto_symbol_free);
	gint lineno = atoi(line_str);
	gint linenum = sci_get_line_count(doc->editor->sci);
	guint i;

	for (i = 0; i < 4; i++)
	{
		PrjorgGotoSymbol *sym = g_new0(PrjorgGotoSymbol, 1);

		sym->file_name = utils_get_utf8_from_locale(doc->real_path);
		sym->icon = TM_ICON_OTHER;

		switch (i)
		{
			case 0:
				/* For translators: Item in a list which, when selected, navigates
				 * to the line typed in the entry above the list */
				sym->name = g_strdup(_("line typed above"));
				if (lineno == 0)
					sym->line = sci_get_current_line(doc->editor->sci) + 1;
				else if (lineno > linenum)
					sym->line = linenum;
				else
					sym->line = lineno;
				break;

			case 1:
				/* For translators: Item in a list which, when selected, navigates
				 * to the beginning of the current document */
				sym->name = g_strdup(_("beginning"));
				sym->line = 1;
				break;

			case 2:
				/* For translators: Item in a list which, when selected, navigates
				 * to the middle of the current document */
				sym->name = g_strdup(_("middle"));
				sym->line = linenum / 2;
				break;

			case 3:
				/* For translators: Item in a list which, when selected, navigates
				 * to the end of the current document */
				sym->name = g_strdup(_("end"));
				sym->line = linenum;
				break;
		}

		g_ptr_array_add(arr, sym);
	}

	prjorg_goto_panel_fill(arr);

	g_ptr_array_free(arr, TRUE);
}


static void goto_file(const gchar *file_str)
{
	GPtrArray *arr = g_ptr_array_new_full(0, (GDestroyNotify)prjorg_goto_symbol_free);
	GHashTable *files_added = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	GPtrArray *filtered;
	guint i;

	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];
		PrjorgGotoSymbol *sym;

		if (!doc->real_path)
			continue;

		sym = g_new0(PrjorgGotoSymbol, 1);
		sym->file_name = utils_get_utf8_from_locale(doc->real_path);
		sym->name = g_path_get_basename(sym->file_name);
		sym->icon = TM_ICON_OTHER;
		g_ptr_array_add(arr, sym);

		g_hash_table_insert(files_added, g_strdup(sym->file_name), GINT_TO_POINTER(1));
	}

	if (prj_org && prj_org->roots)
	{
		PrjOrgRoot *root = prj_org->roots->data;
		GHashTableIter iter;
		gpointer key, value;

		g_hash_table_iter_init(&iter, root->file_table);
		while (g_hash_table_iter_next(&iter, &key, &value))
		{
			if (!g_hash_table_lookup(files_added, key))
			{
				PrjorgGotoSymbol *sym;
				sym = g_new0(PrjorgGotoSymbol, 1);
				sym->file_name = g_strdup(key);
				sym->name = g_path_get_basename(key);
				sym->icon = TM_ICON_NONE;
				g_ptr_array_add(arr, sym);
			}
		}
	}

	filtered = prjorg_goto_panel_filter(arr, file_str);
	prjorg_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
	g_ptr_array_free(arr, TRUE);
	g_hash_table_destroy(files_added);
}


/* symplified hard-coded icons because we don't have access to Geany icon mappings */
static int get_icon(TMTagType type)
{
	switch (type)
	{
		case tm_tag_class_t:
			return TM_ICON_CLASS;
		case tm_tag_macro_t:
		case tm_tag_macro_with_arg_t:
		case tm_tag_undef_t:
			return TM_ICON_MACRO;
		case tm_tag_enum_t:
		case tm_tag_struct_t:
		case tm_tag_typedef_t:
		case tm_tag_union_t:
			return TM_ICON_STRUCT;
		case tm_tag_enumerator_t:
		case tm_tag_field_t:
		case tm_tag_member_t:
			return TM_ICON_MEMBER;
		case tm_tag_method_t:
		case tm_tag_function_t:
		case tm_tag_prototype_t:
			return TM_ICON_METHOD;
		case tm_tag_interface_t:
		case tm_tag_namespace_t:
		case tm_tag_package_t:
			return TM_ICON_NAMESPACE;
		case tm_tag_variable_t:
		case tm_tag_externvar_t:
		case tm_tag_local_var_t:
		case tm_tag_include_t:
			return TM_ICON_VAR;
		case tm_tag_other_t:
			return TM_ICON_OTHER;
		default:
			return TM_ICON_NONE;
	}
}


/* stolen from Geany */
static gboolean langs_compatible(TMParserType lang, TMParserType other)
{
	if (lang == other)
		return TRUE;
	/* Accept CPP tags for C lang and vice versa - we don't have acces to
	 * Geany's TM_PARSER_ constants so let's hard-code 0 and 1 here (not too
	 * terrible things will happen if Geany changes these to something else) */
	else if (lang == 0 && other == 1)
		return TRUE;
	else if (lang == 1 && other == 0)
		return TRUE;

	return FALSE;
}


static void goto_tm_symbol(const gchar *query, GPtrArray *tags, TMParserType lang)
{
	GPtrArray *converted = g_ptr_array_new_full(0, (GDestroyNotify)prjorg_goto_symbol_free);
	GPtrArray *filtered;
	TMTag *tag;
	guint i;

	if (tags)
	{
		foreach_ptr_array(tag, i, tags)
		{
			if (tag->file && langs_compatible(tag->lang, lang) &&
				!(tag->type & (tm_tag_include_t | tm_tag_local_var_t)))
			{
				PrjorgGotoSymbol *sym = g_new0(PrjorgGotoSymbol, 1);
				sym->name = g_strdup(tag->name);
				sym->file_name = utils_get_utf8_from_locale(tag->file->file_name);
				sym->line = tag->line;
				sym->icon = get_icon(tag->type);

				g_ptr_array_add(converted, sym);
			}
		}
	}

	filtered = prjorg_goto_panel_filter(converted, query);
	prjorg_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
	g_ptr_array_free(converted, TRUE);
}


static void perform_lookup(const gchar *query)
{
	GeanyDocument *doc = document_get_current();
	const gchar *query_str = query ? query : "";

	if (g_str_has_prefix(query_str, "#"))
	{
		if (doc)
		{
			// TODO: possibly improve performance by binary searching the start and the end point
			goto_tm_symbol(query_str+1, geany_data->app->tm_workspace->tags_array, doc->file_type->lang);
		}
	}
	else if (g_str_has_prefix(query_str, "@"))
	{
		if (doc)
		{
			GPtrArray *tags = doc->tm_file ? doc->tm_file->tags_array : g_ptr_array_new();
			goto_tm_symbol(query_str+1, tags, doc->file_type->lang);
			if (!doc->tm_file)
				g_ptr_array_free(tags, TRUE);
		}
	}
	else if (g_str_has_prefix(query_str, ":"))
	{
		if (doc)
			goto_line(doc, query_str+1);
	}
	else
		goto_file(query_str);
}


static gchar *get_current_iden(GeanyDocument *doc, gint current_pos)
{
	//TODO: use configured wordchars (also change in Geany)
	const gchar *wordchars = GEANY_WORDCHARS;
	GeanyFiletypeID ft = doc->file_type->id;
	ScintillaObject *sci = doc->editor->sci;
	gint start_pos, end_pos, pos;

	if (ft == GEANY_FILETYPES_LATEX)
		wordchars = GEANY_WORDCHARS"\\"; /* add \ to word chars if we are in a LaTeX file */
	else if (ft == GEANY_FILETYPES_CSS)
		wordchars = GEANY_WORDCHARS"-"; /* add - because they are part of property names */

	pos = current_pos;
	while (TRUE)
	{
		gint new_pos = SSM(sci, SCI_POSITIONBEFORE, pos, 0);
		if (new_pos == pos)
			break;
		if (pos - new_pos == 1)
		{
			gchar c = sci_get_char_at(sci, new_pos);
			if (!strchr(wordchars, c))
				break;
		}
		pos = new_pos;
	}
	start_pos = pos;

	pos = current_pos;
	while (TRUE)
	{
		gint new_pos = SSM(sci, SCI_POSITIONAFTER, pos, 0);
		if (new_pos == pos)
			break;
		if (new_pos - pos == 1)
		{
			gchar c = sci_get_char_at(sci, pos);
			if (!strchr(wordchars, c))
				break;
		}
		pos = new_pos;
	}
	end_pos = pos;

	if (start_pos == end_pos)
		return NULL;

	return sci_get_contents_range(sci, start_pos, end_pos);
}


static void goto_panel_query(const gchar *query_type, gboolean prefill)
{
	GeanyDocument *doc = document_get_current();
	gchar *query = NULL;
	gint pos;

	if (prefill && doc)
	{
		pos = sci_get_current_position(doc->editor->sci);
		query = get_current_iden(doc, pos);
	}
	if (!query)
		query = g_strdup("");
	SETPTR(query, g_strconcat(query_type, query, NULL));

	prjorg_goto_panel_show(query, perform_lookup);

	g_free(query);
}


void prjorg_goto_anywhere_for_workspace(void)
{
	goto_panel_query("#", TRUE);
}


void prjorg_goto_anywhere_for_doc(void)
{
	goto_panel_query("@", TRUE);
}


void prjorg_goto_anywhere_for_line(void)
{
	goto_panel_query(":", FALSE);
}


void prjorg_goto_anywhere_for_file(void)
{
	goto_panel_query("", FALSE);
}
