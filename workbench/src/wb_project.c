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
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>
#include <../../utils/src/filelist.h>
#include "wb_globals.h"
#include "wb_project.h"
#include "sidebar.h"
#include "utils.h"

extern GeanyData *geany_data;

typedef enum
{
	WB_PROJECT_TAG_PREFS_AUTO,
	WB_PROJECT_TAG_PREFS_YES,
	WB_PROJECT_TAG_PREFS_NO,
}WB_PROJECT_TAG_PREFS;

typedef struct
{
	WB_PROJECT_IDLE_ACTION_ID id;
	gpointer param_a;
	gpointer param_b;
}WB_PROJECT_IDLE_ACTION;

typedef struct
{
	GKeyFile *kf;
	guint    dir_count;
}WB_PROJECT_ON_SAVE_USER_DATA;

struct S_WB_PROJECT_DIR
{
	gchar *name;
	gchar *base_dir;
	gchar **file_patterns;	/**< Array of filename extension patterns. */
	gchar **ignored_dirs_patterns;
	gchar **ignored_file_patterns;
	guint file_count;
	guint subdir_count;
	GHashTable *file_table; /* contains all file names within base_dir, maps file_name->TMSourceFile */
	gboolean is_prj_base_dir;
};

struct S_WB_PROJECT
{
	gchar     *filename;
	gchar     *name;
	gboolean  modified;
	//GSList    *s_idle_add_funcs;
	//GSList    *s_idle_remove_funcs;
	GSList    *directories;  /* list of WB_PROJECT_DIR; */
	WB_PROJECT_TAG_PREFS generate_tag_prefs;
	GPtrArray *bookmarks;
};

typedef struct
{
	guint len;
	const gchar *string;
}WB_PROJECT_TEMP_DATA;

static GSList *s_idle_actions = NULL;
static void wb_project_dir_update_tags(WB_PROJECT_DIR *root);

/** Set the projects modified marker.
 *
 * @param prj   The project
 * @param value The value to set
 *
 **/
void wb_project_set_modified(WB_PROJECT *prj, gboolean value)
{
	if (prj != NULL)
	{
		prj->modified = value;
	}
}


/** Has the project been modified since the last save?
 *
 * @param prj   The project
 * @return TRUE if project is modified, FALSE otherwise
 *
 **/
gboolean wb_project_is_modified(WB_PROJECT *prj)
{
	if (prj != NULL)
	{
		return (prj->modified);
	}
	return FALSE;
}


/** Set the filename of a project.
 *
 * @param prj      The project
 * @param filename The filename
 *
 **/
void wb_project_set_filename(WB_PROJECT *prj, const gchar *filename)
{
	if (prj != NULL)
	{
		guint offset;
		gchar *ext;

		g_free(prj->filename);
		prj->filename = g_strdup(filename);
		g_free(prj->name);
		prj->name = g_path_get_basename (filename);
		ext = g_strrstr(prj->name, ".geany");
		if(ext != NULL)
		{
			offset = strlen(prj->name);
			offset -= strlen(".geany");
			if (ext == prj->name + offset)
			{
				/* Strip of file extension by overwriting
				   '.' with string terminator. */
				prj->name[offset] = '\0';
			}
		}
	}
}


/** Get the filename of a project.
 *
 * @param prj The project
 * @return The filename
 *
 **/
const gchar *wb_project_get_filename(WB_PROJECT *prj)
{
	if (prj != NULL)
	{
		return prj->filename;
	}
	return NULL;
}


/** Get the name of a project.
 *
 * @param prj The project
 * @return The project name
 *
 **/
const gchar *wb_project_get_name(WB_PROJECT *prj)
{
	if (prj != NULL)
	{
		return prj->name;
	}
	return NULL;
}


/** Get the list of directories contained in the project.
 *
 * @param prj The project
 * @return GSList of directories (WB_PROJECT_DIRs)
 *
 **/
GSList *wb_project_get_directories(WB_PROJECT *prj)
{
	if (prj != NULL)
	{
		return prj->directories;
	}
	return NULL;
}


/* Check if filename matches filetpye patterns */
static gboolean match_basename(gconstpointer pft, gconstpointer user_data)
{
	const GeanyFiletype *ft = pft;
	const gchar *utf8_base_filename = user_data;
	gint j;
	gboolean ret = FALSE;

	if (G_UNLIKELY(ft->id == GEANY_FILETYPES_NONE))
		return FALSE;

	for (j = 0; ft->pattern[j] != NULL; j++)
	{
		GPatternSpec *pattern = g_pattern_spec_new(ft->pattern[j]);

		if (g_pattern_match_string(pattern, utf8_base_filename))
		{
			ret = TRUE;
			g_pattern_spec_free(pattern);
			break;
		}
		g_pattern_spec_free(pattern);
	}
	return ret;
}


/* Clear idle queue */
static void wb_project_clear_idle_queue(void)
{
	if (s_idle_actions == NULL)
	{
		return;
	}

	g_slist_free_full(s_idle_actions, g_free);
	s_idle_actions = NULL;
}


/* Create a new project dir with base path "utf8_base_dir" */
static WB_PROJECT_DIR *wb_project_dir_new(WB_PROJECT *prj, const gchar *utf8_base_dir)
{
	guint offset;

	if (utf8_base_dir == NULL)
	{
		return NULL;
	}
	WB_PROJECT_DIR *dir = g_new0(WB_PROJECT_DIR, 1);
	dir->base_dir = g_strdup(utf8_base_dir);
	dir->file_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GFreeFunc)tm_source_file_free);

	offset = strlen(dir->base_dir)-1;
	while (offset > 0
		   && dir->base_dir[offset] != '\\'
		   && dir->base_dir[offset] != '/')
	{
		offset--;
	}
	if (offset != 0)
	{
		offset++;
	}
	dir->name = g_strdup(&(dir->base_dir[offset]));
	dir->is_prj_base_dir = FALSE;

	return dir;
}


/* Collect source files */
static void wb_project_dir_collect_source_files(G_GNUC_UNUSED gchar *filename, TMSourceFile *sf, gpointer user_data)
{
	GPtrArray *array = user_data;

	if (sf != NULL)
		g_ptr_array_add(array, sf);
}


