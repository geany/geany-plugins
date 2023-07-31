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

#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <geanyplugin.h>

#include "prjorg-project.h"
#include "prjorg-utils.h"

extern GeanyData *geany_data;


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
	GSList *elem = NULL;
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


void close_file(gchar *utf8_name)
{
	GeanyDocument *doc = document_find_by_filename(utf8_name);

	if (doc)
	{
		document_set_text_changed(doc, FALSE);
		document_close(doc);
	}
}


gboolean create_file(gchar *utf8_name)
{
	gchar *name = utils_get_locale_from_utf8(utf8_name);
	gint fd = g_open(name, O_CREAT|O_EXCL, 0660);  // rw-rw----
	gboolean success = FALSE;

	if (fd != -1)
	{
		GError *err;
		success = TRUE;
		g_close(fd, &err);
	}
	g_free(name);
	return success;
}


gboolean create_dir(char *utf8_name)
{
	gchar *name = utils_get_locale_from_utf8(utf8_name);
	gboolean ret = g_mkdir_with_parents(name, 0770) == 0;  // rwxrwx---
	g_free(name);
	return ret;
}


gboolean remove_file_or_dir(char *utf8_name)
{
	gboolean ret = FALSE;
	gchar *name = utils_get_locale_from_utf8(utf8_name);

	GFile *file = g_file_new_for_path(name);
	ret = g_file_trash(file, NULL, NULL);
	if (!ret)
		ret = g_file_delete(file, NULL, NULL);

	g_free(name);
	g_object_unref(file);
	return ret;
}


static gboolean document_rename(GeanyDocument *document, gchar *utf8_name)
{
	// IMHO: this is wrong. If save as fails Geany's state becomes inconsistent.
	document_rename_file(document, utf8_name);
	return document_save_file_as(document, utf8_name);
}


