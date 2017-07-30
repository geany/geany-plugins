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
 * Code for the WORKBENCH structure.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <geanyplugin.h>
#include "workbench.h"
#include "wb_project.h"
#include "utils.h"

typedef struct
{
	PROJECT_ENTRY_STATUS status;
	gchar                *abs_filename;
	gchar                *rel_filename;
	gboolean             use_abs;
	WB_PROJECT           *project;
}WB_PROJECT_ENTRY;

struct S_WORKBENCH
{
	gchar     *filename;
	gchar     *name;
	gboolean  modified;
	gboolean  rescan_projects_on_open;
	GPtrArray *projects;
	GPtrArray *bookmarks;
};

/* Create a new, empty workbench project entry */
static WB_PROJECT_ENTRY *wb_project_entry_new(void)
{
	WB_PROJECT_ENTRY *new_entry;

	new_entry = g_new(WB_PROJECT_ENTRY, 1);
	memset(new_entry, 0, sizeof(*new_entry));
	new_entry->status = PROJECT_ENTRY_STATUS_UNKNOWN;

    return new_entry;
}


/* Free a workbench entry */
static void wb_project_entry_free(WB_PROJECT_ENTRY *entry)
{
	wb_project_free(entry->project);
	g_free(entry->abs_filename);
	g_free(entry->rel_filename);
	g_free(entry);
}


/** Create a new empty workbench.
 *
 * @return Address of the new structure.
 *
 **/
WORKBENCH *workbench_new(void)
{
	WORKBENCH *new_wb;

	new_wb = g_new(WORKBENCH, 1);
	memset(new_wb, 0, sizeof(*new_wb));
	new_wb->modified = FALSE;
	new_wb->rescan_projects_on_open = TRUE;
	new_wb->projects = g_ptr_array_new();
	new_wb->bookmarks = g_ptr_array_new();

	return new_wb;
}


/** Free a workbench.
 *
 * @param wb The workbench
 * 
 **/
void workbench_free(WORKBENCH *wb)
{
	WB_PROJECT_ENTRY *entry;
	guint index;

	if (wb == NULL)
	{
		return;
	}

	/* Free projects and project entries first */
	for (index = 0 ; index < wb->projects->len ; index++)
	{
		entry = g_ptr_array_index(wb->projects, index);
		if (entry != NULL)
		{
			wb_project_entry_free(entry);
		}
	}

	g_ptr_array_free (wb->projects, TRUE);
	g_free(wb);
}


/** Is the workbench empty?
 *
 * @param wb The workbench
 * @return TRUE is workbench is empty or wb == NULL,
 *         FALSE otherwise.
 *
 **/
gboolean workbench_is_empty(WORKBENCH *wb)
{
	if (wb != NULL)
	{
		return (wb->projects->len == 0);
	}
	return TRUE;
}


/** Get the project count.
 *
 * @param wb The workbench
 * @return TRUE is workbench is empty or wb == NULL,
 *         FALSE otherwise.
 *
 **/
guint workbench_get_project_count(WORKBENCH *wb)
{
	if (wb != NULL)
	{
		return wb->projects->len;
	}
	return 0;
}


/** Is the workbench modified?
 *
 * @param wb The workbench
 * @return TRUE if the workbench is modified,
 *         FALSE if not or wb == NULL
 *
 **/
gboolean workbench_is_modified(WORKBENCH *wb)
{
	if (wb != NULL)
	{
		return wb->modified;
	}
	return FALSE;
}


/** Set the "Rescan projects on open" option.
 *
 * @param wb    The workbench
 * @param value The value to set
 *
 **/
void workbench_set_rescan_projects_on_open(WORKBENCH *wb, gboolean value)
{
	if (wb != NULL)
	{
		if (wb->rescan_projects_on_open != value)
		{
			wb->rescan_projects_on_open = value;
			wb->modified = TRUE;
		}
	}
}


/** Get the "Rescan projects on open" option.
 *
 * @param wb The workbench
 * @return TRUE = rescan all projects after opening the workbench,
 *         FALSE = don't
 *
 **/
gboolean workbench_get_rescan_projects_on_open(WORKBENCH *wb)
{
	if (wb != NULL)
	{
		return wb->rescan_projects_on_open;
	}
	return FALSE;
}


/** Set the filename.
 *
 * @param wb       The workbench
 * @param filename Name of the workbench file
 *
 **/
void workbench_set_filename(WORKBENCH *wb, gchar *filename)
{
	if (wb != NULL)
	{
		guint offset;
		gchar *ext;

		wb->filename = g_strdup(filename);
		wb->name = g_path_get_basename (filename);
		ext = g_strrstr(wb->name, ".geanywb");
		if(ext != NULL)
		{
			offset = strlen(wb->name);
			offset -= strlen(".geanywb");
			if (ext == wb->name + offset)
			{
				// Strip of file extension by overwriting
				// '.' with string terminator.
				wb->name[offset] = '\0';
			}
		}
	}
}