/** Set "is project base dir" of a project dir.
 *
 * @param directory The project dir
 * @param value     TRUE:  directory is the base dir of the project
 *                  FALSE: directory is another dir
 * 
 **/
void wb_project_dir_set_is_prj_base_dir (WB_PROJECT_DIR *directory, gboolean value)
{
	if (directory != NULL)
	{
		directory->is_prj_base_dir = value;
	}
}


/** Get "is project base dir" of a project dir.
 *
 * @param directory The project dir
 * @return TRUE:  directory is the base dir of the project
 *         FALSE: directory is another dir
 *
 **/
gboolean wb_project_dir_get_is_prj_base_dir (WB_PROJECT_DIR *directory)
{
	if (directory != NULL)
	{
		return directory->is_prj_base_dir;
	}
	return FALSE;
}

/** Get the name of a project dir.
 *
 * @param directory The project dir
 * @return The name
 *
 **/
const gchar *wb_project_dir_get_name (WB_PROJECT_DIR *directory)
{
	if (directory != NULL)
	{
		return directory->name;
	}
	return NULL;
}


/** Get the file table for the project dir.
 *
 * @param directory The project dir
 * @return A GHashTable of all files containing to the rpoject dir.
 *         Might be empty if dir has not been scanned yet.
 *
 **/
GHashTable *wb_project_dir_get_file_table (WB_PROJECT_DIR *directory)
{
	if (directory != NULL)
	{
		return directory->file_table;
	}
	return NULL;
}


/** Get the base/root dir of a project dir.
 *
 * @param directory The project dir
 * @return The base dir
 *
 **/
gchar *wb_project_dir_get_base_dir (WB_PROJECT_DIR *directory)
{
	if (directory != NULL)
	{
		return directory->base_dir;
	}
	return NULL;
}


/** Get the file patterns of a project dir.
 *
 * @param directory The project dir
 * @return String array of file patterns
 *
 **/
gchar **wb_project_dir_get_file_patterns (WB_PROJECT_DIR *directory)
{
	if (directory != NULL)
	{
		return directory->file_patterns;
	}
	return NULL;
}


/** Set the file patterns of a project dir.
 *
 * @param directory The project dir
 * @param new       String array of file patterns to set
 * @return FALSE if directory is NULL, TRUE otherwise
 *
 **/
gboolean wb_project_dir_set_file_patterns (WB_PROJECT_DIR *directory, gchar **new)
{
	if (directory != NULL)
	{
		g_strfreev(directory->file_patterns);
		directory->file_patterns = g_strdupv(new);
		return TRUE;
	}
	return FALSE;
}


/** Get the ignored dirs patterns of a project dir.
 *
 * @param directory The project dir
 * @return String array of ignored dirs patterns
 *
 **/
gchar **wb_project_dir_get_ignored_dirs_patterns (WB_PROJECT_DIR *directory)
{
	if (directory != NULL)
	{
		return directory->ignored_dirs_patterns;
	}
	return NULL;
}


/** Set the ignored dirs patterns of a project dir.
 *
 * @param directory The project dir
 * @param new       String array of ignored dirs patterns to set
 * @return FALSE if directory is NULL, TRUE otherwise
 *
 **/
gboolean wb_project_dir_set_ignored_dirs_patterns (WB_PROJECT_DIR *directory, gchar **new)
{
	if (directory != NULL)
	{
		g_strfreev(directory->ignored_dirs_patterns);
		directory->ignored_dirs_patterns = g_strdupv(new);
		return TRUE;
	}
	return FALSE;
}


/** Get the ignored file patterns of a project dir.
 *
 * @param directory The project dir
 * @return String array of ignored file patterns
 *
 **/
gchar **wb_project_dir_get_ignored_file_patterns (WB_PROJECT_DIR *directory)
{
	if (directory != NULL)
	{
		return directory->ignored_file_patterns;
	}
	return NULL;
}


/** Set the ignored file patterns of a project dir.
 *
 * @param directory The project dir
 * @param new       String array of ignored dirs patterns to set
 * @return FALSE if directory is NULL, TRUE otherwise
 *
 **/
gboolean wb_project_dir_set_ignored_file_patterns (WB_PROJECT_DIR *directory, gchar **new)
{
	if (directory != NULL)
	{
		g_strfreev(directory->ignored_file_patterns);
		directory->ignored_file_patterns = g_strdupv(new);
		return TRUE;
	}
	return FALSE;
}


/* Remove all files contained in the project dir from the tm-workspace */
static void wb_project_dir_remove_from_tm_workspace(WB_PROJECT_DIR *root)
{
	GPtrArray *source_files;

	source_files = g_ptr_array_new();
	g_hash_table_foreach(root->file_table, (GHFunc)wb_project_dir_collect_source_files, source_files);
	tm_workspace_remove_source_files(source_files);
	g_ptr_array_free(source_files, TRUE);
}


/* Free a project dir */
static void wb_project_dir_free(WB_PROJECT_DIR *dir)
{
	wb_project_dir_remove_from_tm_workspace(dir);

	g_hash_table_destroy(dir->file_table);
	g_free(dir->base_dir);
	g_free(dir);
}


/* Compare two project dirs */
static gint wb_project_dir_comparator(WB_PROJECT_DIR *a, WB_PROJECT_DIR *b)
{
	gchar *a_realpath, *b_realpath, *a_locale_base_dir, *b_locale_base_dir;
	gint res;

	a_locale_base_dir = utils_get_locale_from_utf8(a->base_dir);
	b_locale_base_dir = utils_get_locale_from_utf8(b->base_dir);
	a_realpath = utils_get_real_path(a_locale_base_dir);
	b_realpath = utils_get_real_path(b_locale_base_dir);

	res = g_strcmp0(a_realpath, b_realpath);

	g_free(a_realpath);
	g_free(b_realpath);
	g_free(a_locale_base_dir);
	g_free(b_locale_base_dir);

	return res;
}


/* Get the file count of a project */
static guint wb_project_get_file_count(WB_PROJECT *prj)
{
	GSList *elem = NULL;
	guint filenum = 0;

	foreach_slist(elem, prj->directories)
	{
		filenum += ((WB_PROJECT_DIR *)elem->data)->file_count;
	}
	return filenum;
}

