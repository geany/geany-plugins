/*
 * Copyright 2017 LarsGit223
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

/*
 * Utility functions.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <geanyplugin.h>
#include "wb_globals.h"
#include "utils.h"

/** Get the relative path.
 *
 * The function examines the relative path of @a utf8_descendant in relation to
 * @a utf8_parent.
 *
 * @param utf8_parent     Parent directory.
 * @param utf8_descendant Sub directory.
 *
 **/
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


/** Combine an absolute and a relative path.
 *
 * The function combines the absolute path @a base with the relative path
 * @a relative and retunrs it as a string. The caller needs to free the
 * string with g_free().
 *
 * @param base     The absolute path.
 * @param relative The relative path.
 * @return Combined path or NULL if no memory is available
 *
 **/
gchar *get_combined_path(const gchar *base, const gchar *relative)
{
	gchar *basedir, *basedir_end;
	const gchar *start;
	gchar *result;
	gint goback;

	start = relative;
	basedir = g_path_get_dirname (base);
	if (relative[0] == '.')
	{
		if (strncmp("..", relative, sizeof("..")-1) == 0)
		{
			start = &(relative[sizeof("..")-1]);
		}

		goback = 0;
		while (*start != '\0')
		{
			if (strncmp("..", &(start[1]), sizeof("..")-1) == 0)
			{
				start += 1 + (sizeof("..")-1);
				goback++;
			}
			else
			{
				break;
			}
		}

		basedir_end = &(basedir[strlen(basedir)]);
		while (goback > 0)
		{
			while (basedir_end > basedir && *basedir_end != G_DIR_SEPARATOR)
			{
				basedir_end--;
			}
			if (*basedir_end == G_DIR_SEPARATOR)
			{
				*basedir_end = '\0';
			}
			else
			{
				break;
			}
			goback--;
		}
	}

	result = g_strconcat(basedir, start, NULL);
	return result;
}


/** Get the relative path.
 *
 * The function examines the relative path of @a target in relation to
 * @a base. It is not required that target is a sub-directory of base.
 *
 * @param base   Base directory.
 * @param target Target directory or NULL in case of error.
 *
 **/
gchar *get_any_relative_path (const gchar *base, const gchar *target)
{
	guint index = 0, equal, equal_index = 0, base_parts = 0, length;
	GPtrArray *relative;
	gchar *part;
	gchar *result = NULL;
	gchar **splitv_base;
	gchar **splitv_target;

	/* Split up pathes into parts and count them */
	splitv_base = g_strsplit (base, G_DIR_SEPARATOR_S, -1);
	index = 0;
	while (splitv_base[index] != NULL)
	{
		if (strlen(splitv_base[index]) > 0)
		{
			base_parts++;
		}
		index++;
	}
	splitv_target = g_strsplit (target, G_DIR_SEPARATOR_S, -1);
	index = 0;;
	while (splitv_target[index] != NULL)
	{
		index++;
	}

	/* Count equal dirs */
	equal = 0;
	index = 0;
	while (splitv_base[index] != NULL && splitv_target[index] != NULL)
	{
		if (g_strcmp0 (splitv_base[index], splitv_target[index]) == 0)
		{
			/* We might encounter empty strings! */
			if (strlen(splitv_base[index]) > 0)
			{
				equal++;
				equal_index = index;
		    }
		}
		else
		{
			break;
		}
		index++;
	}

	length = 0;
	relative = g_ptr_array_new();
	if (equal < base_parts)
	{
		/* Go back, add ".." */
		for (index = 0 ; index < (base_parts-equal) ; index++)
		{
			if (index == 0)
			{
				g_ptr_array_add(relative, g_strdup(".."));
				length += 2;
			}
			else
			{
				length += 2 + strlen(G_DIR_SEPARATOR_S);
				g_ptr_array_add(relative, g_strdup(G_DIR_SEPARATOR_S));
				g_ptr_array_add(relative, g_strdup(".."));
			}
		}

		/* Add directories */
		index = equal_index + 1;
		while (splitv_target[index] != NULL)
		{
			if (strlen(splitv_target[index]) > 0)
			{
				length += strlen(G_DIR_SEPARATOR_S)
						  + strlen(splitv_target[index]);
				g_ptr_array_add(relative, g_strdup(G_DIR_SEPARATOR_S));
				g_ptr_array_add(relative, g_strdup(splitv_target[index]));
			}
			index++;
		}
	}

	/* Copy it all together */
	result = g_new(char, length+1);
	if (result != NULL)
	{
		guint strpos = 0;

		for (index = 0 ; index < relative->len ; index++)
		{
			part = g_ptr_array_index(relative, index);
			g_strlcpy (&(result[strpos]),
					   part,
					   length+1-strpos);
			strpos += strlen(part);
			g_free(part);
		}
	}
	else
	{
		for (index = 0 ; index < relative->len ; index++)
		{
			part = g_ptr_array_index(relative, index);
			g_free(part);
		}
		result = NULL;
	}
	g_ptr_array_free(relative, TRUE);

	return result;
}


/** Open all files in list.
 *
 * @param list GPtrArray containing file names.
 *
 **/
void open_all_files_in_list(GPtrArray *list)
{
	guint index;

	for (index = 0 ; index < list->len ; index++)
	{
		document_open_file(list->pdata[index], FALSE, NULL, NULL);
	}
}


/** Close all files in list.
 *
 * @param list GPtrArray containing file names.
 *
 **/
void close_all_files_in_list(GPtrArray *list)
{
	GeanyData* geany_data = wb_globals.geany_plugin->geany_data;
	guint index, doc=0;

	for (index = 0 ; index < list->len ; index++)
	{
		foreach_document(doc)
		{
			if (g_strcmp0(list->pdata[index], documents[doc]->file_name) == 0)
			{
				document_close(documents[doc]);
				break;
			}
		}
	}
}


gboolean is_git_repository(gchar *path)
{
	gboolean is_git_repo;
	gchar *git_path;

	git_path = g_build_filename(path, ".git", NULL);
	is_git_repo = g_file_test(git_path, G_FILE_TEST_IS_DIR);
	g_free(git_path);

	return is_git_repo;
}