/** Get the filename.
 *
 * @param wb The workbench
 * @return The filename or NULL
 *
 **/
gchar *workbench_get_filename(WORKBENCH *wb)
{
	if (wb != NULL)
	{
		return wb->filename;
	}
	return NULL;
}


/** Get the name.
 *
 * @param wb The workbench
 * @return The name or NULL
 *
 **/
gchar *workbench_get_name(WORKBENCH *wb)
{
	if (wb != NULL)
	{
		return wb->name;
	}
	return NULL;
}


/** Get the project stored in the workbench at @a index.
 *
 * @param wb    The workbench
 * @param index The index
 * @return Adress of the WB_PROJECT structure or NULL
 *         if wb == NULL or an invalid index
 *
 **/
WB_PROJECT *workbench_get_project_at_index (WORKBENCH *wb, guint index)
{
	if (wb != NULL)
	{
		WB_PROJECT_ENTRY *entry;
		entry = g_ptr_array_index(wb->projects, index);
		if (entry == NULL)
		{
			return NULL;
		}
		return entry->project;
	}
	return NULL;
}


/** Get the project status in the workbench at @a index.
 *
 * Actually the project status just gives information about wheter
 * the project file for the project at @a index was found or not.
 * 
 * @param wb    The workbench
 * @param index The index
 * @return the status or PROJECT_ENTRY_STATUS_UNKNOWN if wb == NULL
 *
 **/
PROJECT_ENTRY_STATUS workbench_get_project_status_at_index (WORKBENCH *wb, guint index)
{
	if (wb != NULL)
	{
		WB_PROJECT_ENTRY *entry;
		entry = g_ptr_array_index(wb->projects, index);
		if (entry == NULL)
		{
			return PROJECT_ENTRY_STATUS_UNKNOWN;
		}
		return entry->status;
	}
	return PROJECT_ENTRY_STATUS_UNKNOWN;
}


/** Get the project status in the workbench by address.
 *
 * Actually the project status just gives information about wheter
 * the project file for the project at @a index was found or not.
 * 
 * @param wb      The workbench
 * @param address Location of the project
 * @return the status or PROJECT_ENTRY_STATUS_UNKNOWN if wb == NULL
 *
 **/
PROJECT_ENTRY_STATUS workbench_get_project_status_by_address (WORKBENCH *wb, WB_PROJECT *address)
{
	guint index;
	if (wb != NULL || address == NULL)
	{
		WB_PROJECT_ENTRY *entry;
		for (index = 0 ; index < wb->projects->len ; index++)
		{
			entry = g_ptr_array_index(wb->projects, index);
			if (entry != NULL && entry->project == address)
			{
				return entry->status;
			}
		}
	}
	return PROJECT_ENTRY_STATUS_UNKNOWN;
}


/** Add a project to the workbench.
 * 
 * @param wb       The workbench
 * @param filename Project file
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean workbench_add_project(WORKBENCH *wb, gchar *filename)
{
	if (wb != NULL)
	{
		GStatBuf buf;
		WB_PROJECT *project;
		WB_PROJECT_ENTRY *entry;

		entry = wb_project_entry_new();
		if (entry == NULL)
		{
			return FALSE;
		}
		project = wb_project_new(filename);
		if (project == NULL)
		{
			wb_project_entry_free(entry);
			return FALSE;
		}

		// Set entry data:
		// - absolute and relative filename (relative to workbench file)
		// - per default use relative path
		// - check status of project file
		// - pointer to the project data
		entry->abs_filename = g_strdup(filename);
		entry->rel_filename = get_any_relative_path
		                          (wb->filename, filename);
		entry->use_abs = FALSE;
		entry->project = project;
		if (g_stat (filename, &buf) == 0)
		{
			entry->status = PROJECT_ENTRY_STATUS_OK;
		}
		else
		{
			entry->status = PROJECT_ENTRY_STATUS_NOT_FOUND;
		}
		g_ptr_array_add (wb->projects, entry);

		wb->modified = TRUE;
		return TRUE;
	}
	return FALSE;
}


/** Remove a project from the workbench.
 * 
 * @param wb      The workbench
 * @param project The Project
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean workbench_remove_project_with_address(WORKBENCH *wb, WB_PROJECT *project)
{
	if (wb != NULL && wb->projects != NULL)
	{
		guint index;
		WB_PROJECT_ENTRY *current;

		for (index = 0 ; index < wb->projects->len ; index++)
		{
			current = g_ptr_array_index(wb->projects, index);
			if (current != NULL && current->project == project)
			{
				g_ptr_array_remove_index (wb->projects, index);
				wb_project_entry_free(current);
				wb->modified = TRUE;
				return TRUE;
			}
		}
	}
	return FALSE;
}


/** Is the file included in the workbench?
 *
 * @param wb       The workbench
 * @param filename The file
 * @return Address of project in which the file is included.
 *         NULL if the file is not included in any workbench project.
 *
 **/