/* Rescan/update the file list of a project dir. */
static guint wb_project_dir_rescan_int(WB_PROJECT *prj, WB_PROJECT_DIR *root)
{
	GSList *lst;
	GSList *elem = NULL;
	guint filenum = 0;
	gchar *searchdir;
	gchar **file_patterns = NULL;

	wb_project_dir_remove_from_tm_workspace(root);
	g_hash_table_remove_all(root->file_table);

	if (root->file_patterns && root->file_patterns[0])
	{
		file_patterns = root->file_patterns;
	}

	searchdir = get_combined_path(prj->filename, root->base_dir);
	root->file_count = 0;
	root->subdir_count = 0;
	lst = gp_filelist_scan_directory_full(&(root->file_count), &(root->subdir_count),
		searchdir, file_patterns, root->ignored_dirs_patterns, root->ignored_file_patterns,
		FILELIST_FLAG_ADD_DIRS);
	g_free(searchdir);

	foreach_slist(elem, lst)
	{
		char *path = elem->data;

		if (path)
		{
			g_hash_table_insert(root->file_table, g_strdup(path), NULL);
			filenum++;
		}
	}

	g_slist_foreach(lst, (GFunc) g_free, NULL);
	g_slist_free(lst);

	return filenum;
}


/* Add a new file to the project directory and update the sidebar. */
#ifdef __WB_LIVE_UPDATE
static void wb_project_dir_add_file_int(WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath)
{
	gboolean matches;
	gchar **file_patterns = NULL;
	SIDEBAR_CONTEXT context;
	WB_MONITOR *monitor = NULL;

	if (root->file_patterns && root->file_patterns[0])
	{
		file_patterns = root->file_patterns;
	}

	matches = gp_filelist_filepath_matches_patterns(filepath,
		file_patterns, root->ignored_dirs_patterns, root->ignored_file_patterns);
	if (!matches)
	{
		/* Ignore it. */
		return;
	}

	/* Update file table and counters. */
	g_hash_table_insert(root->file_table, g_strdup(filepath), NULL);
	if (g_file_test(filepath, G_FILE_TEST_IS_DIR))
	{
		root->subdir_count++;
		monitor = workbench_get_monitor(wb_globals.opened_wb);
		wb_monitor_add_dir(monitor, prj, root, filepath);
	}
	else if (g_file_test(filepath, G_FILE_TEST_IS_REGULAR))
	{
		root->file_count++;
	}

	/* Update sidebar. */
	memset(&context, 0, sizeof(context));
	context.project = prj;
	context.directory = root;
	context.file = (gchar *)filepath;
	sidebar_update(SIDEBAR_CONTEXT_FILE_ADDED, &context);

	/* If the file is a directory we also have to manually add all files
	   contained in it. */
	if (monitor != NULL)
	{
		GSList *scanned, *elem = NULL;

		scanned = gp_filelist_scan_directory_full(&(root->file_count), &(root->subdir_count),
			filepath, file_patterns, root->ignored_dirs_patterns, root->ignored_file_patterns,
			FILELIST_FLAG_ADD_DIRS);

		foreach_slist(elem, scanned)
		{
			char *path = elem->data;

			if (path)
			{
				wb_project_dir_add_file(prj, root, path);
			}
		}

		g_slist_foreach(scanned, (GFunc) g_free, NULL);
		g_slist_free(scanned);
	}
}
#endif


/** Add a new file to the project directory and update the sidebar.
 * 
 * The file is only added if it matches the pattern settings.
 *
 * @param prj      The project to add it to.
 * @param root     The directory to add it to.
 * @param filepath The file to add.
 *
 **/
#ifdef __WB_LIVE_UPDATE
void wb_project_dir_add_file(WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath)
{
	wb_project_dir_add_file_int(prj, root, filepath);
	wb_project_add_idle_action(WB_PROJECT_IDLE_ACTION_ID_UPDATE_TAGS,
		root, NULL);

}
#endif


/* Check if the filepath is equal for the length of the directory path in px_temp */
#ifdef __WB_LIVE_UPDATE
static gboolean wb_project_dir_remove_child (gpointer key, gpointer value, gpointer user_data)
{
	WB_PROJECT_TEMP_DATA *px_temp;
	TMSourceFile *sf;

	px_temp = user_data;
	if (strncmp(px_temp->string, key, px_temp->len) == 0)
	{
		/* We found a child of our removed directory.
		   Remove it from the hash table. This will also free
		   the tags. We do not need to update the sidebar as we
		   already deleted the parent directory/node. */
		sf = value;
		if (sf != NULL)
		{
			tm_workspace_remove_source_file(sf);
		}
		return TRUE;
	}
	return FALSE;
}
#endif


/** Remove a file from the project directory and update the sidebar.
 * 
 * If the file still exists, it is only removed if it matches the pattern settings.
 *
 * @param prj      The project to remove it from.
 * @param root     The directory to remove it from.
 * @param filepath The file to remove.
 *
 **/
