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
 * Code for setup and control of the sidebar.
 */
#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include "wb_globals.h"
#include <gtkcompat.h>
#include <gp_gtkcompat.h>

#include "sidebar.h"
#include "popup_menu.h"
#include "utils.h"

enum
{
	FILEVIEW_COLUMN_ICON,
	FILEVIEW_COLUMN_NAME,
	FILEVIEW_COLUMN_DATA_ID,
	FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER,
	FILEVIEW_N_COLUMNS,
};

typedef enum
{
	MATCH_FULL,
	MATCH_PREFIX,
	MATCH_PATTERN
} MatchType;

typedef struct
{
	gboolean iter_valid;
	GtkTreeIter iter;
	gboolean parent_valid;
	GtkTreeIter parent;
}ITER_SEARCH_RESULT;

typedef struct
{
	GeanyProject *project;
	GPtrArray *expanded_paths;
} ExpandData;

typedef struct
{
	SIDEBAR_CONTEXT *context;
	GtkTreeModel *model;
	guint dataid;
	void (*func)(SIDEBAR_CONTEXT *, gpointer userdata);
	gpointer userdata;
}SB_CALLFOREACH_CONTEXT;

typedef struct SIDEBAR
{
    GtkWidget *file_view_vbox;
    GtkWidget *file_view;
    GtkTreeStore *file_store;
    GtkWidget *file_view_label;
    GtkWidget *file_view_placeholder;
}SIDEBAR;
static SIDEBAR sidebar = {NULL, NULL, NULL, NULL};

static gboolean sidebar_get_directory_iter(WB_PROJECT *prj, WB_PROJECT_DIR *dir, GtkTreeIter *iter);
static gboolean sidebar_get_filepath_iter (WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath, ITER_SEARCH_RESULT *result);

/* Remove all child nodes below parent */
static void sidebar_remove_children(GtkTreeIter *parent)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));
	if (gtk_tree_model_iter_children (model, &iter, parent))
	{
		while (gtk_tree_store_remove (sidebar.file_store, &iter)) {}
	}
}


/* Create a branch for a sub-directory */
static void sidebar_create_branch(gint level, const gchar *abs_base_dir, GSList *leaf_list, GtkTreeIter *parent)
{
	GSList *dir_list = NULL;
	GSList *file_list = NULL;
	GSList *elem = NULL;
	gchar **path_arr, *part, *full, *dirpath;

	foreach_slist (elem, leaf_list)
	{
		if (elem->data == NULL)
		{
			continue;
		}
		path_arr = elem->data;
		if (path_arr[level] == NULL)
		{
			continue;
		}

		if (path_arr[level+1] != NULL)
		{
			dir_list = g_slist_prepend(dir_list, path_arr);
		}
		else
		{
			// Extra check for empty directories
			part = g_build_filenamev(path_arr);
			dirpath = g_build_filename(abs_base_dir, part, NULL);
			if (dirpath != NULL && g_file_test (dirpath, G_FILE_TEST_IS_DIR))
			{
				dir_list = g_slist_prepend(dir_list, path_arr);
				g_free(dirpath);
			}
			else
			{
				file_list = g_slist_prepend(file_list, path_arr);
			}
		}
	}

	foreach_slist (elem, file_list)
	{
		GtkTreeIter iter;
		GIcon *icon = NULL;

		path_arr = elem->data;
		gchar *content_type = g_content_type_guess(path_arr[level], NULL, 0, NULL);

		if (content_type)
		{
			icon = g_content_type_get_icon(content_type);
			if (icon)
			{
				GtkIconInfo *icon_info;

				icon_info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon, 16, 0);
				if (!icon_info)
				{
					g_object_unref(icon);
					icon = NULL;
				}
				else
					gtk_icon_info_free(icon_info);
			}
			g_free(content_type);
		}

		/* Build full absolute file name to use it on row activate to
		   open the file. Will be assigned as data pointer, see below. */
		part = g_build_filenamev(path_arr);
		full = g_build_filename(abs_base_dir, part, NULL);
		g_free(part);

		gtk_tree_store_insert_with_values(sidebar.file_store, &iter, parent, 0,
			FILEVIEW_COLUMN_ICON, icon,
			FILEVIEW_COLUMN_NAME, path_arr[level],
			FILEVIEW_COLUMN_DATA_ID, DATA_ID_FILE,
			FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, full,
			-1);

		if (icon)
		{
			g_object_unref(icon);
		}
	}

	if (dir_list)
	{
		GSList *tmp_list = NULL;
		GtkTreeIter iter;
		gchar *last_dir_name;
		GIcon *icon_dir = g_icon_new_for_string("folder", NULL);

		path_arr = dir_list->data;
		last_dir_name = path_arr[level];

		foreach_slist (elem, dir_list)
		{
			gboolean dir_changed;

			part = g_build_filenamev(path_arr);
			full = g_build_filename(abs_base_dir, part, NULL);
			g_free(part);

			path_arr = (gchar **) elem->data;
			dir_changed = g_strcmp0(last_dir_name, path_arr[level]) != 0;

			if (dir_changed)
			{
				gtk_tree_store_insert_with_values(sidebar.file_store, &iter, parent, 0,
					FILEVIEW_COLUMN_ICON, icon_dir,
					FILEVIEW_COLUMN_NAME, last_dir_name,
					FILEVIEW_COLUMN_DATA_ID, DATA_ID_SUB_DIRECTORY,
					/* cppcheck-suppress leakNoVarFunctionCall
					 * type is gpointer, but admittedly I don't see where it's freed? */
					FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, g_strdup(full),
					-1);

				sidebar_create_branch(level+1, abs_base_dir, tmp_list, &iter);

				g_slist_free(tmp_list);
				tmp_list = NULL;
				last_dir_name = path_arr[level];
			}
			g_free(full);

			tmp_list = g_slist_prepend(tmp_list, path_arr);
		}

		part = g_build_filenamev(path_arr);
		full = g_build_filename(abs_base_dir, part, NULL);
		g_free(part);

		gtk_tree_store_insert_with_values(sidebar.file_store, &iter, parent, 0,
			FILEVIEW_COLUMN_ICON, icon_dir,
			FILEVIEW_COLUMN_NAME, last_dir_name,
			FILEVIEW_COLUMN_DATA_ID, DATA_ID_SUB_DIRECTORY,
			/* cppcheck-suppress leakNoVarFunctionCall
			 * type is gpointer, but admittedly I don't see where it's freed? */
			FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, g_strdup(full),
			-1);
			g_free(full);

		sidebar_create_branch(level+1, abs_base_dir, tmp_list, &iter);

		g_slist_free(tmp_list);
		g_slist_free(dir_list);
		if (icon_dir != NULL)
		{
			g_object_unref(icon_dir);
		}
	}

	g_slist_free(file_list);
}