WB_PROJECT *workbench_file_is_included (WORKBENCH *wb, gchar *filename)
{
	if (wb != NULL)
	{
		guint index;
		WB_PROJECT_ENTRY *current;

		for (index = 0 ; index < wb->projects->len ; index++)
		{
			current = g_ptr_array_index(wb->projects, index);
			if (current != NULL && wb_project_file_is_included(current->project, filename) == TRUE)
			{
				return current->project;
			}
		}
	}
	return NULL;
}


/* Add a workbench bookmark */
static gboolean workbench_add_bookmark_int(WORKBENCH *wb, gchar *filename)
{
	if (wb != NULL)
	{
		gchar *new;

		new = g_strdup(filename);
		if (new != NULL)
		{
			g_ptr_array_add (wb->bookmarks, new);
			return TRUE;
		}
	}
	return FALSE;
}


/** Add a bookmark to a workbench.
 * 
 * @param wb       The workbench
 * @param filename File to bookmark
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean workbench_add_bookmark(WORKBENCH *wb, gchar *filename)
{
	if (workbench_add_bookmark_int(wb, filename) == TRUE)
	{
		wb->modified = TRUE;
		return TRUE;
	}
	return FALSE;
}


/** Remove a bookmark from a workbench by filename address.
 * 
 * @param wb       The workbench
 * @param filename File to remove
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean workbench_remove_bookmark(WORKBENCH *wb, gchar *filename)
{
	if (wb != NULL)
	{
		guint index;
		gchar *current;

		for (index = 0 ; index < wb->bookmarks->len ; index++)
		{
			current = g_ptr_array_index(wb->bookmarks, index);
			if (current == filename)
			{
				g_ptr_array_remove_index (wb->bookmarks, index);
				wb->modified = TRUE;
				return TRUE;
			}
		}
	}
	return FALSE;
}


/** Get the bookmark at @a index.
 * 
 * @param wb    The workbench
 * @param index The index
 * @return Address of filename or NULL
 *
 **/
gchar *workbench_get_bookmark_at_index (WORKBENCH *wb, guint index)
{
	if (wb != NULL)
	{
		gchar *file;
		file = g_ptr_array_index(wb->bookmarks, index);
		if (file == NULL)
		{
			return NULL;
		}
		return file;
	}
	return NULL;
}


/** Get the number of bookmarks in a workbench.
 * 
 * @param wb The workbench
 * @return The number of bookmarks or 0 if wb == NULL
 *
 **/
guint workbench_get_bookmarks_count(WORKBENCH *wb)
{
	if (wb != NULL && wb->bookmarks != NULL)
	{
		return wb->bookmarks->len;
	}
	return 0;
}


/** Save a workbench.
 * 
 * @param wb    The workbench
 * @param error Location for returning an GError
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean workbench_save(WORKBENCH *wb, GError **error)
{
	gboolean success = FALSE;

	if (wb != NULL)
	{
		GKeyFile *kf;
		guint	 index;
		gchar    *contents;
		gchar    group[20];
		gsize    length, boomarks_size;
		WB_PROJECT_ENTRY *entry;

		kf = g_key_file_new ();

		// Save common, simple values
		g_key_file_set_string(kf, "General", "filetype", "workbench");
		g_key_file_set_string(kf, "General", "version", "1.0");
		g_key_file_set_boolean(kf, "General", "RescanProjectsOnOpen", wb->rescan_projects_on_open);

		// Save Workbench bookmarks as string list
		boomarks_size = workbench_get_bookmarks_count(wb);
		if (boomarks_size > 0)
		{
			gchar **bookmarks_strings, *file, *rel_path;

			bookmarks_strings = g_new0(gchar *, boomarks_size+1);
			for (index = 0 ; index < boomarks_size ; index++ )
			{
				file = workbench_get_bookmark_at_index(wb, index);
				rel_path = get_any_relative_path(wb->filename, file);

				bookmarks_strings[index] = rel_path;
			}
			g_key_file_set_string_list
				(kf, "General", "Bookmarks", (const gchar **)bookmarks_strings, boomarks_size);
			for (index = 0 ; index < boomarks_size ; index++ )
			{
				g_free (bookmarks_strings[index]);
			}
			g_free(bookmarks_strings);
		}

		// Save projects data
		for (index = 0 ; index < wb->projects->len ; index++)
		{
			entry = g_ptr_array_index(wb->projects, index);
			g_snprintf(group, sizeof(group), "Project-%u", (index+1));
			g_key_file_set_string(kf, group, "AbsFilename", entry->abs_filename);
			g_key_file_set_string(kf, group, "RelFilename", entry->rel_filename);
			g_key_file_set_boolean(kf, group, "UseAbsFilename", entry->use_abs);
		}
		contents = g_key_file_to_data (kf, &length, error);
		g_key_file_free(kf);

		success = g_file_set_contents (wb->filename, contents, length, error);
		if (success)
		{
			wb->modified = FALSE;
		}
		g_free (contents);
	}
	else if (error != NULL)
	{
		g_set_error (error, 0, 0,
					 "Internal error: param missing (file: %s, line %d)",
					 __FILE__, __LINE__);
	}

	return success;
}


/** Load a workbench file.
 * 
 * The function loads the workbench settings from file @a filename and
 * stores it into @a wb.
 * 
 * @param wb       The workbench
 * @param filename File to load from
 * @param error    Location for returning an GError
 * @return TRUE on success, FALSE otherwise
 *
 **/