#ifdef __WB_LIVE_UPDATE
void wb_project_dir_remove_file(WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath)
{
	gboolean matches, was_dir;
	gchar **file_patterns = NULL;
	WB_MONITOR *monitor;

	if (root->file_patterns && root->file_patterns[0])
	{
		file_patterns = root->file_patterns;
	}

	if (g_file_test(filepath, G_FILE_TEST_EXISTS))
	{
		matches = gp_filelist_filepath_matches_patterns(filepath,
			file_patterns, root->ignored_dirs_patterns, root->ignored_file_patterns);
	}
	else
	{
		/* If the file does not exist any more, then always try to remove it. */
		matches = TRUE;
	}

	if (matches)
	{
		SIDEBAR_CONTEXT context;
		TMSourceFile *sf;

		/* Update file table and counters. */
		sf = g_hash_table_lookup (root->file_table, filepath);
		if (sf != NULL)
		{
			tm_workspace_remove_source_file(sf);
		}
		g_hash_table_remove(root->file_table, filepath);

		/* If the file already has been deleted, we cannot determine if it
		   was a file or directory at this point. But the monitors will
		   help us out, see code at end of function. */

		/* Update sidebar. */
		memset(&context, 0, sizeof(context));
		context.project = prj;
		context.directory = root;
		context.file = (gchar *)filepath;
		sidebar_update(SIDEBAR_CONTEXT_FILE_REMOVED, &context);
	}

	/* Remove the file monitor for filepath. This will only return TRUE
	   if there is a file monitor for filepath and that means that file-
	   path was a directory. So we can determine if filepath was a dir
	   or not even if it has been deleted. */
	monitor = workbench_get_monitor(wb_globals.opened_wb);
	was_dir = wb_monitor_remove_dir(monitor, filepath);
	if (was_dir)
	{
		WB_PROJECT_TEMP_DATA x_temp;

		x_temp.len = strlen(filepath);
		x_temp.string = filepath;
		g_hash_table_foreach_remove(root->file_table,
			wb_project_dir_remove_child, &x_temp);

		if (root->subdir_count > 0)
		{
			root->subdir_count--;
		}
	}
	else
	{
		if (root->file_count > 0)
		{
			root->file_count--;
		}
	}
}
#endif


/* Stolen and modified version from Geany. The only difference is that Geany
 * first looks at shebang inside the file and then, if it fails, checks the
 * file extension. Opening every file is too expensive so instead check just
 * extension and only if this fails, look at the shebang */
static GeanyFiletype *filetypes_detect(const gchar *utf8_filename)
{
	struct stat s;
	GeanyFiletype *ft = NULL;
	gchar *locale_filename;

	locale_filename = utils_get_locale_from_utf8(utf8_filename);
	if (g_stat(locale_filename, &s) != 0 || s.st_size > 10*1024*1024)
		ft = filetypes[GEANY_FILETYPES_NONE];
	else
	{
		guint i;
		gchar *utf8_base_filename;

		/* to match against the basename of the file (because of Makefile*) */
		utf8_base_filename = g_path_get_basename(utf8_filename);
#ifdef G_OS_WIN32
		/* use lower case basename */
		SETPTR(utf8_base_filename, g_utf8_strdown(utf8_base_filename, -1));
#endif

		for (i = 0; i < geany_data->filetypes_array->len; i++)
		{
			GeanyFiletype *ftype = filetypes[i];

			if (match_basename(ftype, utf8_base_filename))
			{
				ft = ftype;
				break;
			}
		}

		if (ft == NULL)
			ft = filetypes_detect_from_file(utf8_filename);

		g_free(utf8_base_filename);
	}

	g_free(locale_filename);

	return ft;
}


/* Regenerate tags */
static void wb_project_dir_regenerate_tags(WB_PROJECT_DIR *root, G_GNUC_UNUSED gpointer user_data)
{
	GHashTableIter iter;
	gpointer key, value;
	GPtrArray *source_files;
	GHashTable *file_table;

	source_files = g_ptr_array_new();
	file_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GFreeFunc)tm_source_file_free);
	g_hash_table_iter_init(&iter, root->file_table);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		TMSourceFile *sf;
		gchar *utf8_path = key;
		gchar *locale_path = utils_get_locale_from_utf8(utf8_path);

		sf = tm_source_file_new(locale_path, filetypes_detect(utf8_path)->name);
		if (sf && !document_find_by_filename(utf8_path))
			g_ptr_array_add(source_files, sf);

		g_hash_table_insert(file_table, g_strdup(utf8_path), sf);
		g_free(locale_path);
	}
	g_hash_table_destroy(root->file_table);
	root->file_table = file_table;

	tm_workspace_add_source_files(source_files);
	g_ptr_array_free(source_files, TRUE);
}


/* Update tags for new files */
static void wb_project_dir_update_tags(WB_PROJECT_DIR *root)
{
	GHashTableIter iter;
	gpointer key, value;
	GPtrArray *source_files;

	source_files = g_ptr_array_new();
	g_hash_table_iter_init(&iter, root->file_table);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		if (value == NULL)
		{
			TMSourceFile *sf;
			gchar *utf8_path = key;
			gchar *locale_path = utils_get_locale_from_utf8(utf8_path);

			sf = tm_source_file_new(locale_path, filetypes_detect(utf8_path)->name);
			if (sf && !document_find_by_filename(utf8_path))
				g_ptr_array_add(source_files, sf);

			g_hash_table_insert(root->file_table, g_strdup(utf8_path), sf);
			g_free(locale_path);
		}
	}

	tm_workspace_add_source_files(source_files);
	g_ptr_array_free(source_files, TRUE);
}


/** Rescan/update the file list of a project dir.
 *
 * @param project   The project to which directory belongs
 * @param directory The project dir
 * @return Number of files
 *
 **/
guint wb_project_dir_rescan(WB_PROJECT *prj, WB_PROJECT_DIR *root)
{
	guint total, filenum;

	filenum = wb_project_dir_rescan_int(prj, root);
	total = wb_project_get_file_count(prj);
	if (prj->generate_tag_prefs == WB_PROJECT_TAG_PREFS_YES || (prj->generate_tag_prefs == WB_PROJECT_TAG_PREFS_AUTO && total < 300))
	{
		wb_project_dir_regenerate_tags(root, NULL);
	}
	return filenum;
}


/** Rescan/update the file list of a project.
 *
 * @param project   The project
 *
 **/
