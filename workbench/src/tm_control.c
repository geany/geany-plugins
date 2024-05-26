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
 * Code for the Workbench internal tag-manager-control.
 */
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>
#include "idle_queue.h"

extern GeanyData *geany_data;
static GHashTable *wb_tm_file_table = NULL;


/* Callback function for freeing a file table entry. The entry must
   be freed using the tag-manager in a syncronized way. So we do
   not make the call here directly but use our idle-action-queue. */
static void free_file_table_entry (gpointer data)
{
	wb_idle_queue_add_action(WB_IDLE_ACTION_ID_TM_SOURCE_FILE_FREE, data);
}


/** Initialize tm-control data.
 **/
void wb_tm_control_init (void)
{
	wb_tm_file_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GFreeFunc)free_file_table_entry);
}


/** Cleanup tm-control data.
 **/
void wb_tm_control_cleanup (void)
{
	wb_tm_file_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GFreeFunc)free_file_table_entry);
}


/** Add the file to the tm-workspace.
 * 
 * @param filename Name of the file to add.
 **/
void wb_tm_control_source_file_add(gchar *filename)
{
	TMSourceFile *sf = g_hash_table_lookup(wb_tm_file_table, filename);

	if (sf != NULL && !document_find_by_filename(filename))
	{
		tm_workspace_add_source_file(sf);
		g_hash_table_insert (wb_tm_file_table, g_strdup(filename), sf);
	}
	g_free(filename);
}


/** Remove the file from the tm-workspace.
 * 
 * @param filename Name of the file to add.
 **/
void wb_tm_control_source_file_remove(gchar *filename)
{
	TMSourceFile *sf = g_hash_table_lookup(wb_tm_file_table, filename);
	if (sf != NULL)
	{
		tm_workspace_remove_source_file(sf);
		g_hash_table_remove(wb_tm_file_table, filename);
	}
	g_free(filename);
}


/** Free a TMSourceFile
 * 
 * @param source_file The file to be freed
 **/
void wb_tm_control_source_file_free(TMSourceFile *source_file)
{
	tm_source_file_free(source_file);
}


/** Remove the files from the tm-workspace.
 * 
 * @param files GPtrArray of file names (gchar *)
 **/
void wb_tm_control_source_files_remove(GPtrArray *files)
{
	GPtrArray *source_files;
	TMSourceFile *sf;
	guint index;

	source_files = g_ptr_array_new();
	for (index = 0 ; index < files->len ; index++)
	{
		gchar *utf8_path = files->pdata[index];
		gchar *locale_path = utils_get_locale_from_utf8(utf8_path);

		sf = g_hash_table_lookup(wb_tm_file_table, locale_path);
		if (sf != NULL)
		{
			g_ptr_array_add(source_files, sf);
			g_hash_table_remove(wb_tm_file_table, locale_path);
		}

		g_free(locale_path);
	} 

	tm_workspace_remove_source_files(files);
	g_ptr_array_free(source_files, TRUE);
	g_ptr_array_free(files, TRUE);
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


/* Stolen and modified version from Geany. The only difference is that Geany
 * first looks at shebang inside the file and then, if it fails, checks the
 * file extension. Opening every file is too expensive so instead check just
 * extension and only if this fails, look at the shebang */
static GeanyFiletype *filetypes_detect(const gchar *utf8_filename)
{
	GStatBuf s;
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


/** Add the files to the tm-workspace.
 * 
 * @param files GPtrArray of file names (gchar *)
 **/
void wb_tm_control_source_files_new(GPtrArray *files)
{
	GPtrArray *source_files;
	TMSourceFile *sf;
	guint index;

	source_files = g_ptr_array_new();
	for (index = 0 ; index < files->len ; index++)
	{
		gchar *utf8_path = files->pdata[index];
		gchar *locale_path = utils_get_locale_from_utf8(utf8_path);

		sf = g_hash_table_lookup(wb_tm_file_table, locale_path);
		if (sf == NULL)
		{
			sf = tm_source_file_new(locale_path, filetypes_detect(utf8_path)->name);
			if (sf != NULL && !document_find_by_filename(utf8_path))
			{
				g_ptr_array_add(source_files, sf);
				g_hash_table_insert (wb_tm_file_table, g_strdup(locale_path), sf);
			}
		}

		g_free(locale_path);
	} 

	tm_workspace_add_source_files(source_files);
	g_ptr_array_free(source_files, TRUE);
	g_ptr_array_free(files, TRUE);
}