/* Reverse strcmp */
static int rev_strcmp(const char *str1, const char *str2)
{
	return g_strcmp0(str2, str1);
}


/* Insert given directory/sub-directory into the sidebar file tree */
static void sidebar_insert_project_directory(WB_PROJECT *prj, WB_PROJECT_DIR *directory, GtkTreeIter *parent)
{
	GSList *lst = NULL;
	GSList *path_list = NULL;
	GSList *elem = NULL;
	GHashTableIter iter;
	gpointer key, value;
	gchar *abs_base_dir;

	g_hash_table_iter_init(&iter, wb_project_dir_get_file_table(directory));
	abs_base_dir = get_combined_path(wb_project_get_filename(prj), wb_project_dir_get_base_dir(directory));

	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		gchar *path = get_relative_path(abs_base_dir, key);
		if (path != NULL)
		{
			lst = g_slist_prepend(lst, path);
		}
	}
	/* sort in reverse order so we can prepend nodes to the tree store -
	 * the store behaves as a linked list and prepending is faster */
	lst = g_slist_sort(lst, (GCompareFunc) rev_strcmp);

	foreach_slist (elem, lst)
	{
		gchar **path_split;

		path_split = g_strsplit_set(elem->data, "/\\", 0);
		path_list = g_slist_prepend(path_list, path_split);
	}

	if (path_list != NULL)
		sidebar_create_branch(0, abs_base_dir, path_list, parent);

	g_slist_free_full(lst, g_free);
	g_slist_free_full(path_list, (GDestroyNotify) g_strfreev);
}


/* Insert all directories (WB_PROJECT_DIRs) into the sidebar file tree */
static void sidebar_insert_project_directories (WB_PROJECT *project, GtkTreeIter *parent, gint *position)
{
	GtkTreeIter iter;
	GIcon *icon, *icon_dir, *icon_base;
	GSList *elem = NULL, *dirs;

	if (project == NULL)
	{
		return;
	}

	dirs = wb_project_get_directories(project);
	if (dirs != NULL)
	{
		const gchar *name;

		icon_dir = g_icon_new_for_string("system-search", NULL);
		icon_base = g_icon_new_for_string("user-home", NULL);

		foreach_slist (elem, dirs)
		{
			if (wb_project_dir_get_is_prj_base_dir(elem->data) == TRUE)
			{
				icon = icon_base;
				name = _("Base dir");
			}
			else
			{
				icon = icon_dir;
				name = wb_project_dir_get_name(elem->data);
			}

			gtk_tree_store_insert_with_values(sidebar.file_store, &iter, parent, *position,
				FILEVIEW_COLUMN_ICON, icon,
				FILEVIEW_COLUMN_NAME, name,
				FILEVIEW_COLUMN_DATA_ID, DATA_ID_DIRECTORY,
				FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, elem->data,
				-1);
			(*position)++;
			sidebar_insert_project_directory(project, elem->data, &iter);
		}

		if (icon_dir != NULL)
		{
			g_object_unref(icon_dir);
		}
		if (icon_base != NULL)
		{
			g_object_unref(icon_base);
		}
	}
	else
	{
		icon = g_icon_new_for_string("dialog-information", NULL);

		gtk_tree_store_insert_with_values(sidebar.file_store, &iter, parent, *position,
			FILEVIEW_COLUMN_ICON, icon,
			FILEVIEW_COLUMN_NAME, _("No directories"),
			FILEVIEW_COLUMN_DATA_ID, DATA_ID_NO_DIRS,
			-1);
		(*position)++;

		if (icon != NULL)
		{
			g_object_unref(icon);
		}
	}
}


/* Add a file to the sidebar. filepath can be a file or directory. */
static void sidebar_add_file (WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath)
{
	GIcon *icon = NULL;
	gchar *name;
	guint dataid;
	ITER_SEARCH_RESULT search_result;

	if (!sidebar_get_filepath_iter(prj, root, filepath, &search_result))
	{
		return;
	}
	else
	{
		if (search_result.iter_valid)
		{
			/* Error, file already exists in sidebar tree. */
			return;
		}
		else
		{
			/* This is the expected result as we want to add a new file node.
			   But parent should be valid. */
			if (!search_result.parent_valid)
			{
				return;
			}
		}
	}

	/* Collect data */
	name = g_path_get_basename(filepath);
	if (g_file_test (filepath, G_FILE_TEST_IS_DIR))
	{
		dataid = DATA_ID_SUB_DIRECTORY;
		icon = g_icon_new_for_string("folder", NULL);
	}
	else
	{
		dataid = DATA_ID_FILE;

		gchar *content_type = g_content_type_guess(filepath, NULL, 0, NULL);
		if (content_type)
		{
			icon = g_content_type_get_icon(content_type);
			if (icon)
			{
				GtkIconInfo *icon_info;

				icon_info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon, 16, 0);
				if (!icon_info)
				{
					g_object_unref(icon);
					icon = NULL;
				}
				else
					gtk_icon_info_free(icon_info);
			}
			g_free(content_type);
		}

	}

	/* Create new row/node in sidebar tree. */
	gtk_tree_store_insert_with_values(sidebar.file_store, &search_result.iter, &search_result.parent, -1,
		FILEVIEW_COLUMN_ICON, icon,
		FILEVIEW_COLUMN_NAME, name,
		FILEVIEW_COLUMN_DATA_ID, dataid,
		/* cppcheck-suppress leakNoVarFunctionCall
		 * type is gpointer, but admittedly I don't see where it's freed? */
		FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, g_strdup(filepath),
		-1);

	if (icon)
	{
		g_object_unref(icon);
	}
	g_free(name);
}