void wb_project_rescan(WB_PROJECT *prj)
{
	GSList *elem = NULL;
	guint filenum = 0;
	GHashTableIter iter;

	if (!prj)
	{
		return;
	}

	wb_project_clear_idle_queue();

	foreach_slist(elem, prj->directories)
	{
		filenum += wb_project_dir_rescan_int(prj, elem->data);
	}

	if (prj->generate_tag_prefs == WB_PROJECT_TAG_PREFS_YES || (prj->generate_tag_prefs == WB_PROJECT_TAG_PREFS_AUTO && filenum < 300))
	{
		g_slist_foreach(prj->directories, (GFunc)wb_project_dir_regenerate_tags, NULL);
	}

	/* Create file monitors for directories. */
#ifdef __WB_LIVE_UPDATE
	if (workbench_get_enable_live_update(wb_globals.opened_wb) == TRUE)
	{
		WB_MONITOR *monitor;

		monitor = workbench_get_monitor(wb_globals.opened_wb);
		foreach_slist(elem, prj->directories)
		{
			gpointer path, value;
			GHashTable *filehash;
			gchar *abs_path;

			/* First add monitor for base dir */
			abs_path = get_combined_path(wb_project_get_filename(prj),
										wb_project_dir_get_base_dir(elem->data));
			wb_monitor_add_dir(monitor, prj, elem->data, abs_path);
			g_free(abs_path);

			/* Now add all dirs in file table */
			filehash = ((WB_PROJECT_DIR *)elem->data)->file_table;
			g_hash_table_iter_init(&iter, filehash);
			while (g_hash_table_iter_next (&iter, &path, &value))
			{
				if (path != NULL && g_file_test(path, G_FILE_TEST_IS_DIR))
				{
					wb_monitor_add_dir(monitor, prj, elem->data, path);
				}
			}
		}
	}
#endif
}


/** Is @a filename included in the project directory?
 *
 * @param dir      The project directory
 * @param filename The file
 * @return TRUE if file is included, FALSE otherwise
 *
 **/
gboolean wb_project_dir_file_is_included(WB_PROJECT_DIR *dir, const gchar *filename)
{
	if (filename == NULL || dir == NULL)
	{
		return FALSE;
	}

	if (g_hash_table_lookup_extended (dir->file_table, filename, NULL, NULL))
	{
		return TRUE;
	}

	return FALSE;
}


/** Is @a filename included in the project?
 *
 * @param dir      The project
 * @param filename The file
 * @return TRUE if file is included, FALSE otherwise
 *
 **/
gboolean wb_project_file_is_included(WB_PROJECT *prj, const gchar *filename)
{
	GSList *elem = NULL;

	if (prj == NULL)
	{
		return FALSE;
	}

	foreach_slist(elem, prj->directories)
	{
		if (wb_project_dir_file_is_included(elem->data, filename) == TRUE)
		{
			return TRUE;
		}
	}
	return FALSE;
}


/* Add single tm file. Only to be called on-idle! */
static void wb_project_add_single_tm_file(WB_PROJECT *prj, const gchar *filename)
{
	GSList *elem = NULL;

	foreach_slist (elem, prj->directories)
	{
		WB_PROJECT_DIR *dir = elem->data;
		TMSourceFile *sf = g_hash_table_lookup(dir->file_table, filename);

		if (sf != NULL && !document_find_by_filename(filename))
		{
			tm_workspace_add_source_file(sf);
			break;  /* single file representation in TM is enough */
		}
	}
}


/* This function gets called when document is being opened by Geany and we need
 * to remove the TMSourceFile from the tag manager because Geany inserts
 * it for the newly open tab. Even though tag manager would handle two identical
 * files, the file inserted by the plugin isn't updated automatically in TM
 * so any changes wouldn't be reflected in the tags array (e.g. removed function
 * from source file would still be found in TM)
 *
 * Additional problem: The document being opened may be caused
 * by going to tag definition/declaration - tag processing is in progress
 * when this function is called and if we remove the TmSourceFile now, line
 * number for the searched tag won't be found. For this reason delay the tag
 * TmSourceFile removal until idle */
static void wb_project_remove_single_tm_file(WB_PROJECT *prj, const gchar *filename)
{
	GSList *elem = NULL;

	foreach_slist (elem, prj->directories)
	{
		WB_PROJECT_DIR *dir = elem->data;
		TMSourceFile *sf = g_hash_table_lookup(dir->file_table, filename);

		if (sf != NULL)
		{
			tm_workspace_remove_source_file(sf);
		}
	}
}


/* On-idle callback function. */
static gboolean wb_project_on_idle_callback(gpointer foo)
{
	GSList *elem = NULL;
	WB_PROJECT_IDLE_ACTION *action;

	foreach_slist (elem, s_idle_actions)
	{
		action = elem->data;
		switch (action->id)
		{
			case WB_PROJECT_IDLE_ACTION_ID_ADD_SINGLE_TM_FILE:
				if (action->param_a != NULL && action->param_b != NULL)
				{
					wb_project_add_single_tm_file
						(action->param_a, action->param_b);
					g_free(action->param_b);
				}
			break;

			case WB_PROJECT_IDLE_ACTION_ID_REMOVE_SINGLE_TM_FILE:
				if (action->param_a != NULL && action->param_b != NULL)
				{
					wb_project_remove_single_tm_file
						(action->param_a, action->param_b);
					g_free(action->param_b);
				}
			break;

			case WB_PROJECT_IDLE_ACTION_ID_UPDATE_TAGS:
				if (action->param_a != NULL)
				{
					wb_project_dir_update_tags(action->param_a);
				}
			break;
		}
	}

	wb_project_clear_idle_queue();

	return FALSE;
}


/** Add a new idle action to the list.
 *
 * The function allocates a new WB_PROJECT_IDLE_ACTION structure and fills
 * in the values passed. On-idle genay will then call wb_project_on_idle_callback
 * and that function will call the function related to the action ID
 * and pass the relevant parameters to it.
 * 
 * @param id The action to execute on-idle
 * @param param_a Parameter A
 * @param param_a Parameter B
 *
 **/
void wb_project_add_idle_action(WB_PROJECT_IDLE_ACTION_ID id, gpointer param_a, gpointer param_b)
{
	WB_PROJECT_IDLE_ACTION *action;

	action = g_new0(WB_PROJECT_IDLE_ACTION, 1);
	action->id = id;
	action->param_a = param_a;
	action->param_b = param_b;

	if (s_idle_actions == NULL)
	{
		plugin_idle_add(wb_globals.geany_plugin, (GSourceFunc)wb_project_on_idle_callback, NULL);
	}

	s_idle_actions = g_slist_prepend(s_idle_actions, action);
}