gboolean rename_file_or_dir(gchar *utf8_oldname, gchar *utf8_newname)
{
	GeanyDocument *doc = document_find_by_filename(utf8_oldname);
	if (doc)
		return document_rename(doc, utf8_newname);
	else
	{
		gchar *oldname = utils_get_locale_from_utf8(utf8_oldname);
		gchar *newname = utils_get_locale_from_utf8(utf8_newname);
		gint res = g_rename(oldname, newname);
		g_free(oldname);
		g_free(newname);
		return res == 0;
	}
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


GtkWidget *menu_item_new(const gchar *icon_name, const gchar *label)
{
	GtkWidget *item = gtk_image_menu_item_new_with_mnemonic(label);

	if (icon_name != NULL)
	{
		GtkWidget *image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
		gtk_widget_show(image);
	}
	gtk_widget_show(item);
	return item;
}

gchar *try_find_header_source(gchar *utf8_file_name, gboolean is_header, GSList *file_list, GSList *header_patterns, GSList *source_patterns)
{
	gchar *full_name;
	gchar *name_pattern;
	GSList *elem = NULL;
	GPatternSpec *pattern;
	gboolean found = FALSE;

	name_pattern = g_path_get_basename(utf8_file_name);
	SETPTR(name_pattern, utils_remove_ext_from_filename(name_pattern));
	SETPTR(name_pattern, g_strconcat(name_pattern, ".*", NULL));
	pattern = g_pattern_spec_new(name_pattern);
	g_free(name_pattern);

	foreach_slist (elem, file_list)
	{
		full_name = elem->data;
		gchar *base_name = g_path_get_basename(full_name);

		if (g_pattern_match_string(pattern, base_name))
		{
			if ((is_header && patterns_match(source_patterns, base_name)) ||
				(!is_header && patterns_match(header_patterns, base_name)))
			{
				g_free(base_name);
				found = TRUE;
				break;
			}
		}
		g_free(base_name);
	}

	g_pattern_spec_free(pattern);

	if(found)
		return g_strdup(full_name);

	return NULL;
}

gchar *find_header_source(GeanyDocument *doc)
{
	GSList *header_patterns, *source_patterns;
	gboolean known_type = TRUE;
	gboolean is_header = FALSE;
	gchar *found_name = NULL;

	if (!doc || !doc->file_name)
		return NULL;

	if (prj_org)
	{
		header_patterns = get_precompiled_patterns(prj_org->header_patterns);
		source_patterns = get_precompiled_patterns(prj_org->source_patterns);
	}
	else
	{
		// use default header/source patterns if project isn't open
		gchar **headers, **source;
		headers = g_strsplit(PRJORG_PATTERNS_HEADER, " ", -1);
		source = g_strsplit(PRJORG_PATTERNS_SOURCE, " ", -1);

		header_patterns = get_precompiled_patterns(headers);
		source_patterns = get_precompiled_patterns(source);

		g_strfreev(headers);
		g_strfreev(source);
	}

	// check if file matches patterns
	{
		gchar *doc_basename;
		doc_basename = g_path_get_basename(doc->file_name);

		if (patterns_match(header_patterns, doc_basename))
			is_header = TRUE;
		else if (patterns_match(source_patterns, doc_basename))
			is_header = FALSE;
		else
			known_type = FALSE;

		g_free(doc_basename);
	}

	if (known_type)
	{
		GSList *elem = NULL, *list = NULL;
		guint i = 0;

		/* check open documents */
		foreach_document(i)
		{
			gchar *filename;
			filename = document_index(i)->file_name;
			list = g_slist_prepend(list, filename);
		}
		found_name = try_find_header_source(doc->file_name, is_header, list, header_patterns, source_patterns);
		g_slist_free(list);
		list = NULL;

		/* check document directory */
		if (!found_name)
		{
			gchar *utf8_doc_dir;
			gchar *locale_doc_dir;

			utf8_doc_dir = g_path_get_dirname(doc->file_name);
			locale_doc_dir = utils_get_locale_from_utf8(utf8_doc_dir);

			list = utils_get_file_list(locale_doc_dir, NULL, NULL);
			foreach_list (elem, list)
			{
				gchar *full_name;
				full_name = g_build_filename(locale_doc_dir, elem->data, NULL);
				SETPTR(full_name, utils_get_utf8_from_locale(full_name));
				SETPTR(elem->data, full_name);
			}
			found_name = try_find_header_source(doc->file_name, is_header, list, header_patterns, source_patterns);
			g_slist_foreach(list, (GFunc) g_free, NULL);
			g_slist_free(list);
			g_free(utf8_doc_dir);
			g_free(locale_doc_dir);
			list = NULL;
		}

		/* check project files */
		if (!found_name && prj_org)
		{
			foreach_slist(elem, prj_org->roots)
			{
				GHashTableIter iter;
				gpointer key, value;
				PrjOrgRoot *root = elem->data;

				list = NULL;
				g_hash_table_iter_init(&iter, root->file_table);
				while (g_hash_table_iter_next(&iter, &key, &value))
					list = g_slist_prepend(list, key);
				found_name = try_find_header_source(doc->file_name, is_header, list, header_patterns, source_patterns);
				g_slist_free(list);
				if (found_name)
					break;
			}
		}
	}

	g_slist_foreach(header_patterns, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(header_patterns);
	g_slist_foreach(source_patterns, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(source_patterns);

	return found_name;
}


/* sets filetype of header to match source
 * changes filetype only if document is a header */
void set_header_filetype(GeanyDocument * doc)
{
	gchar *doc_basename, *full_name;
	gboolean is_header;
	GSList * header_patterns;

	if (!doc || !doc->file_name)
		return;

	if (prj_org)
	{
		header_patterns = get_precompiled_patterns(prj_org->header_patterns);
	}
	else
	{
		/* use default patterns when project isn't open */
		gchar ** patterns = g_strsplit(PRJORG_PATTERNS_HEADER, " ", -1);
		header_patterns = get_precompiled_patterns(patterns);
		g_strfreev(patterns);
	}

	doc_basename = g_path_get_basename(doc->file_name);
	is_header = patterns_match(header_patterns, doc_basename);
	full_name = is_header ? find_header_source(doc) : NULL;

	if (full_name)
	{
		GeanyFiletype * source_filetype = filetypes_detect_from_file(full_name);
		document_set_filetype(doc, source_filetype);
		g_free(full_name);
	}

	g_free(doc_basename);
	g_slist_free(header_patterns);
}