/* Remove a file from the sidebar. filepath can be a file or directory. */
static void sidebar_remove_file (WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath)
{
	ITER_SEARCH_RESULT search_result;

	if (!sidebar_get_filepath_iter(prj, root, filepath, &search_result))
	{
		return;
	}
	else
	{
		if (search_result.iter_valid)
		{
			/* File was found, remove the node. */
			gtk_tree_store_remove(sidebar.file_store, &search_result.iter);
		}
	}
}


/* Get the GtkTreeIter for project prj */
static gboolean sidebar_get_project_iter(WB_PROJECT *prj, GtkTreeIter *iter)
{
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));
	if (gtk_tree_model_get_iter_first (model, iter))
	{
		WB_PROJECT *current;

		do
		{
			gtk_tree_model_get(model, iter, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &current, -1);
			if (current == prj)
			{
				return TRUE;
			}
		}while (gtk_tree_model_iter_next(model, iter));
	}
	return FALSE;
}


/* Get the GtkTreeIter for directory dir in project prj */
static gboolean sidebar_get_directory_iter(WB_PROJECT *prj, WB_PROJECT_DIR *dir, GtkTreeIter *iter)
{
	GtkTreeIter parent;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));
	if (sidebar_get_project_iter(prj, iter))
	{
		parent = *iter;
		if (gtk_tree_model_iter_children (model, iter, &parent))
		{
			WB_PROJECT_DIR *current;

			do
			{
				gtk_tree_model_get(model, iter, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &current, -1);
				if (current == dir)
				{
					return TRUE;
				}
			}while (gtk_tree_model_iter_next(model, iter));
		}
	}
	return FALSE;
}


/* Get the GtkTreeIter for filepath (absolute) in directory root in project prj */
static gboolean sidebar_get_filepath_iter (WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath, ITER_SEARCH_RESULT *search_result)
{
	guint len, index;
	gchar *part, **parts, *found = NULL, *last_found, *abs_base_dir;
	GtkTreeIter iter, parent;
	GtkTreeModel *model;

	if (search_result == NULL)
	{
		return FALSE;
	}
	search_result->iter_valid = FALSE;
	search_result->parent_valid = FALSE;

	abs_base_dir = get_combined_path(wb_project_get_filename(prj), wb_project_dir_get_base_dir(root));
	len = strlen(abs_base_dir);
	if (strncmp(abs_base_dir, filepath, len) != 0)
	{
		/* Error, return. */
		return FALSE;
	}
	if (filepath[len] == G_DIR_SEPARATOR)
	{
		len++;
	}
	part = g_strdup(&(filepath[len]));
	if (strlen(part) == 0)
	{
		g_free(part);
		return FALSE;
	}
	parts = g_strsplit(part, G_DIR_SEPARATOR_S, -1);
	if (parts[0] == NULL)
	{
		g_free(part);
		g_strfreev(parts);
		return FALSE;
	}

	if (sidebar_get_directory_iter(prj, root, &iter))
	{
		index = 0;
		last_found = NULL;
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));

		while (parts[index] != NULL)
		{
			parent = iter;
			if (gtk_tree_model_iter_children(model, &iter, &parent))
			{
				gchar *current;

				found = NULL;
				do
				{
					gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_NAME, &current, -1);

					if (g_strcmp0(current, parts[index]) == 0)
					{
						found = current;
						last_found = current;
						index++;
						//parent = iter;
						break;
					}
				}while (gtk_tree_model_iter_next(model, &iter));

				/* Did we find the next match? */
				if (found == NULL)
				{
					/* No, abort. */
					break;
				}
			}
			else
			{
				break;
			}
		}

		/* Did we find a full match? */
		if (parts[index] == NULL && found != NULL)
		{
			/* Yes. */
			search_result->iter_valid = TRUE;
			search_result->iter = iter;

			search_result->parent_valid = TRUE;
			search_result->parent = parent;
		}
		else if (parts[index+1] == NULL &&
				 (last_found != NULL || parts[1] == NULL))
		{
			/* Not a full match but everything but the last part
			   of the filepath. At least return the parent. */
			search_result->parent_valid = TRUE;
			search_result->parent = parent;
		}
	}

	g_free(part);
	g_free(abs_base_dir);
	g_strfreev(parts);

	return TRUE;
}


/* Remove all rows from the side bar tree with the given data id */
static void sidebar_remove_nodes_with_data_id(guint toremove, GtkTreeIter *start)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean has_next;
	guint dataid;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));
	if (start == NULL)
	{
		if (!gtk_tree_model_get_iter_first(model, &iter))
		{
			return;
		}
	}
	else
	{
		iter = *start;
	}

	do
	{
		gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_DATA_ID, &dataid, -1);
		if (dataid == toremove)
		{
			has_next = gtk_tree_store_remove(sidebar.file_store, &iter);
		}
		else
		{
			has_next = gtk_tree_model_iter_next(model, &iter);
		}
	}
	while (has_next);
}


/* Insert all project bookmarks into the sidebar file tree */
static void sidebar_insert_project_bookmarks(WB_PROJECT *project, GtkTreeIter *parent, gint *position)
{
	GIcon *icon;
	guint index, max;
	GtkTreeIter iter;

	if (project == NULL)
		return;

	max = wb_project_get_bookmarks_count(project);
	if (max == 0)
		return;

	icon = g_icon_new_for_string("user-bookmarks", NULL);
	for (index = 0 ; index < max ; index++)
	{
		gchar *file, *name;

		file = wb_project_get_bookmark_at_index(project, index);
		name = get_any_relative_path(wb_project_get_filename(project), file);
		gtk_tree_store_insert_with_values(sidebar.file_store, &iter, parent, *position,
			FILEVIEW_COLUMN_ICON, icon,
			FILEVIEW_COLUMN_NAME, name,
			FILEVIEW_COLUMN_DATA_ID, DATA_ID_PRJ_BOOKMARK,
			FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, file,
			-1);
		(*position)++;
	}
	if (icon != NULL)
	{
		g_object_unref(icon);
	}
}