/* Add a directory to the project */
static WB_PROJECT_DIR *wb_project_add_directory_int(WB_PROJECT *prj, const gchar *dirname, gboolean rescan)
{
	if (prj != NULL)
	{
		WB_PROJECT_DIR *new_dir = wb_project_dir_new(prj, dirname);

		if (prj->directories != NULL)
		{
			GSList *lst = prj->directories->next;
			lst = g_slist_prepend(lst, new_dir);
			lst = g_slist_sort(lst, (GCompareFunc)wb_project_dir_comparator);
			prj->directories->next = lst;
		}
		else
		{
			prj->directories = g_slist_append(prj->directories, new_dir);
		}

		if (rescan)
		{
			wb_project_rescan(prj);
		}
		return new_dir;
	}
	return NULL;
}


/** Adds a new project dir to the project.
 *
 * @param project The project
 * @param dirname The base dir of the new project dir
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean wb_project_add_directory(WB_PROJECT *prj, const gchar *dirname)
{
	gchar *reldirname;

	/* Convert dirname to path relative to the project file */
	reldirname = get_any_relative_path(prj->filename, dirname);

	if (wb_project_add_directory_int(prj, reldirname, TRUE) != NULL)
	{
		prj->modified = TRUE;
		return TRUE;
	}

	g_free(reldirname);
	return FALSE;
}


/** Remove a project dir from the project.
 *
 * @param project The project
 * @param dir     The project dir to remove
 * @return FALSE on NULL parameter, TRUE otherwise
 *
 **/
gboolean wb_project_remove_directory (WB_PROJECT *prj, WB_PROJECT_DIR *dir)
{
	if (prj != NULL && dir != NULL)
	{
		prj->directories = g_slist_remove(prj->directories, dir);
		wb_project_dir_free(dir);
		wb_project_rescan(prj);
		prj->modified = TRUE;
	}
	return FALSE;
}


/** Get an info string for the project dir.
 *
 * @param dir The project dir
 * @return The info string
 *
 **/
gchar *wb_project_dir_get_info (WB_PROJECT_DIR *dir)
{
	gchar *str;

	if (dir == NULL)
		return g_strdup("");

	GString *temp = g_string_new(NULL);
	gchar *text;
	g_string_append_printf(temp, _("Directory-Name: %s\n"), wb_project_dir_get_name(dir));
	g_string_append_printf(temp, _("Base-Directory: %s\n"), wb_project_dir_get_base_dir(dir));

	g_string_append(temp, _("File Patterns:"));
	str = g_strjoinv(" ", dir->file_patterns);
	if (str != NULL )
	{
		g_string_append_printf(temp, " %s\n", str);
		g_free(str);
	}
	else
	{
		g_string_append(temp, "\n");
	}

	g_string_append(temp, _("Ignored Dir. Patterns:"));
	str = g_strjoinv(" ", dir->ignored_dirs_patterns);
	if (str != NULL )
	{
		g_string_append_printf(temp, " %s\n", str);
		g_free(str);
	}
	else
	{
		g_string_append(temp, "\n");
	}

	g_string_append(temp, _("Ignored File Patterns:"));
	str = g_strjoinv(" ", dir->ignored_file_patterns);
	if (str != NULL )
	{
		g_string_append_printf(temp, " %s\n", str);
		g_free(str);
	}
	else
	{
		g_string_append(temp, "\n");
	}

	g_string_append_printf(temp, _("Number of Sub-Directories: %u\n"), dir->subdir_count);
	g_string_append_printf(temp, _("Number of Files: %u\n"), dir->file_count);

	/* Steal string content */
	text = temp->str;
	g_string_free (temp, FALSE);

	return text;
}


/** Get an info string for the project.
 *
 * @param prj The project
 * @return The info string
 *
 **/
gchar *wb_project_get_info (WB_PROJECT *prj)
{
	GString *temp = g_string_new(NULL);
	gchar *text;

	if (prj == NULL)
		return g_strdup("");

	g_string_append_printf(temp, _("Project: %s\n"), wb_project_get_name(prj));
	g_string_append_printf(temp, _("File: %s\n"), wb_project_get_filename(prj));
	g_string_append_printf(temp, _("Number of Directories: %u\n"), g_slist_length(prj->directories));
	if (wb_project_is_modified(prj))
	{
		g_string_append(temp, _("\nThe project contains unsaved changes!\n"));
	}

	/* Steal string content */
	text = temp->str;
	g_string_free (temp, FALSE);

	return text;
}


/* Save directories to key file */
static void wb_project_save_directories (gpointer data, gpointer user_data)
{
	gchar key[250], *str;
	WB_PROJECT_DIR *dir;
	WB_PROJECT_ON_SAVE_USER_DATA *tmp;

	if (data == NULL || user_data == NULL)
	{
		return;
	}
	tmp = (WB_PROJECT_ON_SAVE_USER_DATA *)user_data;
	dir = (WB_PROJECT_DIR *)data;

	if (wb_project_dir_get_is_prj_base_dir(dir) == TRUE)
	{
		g_key_file_set_string(tmp->kf, "Workbench", "Prj-BaseDir", dir->base_dir);

		str = g_strjoinv(";", dir->file_patterns);
		g_key_file_set_string(tmp->kf, "Workbench", "Prj-FilePatterns", str);
		g_free(str);

		str = g_strjoinv(";", dir->ignored_dirs_patterns);
		g_key_file_set_string(tmp->kf, "Workbench", "Prj-IgnoredDirsPatterns", str);
		g_free(str);

		str = g_strjoinv(";", dir->ignored_file_patterns);
		g_key_file_set_string(tmp->kf, "Workbench", "Prj-IgnoredFilePatterns", str);
		g_free(str);
	}
	else
	{
		g_snprintf(key, sizeof(key), "Dir%u-BaseDir", tmp->dir_count);
		g_key_file_set_string(tmp->kf, "Workbench", key, dir->base_dir);

		g_snprintf(key, sizeof(key), "Dir%u-FilePatterns", tmp->dir_count);
		str = g_strjoinv(";", dir->file_patterns);
		g_key_file_set_string(tmp->kf, "Workbench", key, str);
		g_free(str);

		g_snprintf(key, sizeof(key), "Dir%u-IgnoredDirsPatterns", tmp->dir_count);
		str = g_strjoinv(";", dir->ignored_dirs_patterns);
		g_key_file_set_string(tmp->kf, "Workbench", key, str);
		g_free(str);

		g_snprintf(key, sizeof(key), "Dir%u-IgnoredFilePatterns", tmp->dir_count);
		str = g_strjoinv(";", dir->ignored_file_patterns);
		g_key_file_set_string(tmp->kf, "Workbench", key, str);
		g_free(str);

		tmp->dir_count++;
	}
}


