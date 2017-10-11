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
 * Code for the WB_PROJECT structure.
 */
#include <glib.h>
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "filelist.h"
#include <geanyplugin.h>

typedef struct
{
	guint file_count;
	guint folder_count;
	GSList *filelist;
	GSList *file_patterns;
	GSList *ignored_dirs_list;
	GSList *ignored_file_list;
	GHashTable *visited_paths;
}SCAN_DIR_PARAMS;


/** Get precompiled patterns.
 *
 * The function builds the precompiled patterns for @a patterns and returns them
 * as a list.
 *
 * @param patterns NULL terminated string array of patterns.
 * @return Pointer to GSList of patterns or NULL if patterns == NULL
 *
 **/
static GSList *filelist_get_precompiled_patterns(gchar **patterns)
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


/** Check if a string matches a pattern.
 *
 * The function checks if @a str matches pattern @a patterns.
 *
 * @param patterns Pattern list.
 * @param str      String to check
 * @return TRUE if str matches the pattern, FALSE otherwise
 *
 **/
static gboolean filelist_patterns_match(GSList *patterns, const gchar *str)
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


/* Scan directory searchdir. Input and output parameters come from/go to params. */
static void filelist_scan_directory_int(const gchar *searchdir, SCAN_DIR_PARAMS *params)
{
	GDir *dir;
	gchar *locale_path = utils_get_locale_from_utf8(searchdir);
	gchar *real_path = utils_get_real_path(locale_path);

	dir = g_dir_open(locale_path, 0, NULL);
	if (!dir || !real_path || g_hash_table_lookup(params->visited_paths, real_path))
	{
		if (dir != NULL)
		{
			g_dir_close(dir);
		}
		g_free(locale_path);
		g_free(real_path);
		return;
	}

	g_hash_table_insert(params->visited_paths, real_path, GINT_TO_POINTER(1));

	while (TRUE)
	{
		const gchar *locale_name;
		gchar *locale_filename, *utf8_filename, *utf8_name;

		locale_name = g_dir_read_name(dir);
		if (!locale_name)
			break;

		utf8_name = utils_get_utf8_from_locale(locale_name);
		locale_filename = g_build_filename(locale_path, locale_name, NULL);
		utf8_filename = utils_get_utf8_from_locale(locale_filename);

		if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
		{
			if (!filelist_patterns_match(params->ignored_dirs_list, utf8_name))
			{
				filelist_scan_directory_int(utf8_filename, params);
				params->folder_count++;
			}
		}
		else if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR))
		{
			if (filelist_patterns_match(params->file_patterns, utf8_name) && !filelist_patterns_match(params->ignored_file_list, utf8_name))
			{
				params->file_count++;
				params->filelist = g_slist_prepend(params->filelist, g_strdup(utf8_filename));
			}
		}

		g_free(utf8_filename);
		g_free(locale_filename);
		g_free(utf8_name);
	}

	g_dir_close(dir);
	g_free(locale_path);
}


/** Scan a directory and return a list of files.
 *
 * The function scans directory searchdir and returns a list of files.
 * The list will only include files which match the patterns in file_patterns.
 * Directories or files matched by ignored_dirs_patterns or ignored_file_patterns
 * will not be scanned or added to the list.
 *
 * @param files     Can be optionally specified to return the number of matched/found
 *                  files in *files.
 * @param folders   Can be optionally specified to return the number of matched/found
 *                  folders/sub-directories in *folders.
 * @param searchdir Directory which shall be scanned
 * @param file_patterns
 *                  File patterns for matching files (e.g. "*.c") or NULL
 *                  for all files.
 * @param ignored_dirs_patterns
 *                  Patterns for ignored directories
 * @param ignored_file_patterns
 *                  Patterns for ignored files
 * @return GSList of matched files
 *
 **/
GSList *filelist_scan_directory(guint *files, guint *folders, const gchar *searchdir, gchar **file_patterns,
		gchar **ignored_dirs_patterns, gchar **ignored_file_patterns)
{
	SCAN_DIR_PARAMS params;

	memset(&params, 0, sizeof(params));

	if (!file_patterns || !file_patterns[0])
	{
		const gchar *all_pattern[] = { "*", NULL };
		params.file_patterns = filelist_get_precompiled_patterns((gchar **)all_pattern);
	}
	else
	{
		params.file_patterns = filelist_get_precompiled_patterns(file_patterns);
	}

	params.ignored_dirs_list = filelist_get_precompiled_patterns(ignored_dirs_patterns);
	params.ignored_file_list = filelist_get_precompiled_patterns(ignored_file_patterns);

	params.visited_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	filelist_scan_directory_int(searchdir, &params);
	g_hash_table_destroy(params.visited_paths);

	g_slist_foreach(params.file_patterns, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(params.file_patterns);

	g_slist_foreach(params.ignored_dirs_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(params.ignored_dirs_list);

	g_slist_foreach(params.ignored_file_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(params.ignored_file_list);

	if (files != NULL)
	{
		*files = params.file_count;
	}
	if (folders != NULL)
	{
		*folders = params.folder_count;
	}

	return params.filelist;
}