/* Update the sidebar for a project only */
static void sidebar_update_project(WB_PROJECT *project, gboolean title_only)
{
	GtkTreeIter iter;

	if (wb_globals.opened_wb == NULL)
		return;

	if (sidebar_get_project_iter(project, &iter))
	{
		/* Update the title */
		GString *name = g_string_new(wb_project_get_name(project));
		if (wb_project_is_modified(project))
		{
			g_string_append_c(name, '*');
		}

		gtk_tree_store_set(sidebar.file_store, &iter,
			FILEVIEW_COLUMN_NAME, name->str,
			-1);
		g_string_free(name, TRUE);

		/* Update children/content to? */
		if (!title_only)
		{
			gint position = 0;
			sidebar_remove_children(&iter);
			sidebar_insert_project_bookmarks(project, &iter, &position);
			sidebar_insert_project_directories(project, &iter, &position);
		}
	}
}


/* Insert all projects into the sidebar file tree */
static void sidebar_insert_all_projects(GtkTreeIter *iter, gint *position)
{
	GIcon *icon_good, *icon_bad, *icon;
	guint index, max;

	if (wb_globals.opened_wb == NULL)
		return;

	icon_good = g_icon_new_for_string("package-x-generic", NULL);
	icon_bad = g_icon_new_for_string("dialog-error", NULL);

	max = workbench_get_project_count(wb_globals.opened_wb);
	for (index = 0 ; index < max ; index++)
	{
		gint child_position;
		WB_PROJECT *project;

		project = workbench_get_project_at_index(wb_globals.opened_wb, index);
		if (workbench_get_project_status_at_index(wb_globals.opened_wb, index)
			==
			PROJECT_ENTRY_STATUS_OK)
		{
			icon = icon_good;
		}
		else
		{
			icon = icon_bad;
		}

		GString *name = g_string_new(wb_project_get_name(project));
		if (wb_project_is_modified(project))
		{
			g_string_append_c(name, '*');
		}

		gtk_tree_store_insert_with_values(sidebar.file_store, iter, NULL, *position,
			FILEVIEW_COLUMN_ICON, icon,
			FILEVIEW_COLUMN_NAME, name->str,
			FILEVIEW_COLUMN_DATA_ID, DATA_ID_PROJECT,
			FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, project,
			-1);
		g_string_free(name, TRUE);

		child_position = 0;
		/* Not required here as we build a completely new tree
		   sidebar_remove_children(&iter); */
		sidebar_insert_project_bookmarks(project, iter, &child_position);
		sidebar_insert_project_directories(project, iter, &child_position);
	}
	gtk_tree_view_expand_all(GTK_TREE_VIEW(sidebar.file_view));

	if (icon_good != NULL)
	{
		g_object_unref(icon_good);
	}
	if (icon_bad != NULL)
	{
		g_object_unref(icon_bad);
	}
}


/* Insert all workbench bookmarks into the sidebar file tree */
static void sidebar_insert_workbench_bookmarks(WORKBENCH *workbench, GtkTreeIter *iter, gint *position)
{
	GIcon *icon;
	guint index, max;

	if (workbench == NULL)
	{
		return;
	}

	sidebar_remove_nodes_with_data_id(DATA_ID_WB_BOOKMARK, NULL);

	max = workbench_get_bookmarks_count(workbench);
	if (max == 0)
	{
		return;
	}

	icon = g_icon_new_for_string("user-bookmarks", NULL);
	for (index = 0 ; index < max ; index++)
	{
		gchar *file, *name;

		file = workbench_get_bookmark_at_index(workbench, index);
		name = get_any_relative_path(workbench_get_filename(workbench), file);
		gtk_tree_store_insert_with_values(sidebar.file_store, iter, NULL, *position,
			FILEVIEW_COLUMN_ICON, icon,
			FILEVIEW_COLUMN_NAME, name,
			FILEVIEW_COLUMN_DATA_ID, DATA_ID_WB_BOOKMARK,
			FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, file,
			-1);
		(*position)++;
	}

	gtk_tree_view_expand_all(GTK_TREE_VIEW(sidebar.file_view));
	if (icon != NULL)
	{
		g_object_unref(icon);
	}
}


/* Reset/Clear/empty the sidebar file tree */
static void sidebar_reset_tree_store(void)
{
	gtk_tree_store_clear(sidebar.file_store);
}


/* Update the workbench part of the sidebar */
static void sidebar_update_workbench(GtkTreeIter *iter, gint *position)
{
	guint count;

	if (wb_globals.opened_wb == NULL)
	{
		gtk_label_set_text (GTK_LABEL(sidebar.file_view_label), _("No workbench opened."));
		sidebar_show_intro_message(_("Create or open a workbench, using the workbench menu."), FALSE);
		sidebar_deactivate();
	}
	else
	{
		gint length;
		gchar text[200];

		gtk_tree_view_set_hover_expand(GTK_TREE_VIEW(sidebar.file_view),
			workbench_get_expand_on_hover(wb_globals.opened_wb));

		gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(sidebar.file_view),
			workbench_get_enable_tree_lines(wb_globals.opened_wb));

		count = workbench_get_project_count(wb_globals.opened_wb);
		length = g_snprintf(text, sizeof(text),
							g_dngettext(GETTEXT_PACKAGE, "%s: %u Project", "%s: %u Projects", count),
							workbench_get_name(wb_globals.opened_wb), count);
		if (length < (gint)(sizeof(text)-1) && workbench_is_modified(wb_globals.opened_wb))
		{
			text [length] = '*';
			text [length+1] = '\0';
		}
		gtk_label_set_text (GTK_LABEL(sidebar.file_view_label), text);
		if (count == 0)
		{
			sidebar_show_intro_message(
				_("Add a project using the context menu or select \"Search projects\" from the menu."),
				TRUE);
		}
		else
		{
			/* Add/show workbench bookmarks if any */
			if (iter != NULL)
			{
				sidebar_insert_workbench_bookmarks(wb_globals.opened_wb, iter, position);
			}
		}
	}
}