/* Add a bookmark to the project */
static gboolean wb_project_add_bookmark_int(WB_PROJECT *prj, const gchar *filename)
{
	if (prj != NULL)
	{
		gchar *new;

		new = g_strdup(filename);
		if (new != NULL)
		{
			g_ptr_array_add (prj->bookmarks, new);
			return TRUE;
		}
	}
	return FALSE;
}


/** Add a bookmark to the project.
 *
 * @param prj      The project
 * @param filename Bookmark
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean wb_project_add_bookmark(WB_PROJECT *prj, const gchar *filename)
{
	if (wb_project_add_bookmark_int(prj, filename) == TRUE)
	{
		prj->modified = TRUE;
		return TRUE;
	}
	return FALSE;
}


/** Remove a bookmark from the project.
 *
 * @param prj      The project
 * @param filename Bookmark
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean wb_project_remove_bookmark(WB_PROJECT *prj, const gchar *filename)
{
	if (prj != NULL)
	{
		guint index;
		gchar *current;

		for (index = 0 ; index < prj->bookmarks->len ; index++)
		{
			current = g_ptr_array_index(prj->bookmarks, index);
			if (current == filename)
			{
				g_ptr_array_remove_index (prj->bookmarks, index);
				prj->modified = TRUE;
				return TRUE;
			}
		}
	}
	return FALSE;
}


/* Free all bookmarks */
static gboolean wb_project_free_all_bookmarks(WB_PROJECT *prj)
{
	if (prj != NULL)
	{
		guint index;
		gchar *current;

		for (index = 0 ; index < prj->bookmarks->len ; index++)
		{
			current = g_ptr_array_index(prj->bookmarks, index);
			g_free(current);
		}
		g_ptr_array_free(prj->bookmarks, TRUE);
	}
	return FALSE;
}


/** Get the bookmark of a project at index @a index.
 *
 * @param prj   The project
 * @param index The index
 * @return The filename of the boomark (or NULL if prj is NULL)
 *
 **/
gchar *wb_project_get_bookmark_at_index (WB_PROJECT *prj, guint index)
{
	if (prj != NULL)
	{
		gchar *file;
		file = g_ptr_array_index(prj->bookmarks, index);
		if (file == NULL)
		{
			return NULL;
		}
		return file;
	}
	return NULL;
}


/** Get the number of bookmarks of a project.
 *
 * @param prj   The project
 * @return The number of bookmarks
 *
 **/
guint wb_project_get_bookmarks_count(WB_PROJECT *prj)
{
	if (prj != NULL && prj->bookmarks != NULL)
	{
		return prj->bookmarks->len;
	}
	return 0;
}


/** Save a project.
 *
 * The function saves the project config file. Only the workbench plugin info
 * is changed, all other config options remain unchanged.
 *
 * @param prj   The project
 * @param error Location to store error info at
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean wb_project_save(WB_PROJECT *prj, GError **error)
{
	GKeyFile *kf;
	guint    index;
	gchar    *contents;
	gsize    length, boomarks_size;
	gboolean success = FALSE;
	WB_PROJECT_ON_SAVE_USER_DATA tmp;

	g_return_val_if_fail(prj, FALSE);

	/* Load existing data into GKeyFile */
	kf = g_key_file_new ();
	if (!g_key_file_load_from_file(kf, prj->filename, G_KEY_FILE_NONE, error))
	{
		return FALSE;
	}

	/* Remove existing, old data from our plugin */
	g_key_file_remove_group (kf, "Workbench", NULL);

	/* Save Project bookmarks as string list */
	boomarks_size = wb_project_get_bookmarks_count(prj);
	if (boomarks_size > 0)
	{
		gchar **bookmarks_strings, *file, *rel_path;

		bookmarks_strings = g_new0(gchar *, boomarks_size+1);
		for (index = 0 ; index < boomarks_size ; index++ )
		{
			file = wb_project_get_bookmark_at_index(prj, index);
			rel_path = get_any_relative_path(prj->filename, file);

			bookmarks_strings[index] = rel_path;
		}
		g_key_file_set_string_list
			(kf, "Workbench", "Bookmarks", (const gchar **)bookmarks_strings, boomarks_size);
		for (index = 0 ; index < boomarks_size ; index++ )
		{
			g_free (bookmarks_strings[index]);
		}
		g_free(bookmarks_strings);
	}

	/* Init tmp data */
	tmp.kf = kf;
	tmp.dir_count = 1;

	/* Store our directories */
	g_slist_foreach(prj->directories, (GFunc)wb_project_save_directories, &tmp);

	/* Get data as string */
	contents = g_key_file_to_data (kf, &length, error);
	g_key_file_free(kf);

	/* Save to file */
	success = g_file_set_contents (prj->filename, contents, length, error);
	if (success)
	{
		prj->modified = FALSE;
	}
	g_free (contents);

	return success;
}


