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
}
ScanDirParams;


typedef struct
{
	GSList *filelist;
	GHashTable *visited_paths;
	void (*callback)(const gchar *path, gboolean *add, gboolean *enter, void *userdata);
	void *userdata;
}
ScanDirParamsCallback;


/** Get precompiled patterns.
 *
 * The function builds the precompiled patterns for @a patterns and returns them
 * as a list.
 *
 * @param patterns NULL terminated string array of patterns.
 * @return Pointer to GSList of patterns or NULL if patterns == NULL
 *
 **/
GSList *filelist_get_precompiled_patterns(gchar **patterns)
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
gboolean filelist_patterns_match(GSList *patterns, const gchar *str)
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
static void filelist_scan_directory_int(const gchar *searchdir, ScanDirParams *params, guint flags)
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
		{
			break;
		}

		utf8_name = utils_get_utf8_from_locale(locale_name);
		locale_filename = g_build_filename(locale_path, locale_name, NULL);
		utf8_filename = utils_get_utf8_from_locale(locale_filename);

		if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
		{
			if (!filelist_patterns_match(params->ignored_dirs_list, utf8_name))
			{
				filelist_scan_directory_int(utf8_filename, params, flags);
				params->folder_count++;
				if (flags & FILELIST_FLAG_ADD_DIRS)
				{
					params->filelist = g_slist_prepend(params->filelist, g_strdup(utf8_filename));
				}
			}
		}
		else if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR))
		{
			if (filelist_patterns_match(params->file_patterns, utf8_name) && !
				filelist_patterns_match(params->ignored_file_list, utf8_name))
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
GSList *gp_filelist_scan_directory(guint *files, guint *folders, const gchar *searchdir, gchar **file_patterns,
		gchar **ignored_dirs_patterns, gchar **ignored_file_patterns)
{
	ScanDirParams params = { 0 };

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
	filelist_scan_directory_int(searchdir, &params, 0);
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


/** Scan a directory and return a list of files and directories.
 *
 * The function scans directory searchdir and returns a list of files.
 * The list will only include files which match the patterns in file_patterns.
 * Directories or files matched by ignored_dirs_patterns or ignored_file_patterns
 * will not be scanned or added to the list.
 *
 * If flags is 0 then the result will be the same as for gp_filelist_scan_directory().
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
 * @param flags     Flags which influence the returned list:
 *                  - FILELIST_FLAG_ADD_DIRS: if set, directories will be added
 *                    as own list entries. This also includes empty dirs.
 * @return GSList of matched files
 *
 **/
GSList *gp_filelist_scan_directory_full(guint *files, guint *folders, const gchar *searchdir, gchar **file_patterns,
		gchar **ignored_dirs_patterns, gchar **ignored_file_patterns, guint flags)
{
	ScanDirParams params = { 0 };

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
	filelist_scan_directory_int(searchdir, &params, flags);
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


/** Check if a filepath matches the given patterns.
 *
 * If the filepath belongs to a regular file, then TRUE will be returned if
 * the filepath matches the patterns in file_patterns but does not match the
 * patterns in ignored_file_patterns.
 * 
 * If the filepath belongs to a directory, then TRUE will be returned if it
 * does not match the patterns in ignored_dirs_patterns.
 *
 * @param filepath  The file or dorectory path to check
 * @param file_patterns
 *                  File patterns for matching files (e.g. "*.c") or NULL
 *                  for all files.
 * @param ignored_dirs_patterns
 *                  Patterns for ignored directories
 * @param ignored_file_patterns
 *                  Patterns for ignored files
 * @return gboolean
 *
 **/
gboolean gp_filelist_filepath_matches_patterns(const gchar *filepath, gchar **file_patterns,
		gchar **ignored_dirs_patterns, gchar **ignored_file_patterns)
{
	gboolean match = FALSE;
	GSList *file_patterns_list;
	GSList *ignored_dirs_list;
	GSList *ignored_file_list;

	if (!file_patterns || !file_patterns[0])
	{
		const gchar *all_pattern[] = { "*", NULL };
		file_patterns_list = filelist_get_precompiled_patterns((gchar **)all_pattern);
	}
	else
	{
		file_patterns_list = filelist_get_precompiled_patterns(file_patterns);
	}

	ignored_dirs_list = filelist_get_precompiled_patterns(ignored_dirs_patterns);
	ignored_file_list = filelist_get_precompiled_patterns(ignored_file_patterns);

	if (g_file_test(filepath, G_FILE_TEST_IS_DIR))
	{
		if (!filelist_patterns_match(ignored_dirs_list, filepath))
		{
			match = TRUE;
		}
	}
	else if (g_file_test(filepath, G_FILE_TEST_IS_REGULAR))
	{
		if (filelist_patterns_match(file_patterns_list, filepath) &&
			!filelist_patterns_match(ignored_file_list, filepath))
		{
			match = TRUE;
		}
	}

	g_slist_foreach(file_patterns_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(file_patterns_list);

	g_slist_foreach(ignored_dirs_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(ignored_dirs_list);

	g_slist_foreach(ignored_file_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(ignored_file_list);

	return match;
}


/* Scan directory searchdir. Let a callback-function decide which files
   to add and which directories to enter/crawl. */
static void filelist_scan_directory_callback_int(const gchar *searchdir, ScanDirParamsCallback *params)
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
		gboolean add, enter;

		locale_name = g_dir_read_name(dir);
		if (!locale_name)
		{
			break;
		}

		utf8_name = utils_get_utf8_from_locale(locale_name);
		locale_filename = g_build_filename(locale_path, locale_name, NULL);
		utf8_filename = utils_get_utf8_from_locale(locale_filename);

		/* Call user callback. */
		params->callback(locale_filename, &add, &enter, params->userdata);

		/* Should the file/path be added to the list? */
		if (add)
		{
			params->filelist = g_slist_prepend(params->filelist, g_strdup(utf8_filename));
		}

		/* If the path is a directory, should it be entered? */
		if (enter && g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
		{
			filelist_scan_directory_callback_int(utf8_filename, params);
		}

		g_free(utf8_filename);
		g_free(locale_filename);
		g_free(utf8_name);
	}

	g_dir_close(dir);
	g_free(locale_path);
}


/** Scan a directory and return a list of files and directories.
 *
 * The function scans directory searchdir and returns a list of files.
 * The list will only include files for which the callback function returned
 * 'add == TRUE'. Sub-directories will only be scanned if the callback function
 * returned 'enter == TRUE'.
 * 
 * @param searchdir Directory which shall be scanned
 * @param callback  Function to be called to decide which files to add to
 *                  the list and to decide which sub-directories to enter
 *                  or to ignore.
 * @param userdata  A pointer which is transparently passed through
 *                  to the callback function.
 * @return GSList of matched files
 *
 **/
GSList *gp_filelist_scan_directory_callback(const gchar *searchdir,
	void (*callback)(const gchar *path, gboolean *add, gboolean *enter, void *userdata),
	void *userdata)
{
	ScanDirParamsCallback params = { 0 };

	params.callback = callback;
	params.userdata = userdata;
	params.visited_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	filelist_scan_directory_callback_int(searchdir, &params);
	g_hash_table_destroy(params.visited_paths);

	return params.filelist;
}