/** Update the sidebar.
 *
 * Update the sidebar according to the given situation/event @a event
 * and data in @a context.
 *
 * @param event   Reason for the sidebar update
 * @param context Data, e.g. actually selected project...
 *
 **/
void sidebar_update (SIDEBAR_EVENT event, SIDEBAR_CONTEXT *context)
{
	GtkTreeIter iter;
	gint position = 0;

	switch (event)
	{
		case SIDEBAR_CONTEXT_WB_CREATED:
		case SIDEBAR_CONTEXT_WB_OPENED:
		case SIDEBAR_CONTEXT_PROJECT_ADDED:
		case SIDEBAR_CONTEXT_PROJECT_REMOVED:
			gtk_widget_hide(sidebar.file_view_placeholder);
			sidebar_reset_tree_store();
			sidebar_update_workbench(&iter, &position);
			sidebar_insert_all_projects(&iter, &position);

			if (event == SIDEBAR_CONTEXT_WB_CREATED ||
				event == SIDEBAR_CONTEXT_WB_OPENED)
			{
				gtk_tree_view_set_hover_expand(GTK_TREE_VIEW(sidebar.file_view),
					workbench_get_expand_on_hover(wb_globals.opened_wb));
			}

			sidebar_activate();
		break;
		case SIDEBAR_CONTEXT_WB_SAVED:
		case SIDEBAR_CONTEXT_WB_SETTINGS_CHANGED:
		case SIDEBAR_CONTEXT_WB_CLOSED:
			sidebar_update_workbench(NULL, &position);
		break;
		case SIDEBAR_CONTEXT_PROJECT_SAVED:
			if (context != NULL && context->project != NULL)
			{
				sidebar_update_project(context->project, TRUE);
			}
		break;
		case SIDEBAR_CONTEXT_DIRECTORY_ADDED:
		case SIDEBAR_CONTEXT_DIRECTORY_REMOVED:
		case SIDEBAR_CONTEXT_DIRECTORY_RESCANNED:
		case SIDEBAR_CONTEXT_DIRECTORY_SETTINGS_CHANGED:
		case SIDEBAR_CONTEXT_PRJ_BOOKMARK_ADDED:
		case SIDEBAR_CONTEXT_PRJ_BOOKMARK_REMOVED:
			if (context != NULL && context->project != NULL)
			{
				sidebar_update_project(context->project, FALSE);
			}
		break;
		case SIDEBAR_CONTEXT_WB_BOOKMARK_ADDED:
		case SIDEBAR_CONTEXT_WB_BOOKMARK_REMOVED:
		{
			GtkTreeIter first;
			GtkTreeModel *model;

			model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));
			if (gtk_tree_model_get_iter_first (model, &first))
			{
				sidebar_update_workbench(&first, &position);
			}
		}
		break;
		case SIDEBAR_CONTEXT_FILE_ADDED:
			sidebar_add_file(context->project, context->directory, context->file);
		break;
		case SIDEBAR_CONTEXT_FILE_REMOVED:
			sidebar_remove_file(context->project, context->directory, context->file);
		break;
	}
}


/* Callback function for clicking on a sidebar item */
static void sidebar_filew_view_on_row_activated (GtkTreeView *treeview,
	GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *col, G_GNUC_UNUSED gpointer userdata)
{
	gchar *info;
	GtkTreeModel *model;
	GtkTreeIter  iter;

	model = gtk_tree_view_get_model(treeview);

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		guint dataid;
		gchar *name;
		void  *address;

		gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_NAME, &name, -1);
		gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_DATA_ID, &dataid, -1);
		gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &address, -1);

		switch (dataid)
		{
			case DATA_ID_PROJECT:
				info = wb_project_get_info((WB_PROJECT *)address);
				if (workbench_get_project_status_by_address(wb_globals.opened_wb, address)
					==
					PROJECT_ENTRY_STATUS_OK)
				{
					dialogs_show_msgbox(GTK_MESSAGE_INFO, "%s", info);
				}
				else
				{
					dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("%s\nProject file not found!"), info);
				}
				g_free(info);
			break;
			case DATA_ID_DIRECTORY:
				info = wb_project_dir_get_info((WB_PROJECT_DIR *)address);
				dialogs_show_msgbox(GTK_MESSAGE_INFO, "%s", info);
			break;
			case DATA_ID_WB_BOOKMARK:
			case DATA_ID_PRJ_BOOKMARK:
			case DATA_ID_FILE:
				document_open_file((char *)address, FALSE, NULL, NULL);
			break;
			case DATA_ID_NO_DIRS:
				dialogs_show_msgbox(GTK_MESSAGE_INFO, _("This project has no directories. Directories can be added to a project using the context menu."));
			break;
			default:
			break;
		}

		g_free(name);
	}
}


/* Callbac function for button release, used for the popup menu */
static gboolean sidebar_file_view_on_button_release(G_GNUC_UNUSED GtkWidget * widget, GdkEventButton * event,
		G_GNUC_UNUSED gpointer user_data)
{
	if (event->button == 3)
	{
		SIDEBAR_CONTEXT context;
		POPUP_CONTEXT popup_context = POPUP_CONTEXT_BACKGROUND;

		if (sidebar_file_view_get_selected_context(&context))
		{
			if (context.file != NULL)
			{
				popup_context = POPUP_CONTEXT_FILE;
			}
			else if (context.subdir != NULL)
			{
				popup_context = POPUP_CONTEXT_SUB_DIRECTORY;
			}
			else if (context.directory != NULL)
			{
				popup_context = POPUP_CONTEXT_DIRECTORY;
			}
			else if (context.prj_bookmark != NULL)
			{
				popup_context = POPUP_CONTEXT_PRJ_BOOKMARK;
			}
			else if (context.project != NULL)
			{
				popup_context = POPUP_CONTEXT_PROJECT;
			}
			else if (context.wb_bookmark != NULL)
			{
				popup_context = POPUP_CONTEXT_WB_BOOKMARK;
			}
		}
		popup_menu_show(popup_context, event);

		return TRUE;
	}

	return FALSE;
}