/** Load a project.
 *
 * The function loads the project data from file @a filename into the
 * project structure @a prj.
 *
 * @param prj      The project
 * @param filename File to load
 * @param error    Location to store error info at
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean wb_project_load(WB_PROJECT *prj, gchar *filename, GError **error)
{
	GKeyFile *kf;
	guint	 index;
	gchar	 *contents, *str;
	gchar	 **splitv;
	gchar	 key[100];
	gsize	 length;
	gboolean success = FALSE;
	WB_PROJECT_DIR *new_dir;

	g_return_val_if_fail(prj, FALSE);

	if (!g_file_get_contents (filename, &contents, &length, error))
	{
		return FALSE;
	}

	kf = g_key_file_new ();

	if (!g_key_file_load_from_data (kf, contents, length,
				G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
				error))
	{
		g_key_file_free (kf);
		g_free (contents);
		return FALSE;
	}

	/* Import project's base path and file patterns, if not done yet.
	   (from Geany's standard project configuration) */
	if (g_key_file_has_group (kf, "project")
		&& !g_key_file_has_key(kf, "Workbench", "Prj-BaseDir", NULL))
	{
		gchar *base_path;

		base_path = g_key_file_get_string(kf, "project", "base_path", NULL);
		if (base_path != NULL)
		{
			gchar *reldirname;

			/* Convert dirname to path relative to the project file */
			reldirname = get_any_relative_path(prj->filename, base_path);

			new_dir = wb_project_add_directory_int(prj, reldirname, FALSE);
			if (new_dir != NULL)
			{
				wb_project_set_modified(prj, TRUE);
				wb_project_dir_set_is_prj_base_dir(new_dir, TRUE);
				str = g_key_file_get_string(kf, "project", "file_patterns", NULL);
				if (str != NULL)
				{
					splitv = g_strsplit (str, ";", -1);
					wb_project_dir_set_file_patterns(new_dir, splitv);
					g_strfreev(splitv);
				}
				g_free(str);
			}

			g_free(reldirname);
			g_free(base_path);
		}
	}

	if (g_key_file_has_group (kf, "Workbench"))
	{
		gchar **bookmarks_strings;

		/* Load project bookmarks from string list */
		bookmarks_strings = g_key_file_get_string_list (kf, "Workbench", "Bookmarks", NULL, error);
		if (bookmarks_strings != NULL)
		{
			gchar **file, *abs_path;

			file = bookmarks_strings;
			while (*file != NULL)
			{
				abs_path = get_combined_path(prj->filename, *file);
				if (abs_path != NULL)
				{
					wb_project_add_bookmark_int(prj, abs_path);
					g_free(abs_path);
				}
				file++;
			}
			g_strfreev(bookmarks_strings);
		}

		/* Load project base dir. */
		str = g_key_file_get_string(kf, "Workbench", "Prj-BaseDir", NULL);
		if (str != NULL)
		{
			new_dir = wb_project_add_directory_int(prj, str, FALSE);
			if (new_dir != NULL)
			{
				wb_project_dir_set_is_prj_base_dir(new_dir, TRUE);

				str = g_key_file_get_string(kf, "Workbench", "Prj-FilePatterns", NULL);
				if (str != NULL)
				{
					splitv = g_strsplit (str, ";", -1);
					wb_project_dir_set_file_patterns(new_dir, splitv);
				}
				g_free(str);

				str = g_key_file_get_string(kf, "Workbench", "Prj-IgnoredDirsPatterns", NULL);
				if (str != NULL)
				{
					splitv = g_strsplit (str, ";", -1);
					wb_project_dir_set_ignored_dirs_patterns(new_dir, splitv);
				}
				g_free(str);

				str = g_key_file_get_string(kf, "Workbench", "Prj-IgnoredFilePatterns", NULL);
				if (str != NULL)
				{
					splitv = g_strsplit (str, ";", -1);
					wb_project_dir_set_ignored_file_patterns(new_dir, splitv);
				}
				g_free(str);
			}
		}

		/* Load project dirs */
		for (index = 1 ; index < 1025 ; index++)
		{
			g_snprintf(key, sizeof(key), "Dir%u-BaseDir", index);

			str = g_key_file_get_string(kf, "Workbench", key, NULL);
			if (str == NULL)
			{
				break;
			}
			new_dir = wb_project_add_directory_int(prj, str, FALSE);
			if (new_dir == NULL)
			{
				break;
			}

			g_snprintf(key, sizeof(key), "Dir%u-FilePatterns", index);
			str = g_key_file_get_string(kf, "Workbench", key, NULL);
			if (str != NULL)
			{
				splitv = g_strsplit (str, ";", -1);
				wb_project_dir_set_file_patterns(new_dir, splitv);
			}
			g_free(str);

			g_snprintf(key, sizeof(key), "Dir%u-IgnoredDirsPatterns", index);
			str = g_key_file_get_string(kf, "Workbench", key, NULL);
			if (str != NULL)
			{
				splitv = g_strsplit (str, ";", -1);
				wb_project_dir_set_ignored_dirs_patterns(new_dir, splitv);
			}
			g_free(str);

			g_snprintf(key, sizeof(key), "Dir%u-IgnoredFilePatterns", index);
			str = g_key_file_get_string(kf, "Workbench", key, NULL);
			if (str != NULL)
			{
				splitv = g_strsplit (str, ";", -1);
				wb_project_dir_set_ignored_file_patterns(new_dir, splitv);
			}
			g_free(str);
		}
	}

	g_key_file_free(kf);
	g_free (contents);
	success = TRUE;

	return success;
}


/** Create a new empty project.
 *
 * @return Address of the new structure.
 *
 **/
WB_PROJECT *wb_project_new(const gchar *filename)
{
	WB_PROJECT *new_prj;

	new_prj = g_malloc0(sizeof *new_prj);
	new_prj->modified = FALSE;
	wb_project_set_filename(new_prj, filename);
	new_prj->bookmarks = g_ptr_array_new();
	new_prj->generate_tag_prefs = WB_PROJECT_TAG_PREFS_YES;

	return new_prj;
}


/** Free a project.
 *
 * @param prj Adress of structure to free
 *
 **/
void wb_project_free(WB_PROJECT *prj)
{
	/* Free directories first */
	g_slist_free_full(prj->directories, (GDestroyNotify)wb_project_dir_free);

	/* Free all bookmarks */
	wb_project_free_all_bookmarks(prj);

	g_free(prj->filename);
	g_free(prj->name);
	g_free(prj);
}


/** Check if dir is a valid reference to a directory of prj.
 *
 * @param prj The project to search in
 * @param dir The directory to search for
 * @return TRUE  dir is a directory in prj
 *         FALSE dir was not found in prj
 **/
gboolean wb_project_is_valid_dir_reference(WB_PROJECT *prj, WB_PROJECT_DIR *dir)
{
	GSList *elem = NULL;

	if (prj == NULL)
	{
		return FALSE;
	}

	foreach_slist(elem, prj->directories)
	{
		if (elem->data == dir)
		{
			return TRUE;
		}
	}

	return FALSE;
}
