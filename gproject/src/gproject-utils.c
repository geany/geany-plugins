/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
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

#include <string.h>
#include <glib.h>

#include <geanyplugin.h>

#include "gproject-utils.h"

extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;

static gchar *relpath(const gchar *origin_dir, const gchar *dest_dir)
{
	gchar *origin, *dest;
	gchar **originv, **destv;
	gchar *ret = NULL;
	guint i, j;
	
	origin = tm_get_real_path(origin_dir);
	dest = tm_get_real_path(dest_dir);

	if (EMPTY(origin) || EMPTY(dest) || origin[0] != dest[0])
	{
		g_free(origin);
		g_free(dest);
		return NULL;
	}

	originv = g_strsplit_set(g_path_skip_root(origin), "/\\", -1);
	destv = g_strsplit_set(g_path_skip_root(dest), "/\\", -1);

	for (i = 0; originv[i] != NULL && destv[i] != NULL; i++)
		if (g_strcmp0(originv[i], destv[i]) != 0)
			break;

	ret = g_strdup("");

	for (j = i; originv[j] != NULL; j++)
		SETPTR(ret, g_build_filename(ret, "..", NULL));

	for (j = i; destv[j] != NULL; j++)
		SETPTR(ret, g_build_filename(ret, destv[j], NULL));

	if (strlen(ret) == 0)
		SETPTR(ret, g_strdup("./"));

	g_free(origin);
	g_free(dest);
	g_strfreev(originv);
	g_strfreev(destv);

	return ret;
}


gchar *get_file_relative_path(const gchar *origin_dir, const gchar *dest_file)
{
	gchar *dest_dir, *ret;

	dest_dir = g_path_get_dirname(dest_file);
	ret = relpath(origin_dir, dest_dir);
	if (ret)
	{
		gchar *dest_basename;

		dest_basename = g_path_get_basename(dest_file);

		if (g_strcmp0(ret, "./") != 0)
		{
			SETPTR(ret, g_build_filename(ret, dest_basename, NULL));
		}
		else
		{
			SETPTR(ret, g_strdup(dest_basename));
		}

		g_free(dest_basename);
	}

	g_free(dest_dir);
	return ret;
}


GSList *get_precompiled_patterns(gchar **patterns)
{
	guint i;
	GSList *pattern_list = NULL;

	if (!patterns)
		return NULL;

	for (i = 0; patterns[i] != NULL; i++)
	{
		GPatternSpec *pattern_spec = g_pattern_spec_new(patterns[i]);
		pattern_list = g_slist_prepend(pattern_list, pattern_spec);
	}
	return pattern_list;
}


gboolean patterns_match(GSList *patterns, const gchar *str)
{
	GSList *elem;
	foreach_slist (elem, patterns)
	{
		GPatternSpec *pattern = elem->data;
		if (g_pattern_match_string(pattern, str))
			return TRUE;
	}
	return FALSE;
}


void open_file(gchar *utf8_name)
{
	gchar *name;
	GeanyDocument *doc;

	name = utils_get_locale_from_utf8(utf8_name);
	doc = document_find_by_filename(utf8_name);

	if (!doc)
		doc = document_open_file(name, FALSE, NULL, NULL);
	else
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(geany->main_widgets->notebook),
			document_get_notebook_page(doc));
	}
	
	if (doc)
		gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));

	g_free(name);
}


gchar *get_selection(void)
{
	GeanyDocument *doc = document_get_current();
	const gchar *wc;

#ifdef G_OS_WIN32
	wc = GEANY_WORDCHARS "./-" "\\";
#else
	wc = GEANY_WORDCHARS "./-";
#endif

	if (!doc)
		return NULL;

	if (sci_has_selection(doc->editor->sci))
		return sci_get_selection_contents(doc->editor->sci);
	else
		return editor_get_word_at_pos(doc->editor, -1, wc);
}