/** Get the selected project and the path to it.
 *
 * Get the selected project and return a pointer to it. Also if @a path is not NULL then
 * the path to the row containing the project is returned in *path.
 *
 * @param path @nullable Location to return the path or @c NULL
 *
 **/
WB_PROJECT *sidebar_file_view_get_selected_project(GtkTreePath **path)
{
	gboolean has_parent;
	guint dataid;
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter current, parent;
	WB_PROJECT *project;

	if (path != NULL)
	{
		*path = NULL;
	}
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(sidebar.file_view));
	if (gtk_tree_selection_get_selected(treesel, &model, &current))
	{
		do
		{
			gtk_tree_model_get(model, &current, FILEVIEW_COLUMN_DATA_ID, &dataid, -1);
			gtk_tree_model_get(model, &current, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &project, -1);
			if (dataid == DATA_ID_PROJECT && project != NULL)
			{
				if (path != NULL)
				{
					*path = gtk_tree_model_get_path(model, &current);
				}
				return project;
			}
			has_parent = gtk_tree_model_iter_parent(model,
							&parent, &current);
			current = parent;
		}while (has_parent);
	}
	return NULL;
}


/* Search upwards from the selected row and get the iter of the
   closest parent with DATA_ID id. */
static gboolean sidebar_file_view_get_selected_parent_iter(GtkTreeIter *iter, guint id)
{
	gboolean has_parent;
	guint dataid;
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter current, parent;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(sidebar.file_view));
	if (gtk_tree_selection_get_selected(treesel, &model, &current))
	{
		do
		{
			gtk_tree_model_get(model, &current, FILEVIEW_COLUMN_DATA_ID, &dataid, -1);
			if (dataid == id)
			{
				*iter = current;
				return TRUE;
			}
			has_parent = gtk_tree_model_iter_parent(model,
							&parent, &current);
			current = parent;
		}while (has_parent);
	}
	return FALSE;
}


/** Get the selected project directory and the path to it.
 *
 * Get the selected project directory and return a pointer to it. Also if @a path is not NULL then
 * the path to the row containing the project is returned in *path.
 *
 * @param path @nullable Location to return the path or @c NULL
 *
 **/
static WB_PROJECT_DIR *sidebar_file_view_get_selected_project_dir(GtkTreePath **path)
{
	gboolean has_parent;
	guint dataid;
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter current, parent;
	WB_PROJECT_DIR *directory;

	if (path != NULL)
	{
		*path = NULL;
	}
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(sidebar.file_view));
	if (gtk_tree_selection_get_selected(treesel, &model, &current))
	{
		do
		{
			gtk_tree_model_get(model, &current, FILEVIEW_COLUMN_DATA_ID, &dataid, -1);
			gtk_tree_model_get(model, &current, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &directory, -1);
			if (dataid == DATA_ID_DIRECTORY && directory != NULL)
			{
				if (path != NULL)
				{
					*path = gtk_tree_model_get_path(model, &current);
				}
				return directory;
			}
			has_parent = gtk_tree_model_iter_parent(model,
							&parent, &current);
			current = parent;
		}while (has_parent);
	}
	return NULL;
}


/** Get data corresponding to the current selected tree view selection.
 *
 * The function collects all data corresponding to the current selection and stores it in
 * @a context. This is e.g. the selected file and the directory and project to which it
 * belongs.
 *
 * @param context The location to store the selection context data in.
 * @return TRUE if anything in the tree is selected, FALSE otherwise.
 *
 **/
gboolean sidebar_file_view_get_selected_context(SIDEBAR_CONTEXT *context)
{
	gboolean has_parent;
	guint dataid;
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter current, parent;
	gpointer data;

	if (context == NULL)
	{
		return FALSE;
	}
	memset (context, 0, sizeof(*context));

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(sidebar.file_view));
	if (gtk_tree_selection_get_selected(treesel, &model, &current))
	{
		/* Search through the parents upwards until we find the project node.
		   Save everthing that's interesting in callers variables... */
		do
		{
			gtk_tree_model_get(model, &current, FILEVIEW_COLUMN_DATA_ID, &dataid, -1);
			gtk_tree_model_get(model, &current, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &data, -1);
			if (data != NULL)
			{
				switch (dataid)
				{
					case DATA_ID_WB_BOOKMARK:
						context->wb_bookmark = data;
					break;
					case DATA_ID_PROJECT:
						context->project = data;
					break;
					case DATA_ID_PRJ_BOOKMARK:
						context->prj_bookmark = data;
					break;
					case DATA_ID_DIRECTORY:
						context->directory = data;
					break;
					case DATA_ID_NO_DIRS:
						/* Has not got any data. */
					break;
					case DATA_ID_SUB_DIRECTORY:
						if (context->subdir == NULL)
						{
							context->subdir = data;
						}
					break;
					case DATA_ID_FILE:
						context->file = data;
					break;
				}
			}

			has_parent = gtk_tree_model_iter_parent(model,
							&parent, &current);
			current = parent;
		}while (has_parent);

		return TRUE;
	}
	return FALSE;
}

/* Collect all filenames recursively starting from iter and add them to list */
static void sidebar_get_filelist_for_iter(GPtrArray *list, GtkTreeIter iter, gboolean dirnames)
{
	GtkTreeModel *model;
	GtkTreeIter childs;
	gboolean has_next;
	guint dataid;
	char *filename;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));
	do
	{
		gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_DATA_ID, &dataid, -1);
		switch (dataid)
		{
			case DATA_ID_FILE:
				gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &filename, -1);
				g_ptr_array_add(list, g_strdup(filename));
			break;
			case DATA_ID_DIRECTORY:
			case DATA_ID_SUB_DIRECTORY:
				if (dirnames == TRUE)
				{
					gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &filename, -1);
					g_ptr_array_add(list, g_strdup(filename));
				}
				if (gtk_tree_model_iter_children(model, &childs, &iter) == TRUE)
				{
					sidebar_get_filelist_for_iter(list, childs, dirnames);
				}
			break;
		}
		has_next = gtk_tree_model_iter_next(model, &iter);
	}
	while (has_next);
}