gboolean workbench_load(WORKBENCH *wb, gchar *filename, GError **error)
{
	gboolean success = FALSE;

	if (wb != NULL)
	{
		GKeyFile *kf;
		gboolean valid = TRUE;
		guint    index;
		gchar    *contents, **bookmarks_strings;
		gchar    group[20];
		gsize    length;
		WB_PROJECT_ENTRY *entry;

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

		if (g_key_file_has_key (kf, "General", "filetype", NULL)
			&&
			g_key_file_has_key (kf, "General", "version", NULL))
		{
			gchar *check;
			check = g_key_file_get_string (kf, "General", "filetype", error);
			if (check == NULL || g_strcmp0(check, "workbench") != 0)
			{
				valid = FALSE;
			}
			g_free(check);
		}
		else
		{
			valid = FALSE;
		}

		if (!valid)
		{
			g_set_error (error, 0, 0,
						 "File: %s is not a valid workbench file!",
						 filename);
			return FALSE;
		}
		workbench_set_filename(wb, filename);
		wb->rescan_projects_on_open = g_key_file_get_boolean(kf, "General", "RescanProjectsOnOpen", error);

		// Load Workbench bookmarks from string list
		bookmarks_strings = g_key_file_get_string_list (kf, "General", "Bookmarks", NULL, error);
		if (bookmarks_strings != NULL)
		{
			gchar **file, *abs_path;

			file = bookmarks_strings;
			while (*file != NULL)
			{
				abs_path = get_combined_path(wb->filename, *file);
				if (abs_path != NULL)
				{
					workbench_add_bookmark_int(wb, abs_path);
					g_free(abs_path);
				}
				file++;
			}
			g_strfreev(bookmarks_strings);
		}

		// Load projects data
		for (index = 0 ; index < 1024 ; index++)
		{
			g_snprintf(group, sizeof(group), "Project-%u", (index+1));
			if (g_key_file_has_key (kf, group, "AbsFilename", NULL))
			{
				gchar *prj_filename;
				entry = wb_project_entry_new();
				if (entry == NULL)
				{
					continue;
				}
				entry->abs_filename = g_key_file_get_string(kf, group, "AbsFilename", error);
				entry->rel_filename = g_key_file_get_string(kf, group, "RelFilename", error);
				entry->use_abs = g_key_file_get_boolean(kf, group, "UseAbsFilename", error);
				if (entry->use_abs == TRUE)
				{
					prj_filename = entry->abs_filename;
				}
				else
				{
					prj_filename = get_combined_path
										(wb->filename, entry->rel_filename);
				}
				if (prj_filename != NULL)
				{
					GStatBuf buf;

					entry->project = wb_project_new(prj_filename);
					if (g_stat (prj_filename, &buf) == 0)
					{
						entry->status = PROJECT_ENTRY_STATUS_OK;

						// ToDo: collect and handle project load errors
						wb_project_load(entry->project, prj_filename, error);
					}
					else
					{
						entry->status = PROJECT_ENTRY_STATUS_NOT_FOUND;
					}
					g_ptr_array_add (wb->projects, entry);

					if (wb->rescan_projects_on_open == TRUE)
					{
						wb_project_rescan(entry->project);
					}
				}
			}
			else
			{
				break;
			}
		}

		g_key_file_free(kf);
		g_free (contents);
		success = TRUE;
	}
	else if (error != NULL)
	{
		g_set_error (error, 0, 0,
						"Internal error: param missing (file: %s, line %d)",
						__FILE__, __LINE__);
	}

	return success;
}
