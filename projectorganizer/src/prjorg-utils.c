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

#include "prjorg-utils.h"

extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;


/* utf8 */
gchar *get_relative_path(const gchar *utf8_parent, const gchar *utf8_descendant)
{
	GFile *gf_parent, *gf_descendant;
	gchar *locale_parent, *locale_descendant;
	gchar *locale_ret, *utf8_ret;

	locale_parent = utils_get_locale_from_utf8(utf8_parent);
	locale_descendant = utils_get_locale_from_utf8(utf8_descendant);
	gf_parent = g_file_new_for_path(locale_parent);
	gf_descendant = g_file_new_for_path(locale_descendant);

	locale_ret = g_file_get_relative_path(gf_parent, gf_descendant);
	utf8_ret = utils_get_utf8_from_locale(locale_ret);

	g_object_unref(gf_parent);
	g_object_unref(gf_descendant);
	g_free(locale_parent);
	g_free(locale_descendant);
	g_free(locale_ret);

	return utf8_ret;
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


/* utf8 */
gchar *get_project_base_path(void)
{
	GeanyProject *project = geany_data->app->project;

	if (project && !EMPTY(project->base_path))
	{
		if (g_path_is_absolute(project->base_path))
			return g_strdup(project->base_path);
		else
		{	/* build base_path out of project file name's dir and base_path */
			gchar *path;
			gchar *dir = g_path_get_dirname(project->file_name);

			if (utils_str_equal(project->base_path, "./"))
				return dir;

			path = g_build_filename(dir, project->base_path, NULL);
			g_free(dir);
			return path;
		}
	}
	return NULL;
}