/* Get the lkist of files belonging to the current selection for
   id (id = project, directory, sub-directory) */
static GPtrArray *sidebar_get_selected_filelist (guint id, gboolean dirnames)
{
	GtkTreeModel *model;
	GPtrArray *list;
	GtkTreeIter iter, childs;

	if (sidebar_file_view_get_selected_parent_iter(&iter, id))
	{
		list = g_ptr_array_new_full(1, g_free);
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));
		if (gtk_tree_model_iter_children(model, &childs, &iter) == TRUE)
		{
			sidebar_get_filelist_for_iter(list, childs, dirnames);
		}
		return list;
	}

	return NULL;
}


/** Get the list of files corresponding to the selected project.
 * 
 * @return GPtrArray containing file names or NULL.
 *
 **/
GPtrArray *sidebar_get_selected_project_filelist (gboolean dirnames)
{
	return sidebar_get_selected_filelist(DATA_ID_PROJECT, dirnames);
}


/** Get the list of files corresponding to the selected directory.
 * 
 * @return GPtrArray containing file names or NULL.
 *
 **/
GPtrArray *sidebar_get_selected_directory_filelist (gboolean dirnames)
{
	return sidebar_get_selected_filelist(DATA_ID_DIRECTORY, dirnames);
}


/** Get the list of files corresponding to the selected sub-directory.
 * 
 * @return GPtrArray containing file names or NULL.
 *
 **/
GPtrArray *sidebar_get_selected_subdir_filelist (gboolean dirnames)
{
	return sidebar_get_selected_filelist(DATA_ID_SUB_DIRECTORY, dirnames);
}

/** Setup the sidebar.
 *
 **/
void sidebar_init(void)
{
	GtkWidget *scrollwin;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;
	GList *focus_chain = NULL;

	sidebar.file_view_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_set_name(sidebar.file_view_vbox, "workbench");

	/**** label ****/
	sidebar.file_view_label = gtk_label_new (_("No workbench opened."));
	gtk_box_pack_start(GTK_BOX(sidebar.file_view_vbox), sidebar.file_view_label, FALSE, FALSE, 0);
	
	/**** placeholder ****/
	PangoAttrList *attrList = pango_attr_list_new ();
	pango_attr_list_insert(attrList, pango_attr_style_new(PANGO_STYLE_ITALIC));
	sidebar.file_view_placeholder = gtk_label_new (NULL);
	gtk_label_set_line_wrap(GTK_LABEL(sidebar.file_view_placeholder), TRUE);
	gtk_label_set_attributes(GTK_LABEL(sidebar.file_view_placeholder), attrList);
	pango_attr_list_unref(attrList);
	gtk_widget_hide(sidebar.file_view_placeholder);
	gtk_widget_set_opacity(sidebar.file_view_placeholder, 0.75);
	gtk_box_pack_start(GTK_BOX(sidebar.file_view_vbox), sidebar.file_view_placeholder, FALSE, FALSE, 0);

	/**** tree view ****/

	sidebar.file_view = gtk_tree_view_new();
	g_signal_connect(sidebar.file_view, "row-activated", (GCallback)sidebar_filew_view_on_row_activated, NULL);

	sidebar.file_store = gtk_tree_store_new(FILEVIEW_N_COLUMNS, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_POINTER);
	gtk_tree_view_set_model(GTK_TREE_VIEW(sidebar.file_view), GTK_TREE_MODEL(sidebar.file_store));

	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "gicon", FILEVIEW_COLUMN_ICON);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", FILEVIEW_COLUMN_NAME);

	gtk_tree_view_append_column(GTK_TREE_VIEW(sidebar.file_view), column);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(sidebar.file_view), FALSE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(sidebar.file_view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(sidebar.file_view), FILEVIEW_COLUMN_NAME);

	ui_widget_modify_font_from_string(sidebar.file_view,
		wb_globals.geany_plugin->geany_data->interface_prefs->tagbar_font);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(sidebar.file_view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(sidebar.file_view), "button-release-event",
			G_CALLBACK(sidebar_file_view_on_button_release), NULL);

	sidebar_deactivate();

	/**** the rest ****/
	focus_chain = g_list_prepend(focus_chain, sidebar.file_view);
	gtk_container_set_focus_chain(GTK_CONTAINER(sidebar.file_view_vbox), focus_chain);
	g_list_free(focus_chain);
	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
					   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrollwin), sidebar.file_view);
	gtk_box_pack_start(GTK_BOX(sidebar.file_view_vbox), scrollwin, TRUE, TRUE, 0);

	gtk_widget_show_all(sidebar.file_view_vbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(wb_globals.geany_plugin->geany_data->main_widgets->sidebar_notebook),
				 sidebar.file_view_vbox, gtk_label_new(_("Workbench")));
}


/** Display a text message in the sidebar.
 *
 * The function displays the text @a msg in the sidebar and eventually deactivates the sidebar
 * if @a activate is FALSE.
 *
 * @param msg      The text to display
 * @param activate If TRUE the sidebar will be activated, if not it will be deactivated
 *
 **/
void sidebar_show_intro_message(const gchar *msg, gboolean activate)
{
	GtkTreeIter iter;

	sidebar_reset_tree_store();
	gtk_label_set_text(GTK_LABEL(sidebar.file_view_placeholder), msg);
	gtk_widget_show(sidebar.file_view_placeholder);
	if (activate)
	{
		sidebar_activate();
	}
	else
	{
		sidebar_deactivate();
	}
}


/** Activate the sidebar.
 *
 **/
void sidebar_activate(void)
{
	gtk_widget_set_sensitive(sidebar.file_view_vbox, TRUE);
}


/** Deactivate the sidebar.
 *
 **/
void sidebar_deactivate(void)
{
	gtk_widget_set_sensitive(sidebar.file_view_vbox, FALSE);
}


/** Cleanup the sidebar.
 *
 **/
void sidebar_cleanup(void)
{
	gtk_widget_destroy(sidebar.file_view_vbox);
}


/** Expand all rows in the sidebar tree.
 *
 **/
void sidebar_expand_all(void)
{
	gtk_tree_view_expand_all(GTK_TREE_VIEW(sidebar.file_view));
}


/** Collapse all rows in the sidebar tree.
 *
 **/
void sidebar_collapse_all(void)
{
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(sidebar.file_view));
}


/** Expand all rows in the sidebar tree for the selected project.
 *
 **/
void sidebar_expand_selected_project(void)
{
	GtkTreePath *path;

	sidebar_file_view_get_selected_project(&path);
	if (path != NULL)
	{
		gtk_tree_view_expand_row(GTK_TREE_VIEW(sidebar.file_view),
			path, TRUE);
		gtk_tree_path_free(path);
	}
}


/** Collapse all rows in the sidebar tree for the selected project.
 *
 **/
void sidebar_collapse_selected_project(void)
{
	GtkTreePath *path;

	sidebar_file_view_get_selected_project(&path);
	if (path != NULL)
	{
		gtk_tree_view_collapse_row(GTK_TREE_VIEW(sidebar.file_view),
			path);
		gtk_tree_path_free(path);
	}
}


/** Toggle state of all rows in the sidebar tree for the selected project.
 *
 * If the project is expanded, then it will be collapsed and vice versa.
 *
 **/
void sidebar_toggle_selected_project_expansion(void)
{
	GtkTreePath *path;

	sidebar_file_view_get_selected_project(&path);
	if (path != NULL)
	{
		if (gtk_tree_view_row_expanded
		        (GTK_TREE_VIEW(sidebar.file_view),path))
		{
			gtk_tree_view_collapse_row(GTK_TREE_VIEW(sidebar.file_view),
										path);
		}
		else
		{
			gtk_tree_view_expand_row (GTK_TREE_VIEW(sidebar.file_view),
										path, TRUE);
		}
		gtk_tree_path_free(path);
	}
}


/** Toggle state of all rows in the sidebar tree for the selected directory.
 *
 * If the directory is expanded, then it will be collapsed and vice versa.
 *
 **/
void sidebar_toggle_selected_project_dir_expansion (void)
{
	GtkTreePath *path;

	sidebar_file_view_get_selected_project_dir(&path);
	if (path != NULL)
	{
		if (gtk_tree_view_row_expanded
			(GTK_TREE_VIEW(sidebar.file_view),path))
		{
			gtk_tree_view_collapse_row (GTK_TREE_VIEW(sidebar.file_view),
										path);
		}
		else
		{
			gtk_tree_view_expand_row (GTK_TREE_VIEW(sidebar.file_view),
										path, TRUE);
		}
		gtk_tree_path_free(path);
	}
}


/* Helper function to set context according to given parameters. */
static void sidebar_set_context (SIDEBAR_CONTEXT *context, guint dataid, gpointer data)
{
	if (data != NULL)
	{
		switch (dataid)
		{
			case DATA_ID_WB_BOOKMARK:
				memset(context, 0, sizeof(*context));
				context->wb_bookmark = data;
			break;
			case DATA_ID_PROJECT:
				memset(context, 0, sizeof(*context));
				context->project = data;
			break;
			case DATA_ID_PRJ_BOOKMARK:
				context->prj_bookmark = data;
				context->directory = NULL;
				context->subdir = NULL;
				context->file = NULL;
			break;
			case DATA_ID_DIRECTORY:
				context->directory = data;
				context->subdir = NULL;
				context->file = NULL;
			break;
			case DATA_ID_NO_DIRS:
				/* Has not got any data. */
			break;
			case DATA_ID_SUB_DIRECTORY:
				context->subdir = data;
				context->file = NULL;
			break;
			case DATA_ID_FILE:
				context->file = data;
			break;
		}
	}
}


/* Internal call foreach function. Traverses the whole sidebar. */
void sidebar_call_foreach_int(SB_CALLFOREACH_CONTEXT *foreach_cntxt,
							  GtkTreeIter *iter)
{
	guint currentid;
	gpointer current;
	GtkTreeIter children;

	do
	{
		gtk_tree_model_get(foreach_cntxt->model, iter, FILEVIEW_COLUMN_DATA_ID, &currentid, -1);
		gtk_tree_model_get(foreach_cntxt->model, iter, FILEVIEW_COLUMN_ASSIGNED_DATA_POINTER, &current, -1);

		sidebar_set_context(foreach_cntxt->context, currentid, current);
		if (currentid == foreach_cntxt->dataid)
		{
			/* Found requested node. Call callback function. */
			foreach_cntxt->func(foreach_cntxt->context, foreach_cntxt->userdata);
		}

		/* If the node has childs then check them to. */
		if (gtk_tree_model_iter_children (foreach_cntxt->model, &children, iter))
		{
			sidebar_call_foreach_int(foreach_cntxt, &children);
		}
	}while (gtk_tree_model_iter_next(foreach_cntxt->model, iter));
}


/** Call a callback function for each matching node.
 *
 * This function traverses the complete sidebar tree and calls func each time
 * a node with dataid is found. The function is passed the sidebar context
 * for the current position in hte tree and the given userdata.
 * 
 * @param dataid   Which nodes are of interest?
 * @param func     Callback function
 * @param userdata Pointer to user data. Is transparently passed through
 *                 on calling func.
 *
 **/
void sidebar_call_foreach(guint dataid,
						  void (*func)(SIDEBAR_CONTEXT *, gpointer userdata),
						  gpointer userdata)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	SIDEBAR_CONTEXT context;
	SB_CALLFOREACH_CONTEXT foreach_cntxt;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.file_view));
	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		foreach_cntxt.context = &context;
		foreach_cntxt.model = model;
		foreach_cntxt.dataid = dataid;
		foreach_cntxt.func = func;
		foreach_cntxt.userdata = userdata;
		sidebar_call_foreach_int(&foreach_cntxt, &iter);
	}
}
