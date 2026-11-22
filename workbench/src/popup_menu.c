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
 * Code for the popup menu.
 */
#include <errno.h>
#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <project.h>

#include "wb_globals.h"
#include "dialogs.h"
#include "sidebar.h"
#include "popup_menu.h"
#include "utils.h"

static struct
{
	GtkWidget *widget;

	GtkWidget *add_project;
	GtkWidget *remove_project;
	GtkWidget *project_toggle_select;
	GtkWidget *fold_unfold_project;
	GtkWidget *project_open_all;
	GtkWidget *project_close_all;
	GtkWidget *add_directory;
	GtkWidget *remove_directory;
	GtkWidget *rescan_directory;
	GtkWidget *directory_settings;
	GtkWidget *fold_unfold_directory;
	GtkWidget *directory_open_all;
	GtkWidget *directory_close_all;
	GtkWidget *subdir_open_all;
	GtkWidget *subdir_close_all;
	GtkWidget *expand_all;
	GtkWidget *collapse_all;
	GtkWidget *add_wb_bookmark;
	GtkWidget *add_prj_bookmark;
	GtkWidget *remove_bookmark;
	GtkWidget *new_file;
	GtkWidget *new_directory;
	GtkWidget *remove_file_or_dir;
} s_popup_menu;


/** Show the popup menu.
 *
 * The function shows the popup menu. The value of context decides which popup menu items
 * are enabled and which are disabled.
 *
 * @param context The context/situation in which the popup menu was called
 * @param event   Button event.
 *
 **/
void popup_menu_show(POPUP_CONTEXT context, GdkEventButton *event)
{
	switch (context)
	{
		case POPUP_CONTEXT_PROJECT:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, TRUE);
#if GEANY_API_VERSION >= 240
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, TRUE);
#else
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, FALSE);
#endif
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_file, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_file_or_dir, FALSE);
		break;
		case POPUP_CONTEXT_DIRECTORY:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, TRUE);
#if GEANY_API_VERSION >= 240
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, TRUE);
#else
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, FALSE);
#endif
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_file, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.new_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_file_or_dir, TRUE);
		break;
		case POPUP_CONTEXT_SUB_DIRECTORY:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, TRUE);
#if GEANY_API_VERSION >= 240
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, TRUE);
#else
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, FALSE);
#endif
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.new_file, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.new_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_file_or_dir, TRUE);
		break;
		case POPUP_CONTEXT_FILE:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, TRUE);
#if GEANY_API_VERSION >= 240
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, TRUE);
#else
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, FALSE);
#endif
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.new_file, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.new_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_file_or_dir, TRUE);
		break;
		case POPUP_CONTEXT_BACKGROUND:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.project_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.project_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_file, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_file_or_dir, FALSE);
		break;
		case POPUP_CONTEXT_WB_BOOKMARK:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.project_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.project_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_file, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_file_or_dir, FALSE);
		break;
		case POPUP_CONTEXT_PRJ_BOOKMARK:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, TRUE);
#if GEANY_API_VERSION >= 240
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, TRUE);
#else
			gtk_widget_set_sensitive (s_popup_menu.project_toggle_select, FALSE);
#endif
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_open_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.project_close_all, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_open_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.subdir_close_all, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_file, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.new_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_file_or_dir, FALSE);
		break;
	}
#if GTK_CHECK_VERSION(3, 22, 0)
	gtk_menu_popup_at_pointer(GTK_MENU(s_popup_menu.widget), (GdkEvent *)event);
#else
	gtk_menu_popup(GTK_MENU(s_popup_menu.widget), NULL, NULL, NULL, NULL,
				   event->button, event->time);
#endif
}


/* Helper function for saving the project and updating of the sidebar. */
static void save_project(SIDEBAR_CONTEXT *context)
{
	GError *error = NULL;

	if (context->project != NULL && wb_project_save(context->project, &error))
	{
		sidebar_update(SIDEBAR_CONTEXT_PROJECT_SAVED, context);
	}
}


/* Helper function for saving the workbench and updating of the sidebar. */
static void save_workbench(WORKBENCH *wb)
{
	GError *error = NULL;

	/* Save the new settings to the workbench file (.geanywb). */
	if (!workbench_save(wb, &error))
	{
		dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Could not save workbench file: %s"), error->message);
	}
	sidebar_update(SIDEBAR_CONTEXT_WB_SAVED, NULL);
}


/* Handle popup menu item "Add project" */
static void popup_menu_on_add_project(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *filename;

	filename = dialogs_add_project();
	if (filename == NULL || wb_globals.opened_wb == NULL)
	{
		return;
	}

	if (workbench_add_project(wb_globals.opened_wb, filename))
	{
		sidebar_update(SIDEBAR_CONTEXT_PROJECT_ADDED, NULL);

		/* Save the changes to the workbench file (.geanywb). */
		save_workbench(wb_globals.opened_wb);
	}
	else
	{
		dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Could not add project file: %s"), filename);
	}
	g_free(filename);
}


/* Handle popup menu item "Remove project" */
static void popup_menu_on_remove_project(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	WB_PROJECT *project;

	project = sidebar_file_view_get_selected_project(NULL);
	if (project != NULL && wb_globals.opened_wb != NULL)
	{
		if (workbench_remove_project_with_address(wb_globals.opened_wb, project))
		{
			SIDEBAR_CONTEXT context;

			memset(&context, 0, sizeof(context));
			context.project = project;
			sidebar_update(SIDEBAR_CONTEXT_PROJECT_REMOVED, &context);

			/* Save the changes to the workbench file (.geanywb). */
			save_workbench(wb_globals.opened_wb);
		}
	}
}


/* Handle popup menu item "Select/Unselect project" */
#if GEANY_API_VERSION >= 240
static void popup_menu_on_toggle_selected_project(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	WB_PROJECT *project;

	project = sidebar_file_view_get_selected_project(NULL);
	if (project != NULL && wb_globals.opened_wb != NULL)
	{
		if (workbench_get_selected_project(wb_globals.opened_wb) != project)
		{
			workbench_open_project(wb_globals.opened_wb, project, TRUE);
		}
		else
		{
			workbench_open_project(wb_globals.opened_wb, NULL, TRUE);
		}
	}
}
#endif


/* Handle popup menu item "Expand all" */
static void popup_menu_on_expand_all(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	sidebar_expand_all();
}


/* Handle popup menu item "Collapse all" */
static void popup_menu_on_collapse_all(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	sidebar_collapse_all();
}


/* Handle popup menu item "Fold/unfold project" */
static void popup_menu_on_fold_unfold_project(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	sidebar_toggle_selected_project_expansion();
}


/* Handle popup menu item "Open all files" (project) */
static void popup_menu_on_project_open_all (G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GPtrArray *list;

	list = sidebar_get_selected_project_filelist(FALSE);
	if (list != NULL)
	{
		open_all_files_in_list(list);
		g_ptr_array_free(list, TRUE);
	}
}


/* Handle popup menu item "Close all files" (project) */
static void popup_menu_on_project_close_all (G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GPtrArray *list;

	list = sidebar_get_selected_project_filelist(FALSE);
	if (list != NULL)
	{
		close_all_files_in_list(list);
		g_ptr_array_free(list, TRUE);
	}
}


/* Handle popup menu item "Add directory" */
static void popup_menu_on_add_directory(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *dirname;
	WB_PROJECT *project;

	project = sidebar_file_view_get_selected_project(NULL);
	if (project != NULL)
	{
		dirname = dialogs_add_directory(project);
		if (dirname != NULL)
		{
			SIDEBAR_CONTEXT context;

			memset(&context, 0, sizeof(context));
			context.project = project;
			wb_project_add_directory(project, dirname);
			sidebar_update(SIDEBAR_CONTEXT_DIRECTORY_ADDED, &context);
			g_free(dirname);

			/* Save the now changed project. */
			save_project(&context);
		}
	}
}


/* Handle popup menu item "Remove directory" */
static void popup_menu_on_remove_directory(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	SIDEBAR_CONTEXT context;

	if (sidebar_file_view_get_selected_context(&context)
		&& context.project != NULL && context.directory != NULL)
	{
		wb_project_remove_directory(context.project, context.directory);
		sidebar_update(SIDEBAR_CONTEXT_DIRECTORY_REMOVED, &context);

		/* Save the now changed project. */
		save_project(&context);
	}
}


/* Handle popup menu item "Rescan directory" */
static void popup_menu_on_rescan_directory(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	SIDEBAR_CONTEXT context;

	if (sidebar_file_view_get_selected_context(&context)
		&& context.project != NULL && context.directory != NULL)
	{
		wb_project_dir_rescan(context.project, context.directory);
		sidebar_update(SIDEBAR_CONTEXT_DIRECTORY_RESCANNED, &context);
	}
}


/* Handle popup menu item "Directory settings" */
static void popup_menu_on_directory_settings(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	SIDEBAR_CONTEXT context;

	if (sidebar_file_view_get_selected_context(&context)
		&& context.project != NULL && context.directory != NULL)
	{
		if (dialogs_directory_settings(context.project, context.directory))
		{
			wb_project_set_modified(context.project, TRUE);
			wb_project_dir_rescan(context.project, context.directory);
			sidebar_update(SIDEBAR_CONTEXT_DIRECTORY_SETTINGS_CHANGED, &context);

			/* Save the now changed project. */
			save_project(&context);
		}
	}
}


/* Handle popup menu item "Directory settings" */
static void popup_menu_on_fold_unfold_directory(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	sidebar_toggle_selected_project_dir_expansion();
}


/* Handle popup menu item "Open all files" (directory) */
static void popup_menu_on_directory_open_all (G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GPtrArray *list;

	list = sidebar_get_selected_directory_filelist(FALSE);
	if (list != NULL)
	{
		open_all_files_in_list(list);
		g_ptr_array_free(list, TRUE);
	}
}


/* Handle popup menu item "Close all files" (directory) */
static void popup_menu_on_directory_close_all (G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GPtrArray *list;

	list = sidebar_get_selected_directory_filelist(FALSE);
	if (list != NULL)
	{
		close_all_files_in_list(list);
		g_ptr_array_free(list, TRUE);
	}
}


/* Handle popup menu item "Add to workbench bookmarks" */
static void popup_menu_on_add_to_workbench_bookmarks(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	SIDEBAR_CONTEXT context;

	if (sidebar_file_view_get_selected_context(&context)
		&& context.file != NULL)
	{
		workbench_add_bookmark(wb_globals.opened_wb, context.file);
		sidebar_update(SIDEBAR_CONTEXT_WB_BOOKMARK_ADDED, &context);

		/* Save the changes to the workbench file (.geanywb). */
		save_workbench(wb_globals.opened_wb);
	}
}


/* Handle popup menu item "Add to project bookmarks" */
static void popup_menu_on_add_to_project_bookmarks(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	SIDEBAR_CONTEXT context;

	if (sidebar_file_view_get_selected_context(&context)
		&& context.project != NULL && context.file != NULL)
	{
		wb_project_add_bookmark(context.project, context.file);
		sidebar_update(SIDEBAR_CONTEXT_PRJ_BOOKMARK_ADDED, &context);

		/* Save the now changed project. */
		save_project(&context);
	}
}


/* Handle popup menu item "Remove from bookmarks" */
static void popup_menu_on_remove_from_bookmarks(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	SIDEBAR_CONTEXT context;

	if (sidebar_file_view_get_selected_context(&context)
		&& context.wb_bookmark != NULL)
	{
		workbench_remove_bookmark(wb_globals.opened_wb, context.wb_bookmark);
		sidebar_update(SIDEBAR_CONTEXT_WB_BOOKMARK_REMOVED, &context);

		/* Save the changes to the workbench file (.geanywb). */
		save_workbench(wb_globals.opened_wb);
	}
	if (sidebar_file_view_get_selected_context(&context)
		&& context.project != NULL && context.prj_bookmark != NULL)
	{
		wb_project_remove_bookmark(context.project, context.prj_bookmark);
		sidebar_update(SIDEBAR_CONTEXT_PRJ_BOOKMARK_REMOVED, &context);

		/* Save the now changed project. */
		save_project(&context);
	}
}


/* Handle popup menu item "Open all files" (sub-directory) */
static void popup_menu_on_subdir_open_all (G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GPtrArray *list;

	list = sidebar_get_selected_subdir_filelist(FALSE);
	if (list != NULL)
	{
		open_all_files_in_list(list);
		g_ptr_array_free(list, TRUE);
	}
}


/* Handle popup menu item "Close all files" (sub-directory) */
static void popup_menu_on_subdir_close_all (G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GPtrArray *list;

	list = sidebar_get_selected_subdir_filelist(FALSE);
	if (list != NULL)
	{
		close_all_files_in_list(list);
		g_ptr_array_free(list, TRUE);
	}
}


/* Handle popup menu item "Create new file here..." */
static void popup_menu_on_new_file(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *filename, *path = NULL, *abs_path = NULL;
	SIDEBAR_CONTEXT context;

	if (sidebar_file_view_get_selected_context(&context))
	{
		if (context.subdir != NULL)
		{
			path = context.subdir;
			abs_path = g_strdup(path);
		}
		else
		{
			path = wb_project_dir_get_base_dir(context.directory);
			abs_path = get_combined_path(wb_project_get_filename(context.project), path);
		}
	}

	filename = dialogs_create_new_file(abs_path);
	if (filename == NULL)
	{
		return;
	}
	else if (!g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		FILE *new_file;
		new_file = g_fopen (filename, "w");
		if (new_file == NULL)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Could not create new file \"%s\":\n\n%s"), filename, strerror(errno));
		}
		else
		{
			fclose(new_file);

			if (workbench_get_enable_live_update(wb_globals.opened_wb) == FALSE)
			{
				/* Live update is disabled. We need to update the file list and
				   the sidebar manually. */
				wb_project_dir_rescan(context.project, context.directory);
				sidebar_update(SIDEBAR_CONTEXT_DIRECTORY_RESCANNED, &context);
			}

			document_open_file(filename, FALSE, NULL, NULL);
		}
	}

	g_free(abs_path);
	g_free(filename);
}


/* Handle popup menu item "Create new directory here..." */
static void popup_menu_on_new_directory(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *filename, *path = NULL, *abs_path = NULL;
	SIDEBAR_CONTEXT context;

	if (sidebar_file_view_get_selected_context(&context))
	{
		if (context.subdir != NULL)
		{
			path = context.subdir;
			abs_path = g_strdup(path);
		}
		else
		{
			path = wb_project_dir_get_base_dir(context.directory);
			abs_path = get_combined_path(wb_project_get_filename(context.project), path);
		}
	}

	filename = dialogs_create_new_directory(abs_path);
	if (filename != NULL)
	{
		if (workbench_get_enable_live_update(wb_globals.opened_wb) == FALSE)
		{
			/* Live update is disabled. We need to update the file list and
			   the sidebar manually. */
			wb_project_dir_rescan(context.project, context.directory);
			sidebar_update(SIDEBAR_CONTEXT_DIRECTORY_RESCANNED, &context);
		}
	}

	g_free(abs_path);
	g_free(filename);
}


/* Handle popup menu item "Remove..." */
static void popup_menu_on_remove_file_or_dir(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gboolean remove_it, dir, removed_any = FALSE;
	gchar *path = NULL, *abs_path = NULL;
	SIDEBAR_CONTEXT context;
	GPtrArray *files;

	/* Check if a file, a sub-dir or a directory is selected. */
	if (sidebar_file_view_get_selected_context(&context))
	{
		if (context.file != NULL)
		{
			abs_path = g_strdup(context.file);
			dir = FALSE;
		}
		else if (context.subdir != NULL)
		{
			path = context.subdir;
			abs_path = g_strdup(path);
			files = sidebar_get_selected_subdir_filelist(TRUE);
			dir = TRUE;
		}
		else if (context.directory != NULL)
		{
			path = wb_project_dir_get_base_dir(context.directory);
			abs_path = get_combined_path(wb_project_get_filename(context.project), path);
			files = sidebar_get_selected_directory_filelist(TRUE);
			dir = TRUE;
		}
	}

	if (abs_path == NULL)
	{
		return;
	}

	/* Warn the user. */
	if (dir == FALSE)
	{
		remove_it = dialogs_show_question(_("Do you really want to remove file \"%s\"?\n\nThis cannot be undone!"),
			abs_path);
	}
	else
	{
		remove_it = dialogs_show_question(_("Do you really want to remove directory \"%s\" and all files in it?\n\nThis cannot be undone!"),
			abs_path);
	}

	/* Really remove it? */
	if (remove_it)
	{
		if (dir == FALSE)
		{
			if (g_remove(abs_path) != 0)
			{
				/* Remove file failed. Report error. */
				ui_set_statusbar(TRUE, _("Could not remove file \"%s\"."),
					abs_path);
			}
			else
			{
				removed_any = TRUE;
			}
		}
		else
		{
			if (files != NULL)
			{
				guint index;
				gchar *filename;

				/* First remove all files in the directories. */
				for ( index = 0 ; index < files->len ; index++ )
				{
					filename = files->pdata[index];
					if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
					{
						if (g_remove(filename) != 0)
						{
							/* Remove file failed. Report error. */
							ui_set_statusbar(TRUE, _("Could not remove file \"%s\"."),
								filename);
						}
						else
						{
							removed_any = TRUE;
						}
					}
				}

				/* Now try to remove the directories.
				   This will only succeed if they are empty. */
				for ( index = 0 ; index < files->len ; index++ )
				{
					filename = files->pdata[index];
					if (g_file_test(filename, G_FILE_TEST_IS_DIR))
					{
						if (g_rmdir(filename) != 0)
						{
							/* Remove file failed. Report error. */
							ui_set_statusbar(TRUE, _("Could not remove directory \"%s\"."),
								filename);
						}
						else
						{
							removed_any = TRUE;
						}
					}
				}

				/* At last try to remove the parent dir. */
				if (g_rmdir(abs_path) != 0)
				{
					/* Remove file failed. Report error. */
					ui_set_statusbar(TRUE, _("Could not remove directory \"%s\"."),
						abs_path);
				}
				else
				{
					removed_any = TRUE;
				}

				g_ptr_array_free(files, TRUE);
			}
		}

		/* If anything was removed update the filelist and sidebar. */
		if (removed_any)
		{
			wb_project_dir_rescan(context.project, context.directory);
			sidebar_update(SIDEBAR_CONTEXT_DIRECTORY_RESCANNED, &context);
		}
	}

	g_free(abs_path);
}


/** Setup/Initialize the popup menu.
 *
 **/
void popup_menu_init(void)
{
	GtkWidget *item;

	s_popup_menu.widget = gtk_menu_new();

	item = gtk_menu_item_new_with_mnemonic(_("_Add project..."));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_add_project), NULL);
	s_popup_menu.add_project = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Remove project"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_remove_project), NULL);
	s_popup_menu.remove_project = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Select/Unselect project"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
#if GEANY_API_VERSION >= 240
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_toggle_selected_project), NULL);
#endif
	s_popup_menu.project_toggle_select = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Fold/unfold project"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_fold_unfold_project), NULL);
	s_popup_menu.fold_unfold_project = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Open all files in project"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_project_open_all), NULL);
	s_popup_menu.project_open_all = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Close all files in project"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_project_close_all), NULL);
	s_popup_menu.project_close_all = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Add directory..."));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_add_directory), NULL);
	s_popup_menu.add_directory = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Remove directory"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_remove_directory), NULL);
	s_popup_menu.remove_directory = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Rescan directory"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_rescan_directory), NULL);
	s_popup_menu.rescan_directory = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Directory settings"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_directory_settings), NULL);
	s_popup_menu.directory_settings = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Fold/unfold directory"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_fold_unfold_directory), NULL);
	s_popup_menu.fold_unfold_directory = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Open all files in directory"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_directory_open_all), NULL);
	s_popup_menu.directory_open_all = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Close all files in directory"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_directory_close_all), NULL);
	s_popup_menu.directory_close_all = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Open all files in sub-directory"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_subdir_open_all), NULL);
	s_popup_menu.subdir_open_all = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Close all files in sub-directory"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_subdir_close_all), NULL);
	s_popup_menu.subdir_close_all = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Expand all"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_expand_all), NULL);
	s_popup_menu.expand_all = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Collapse all"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_collapse_all), NULL);
	s_popup_menu.collapse_all = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Add to Workbench Bookmarks"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_add_to_workbench_bookmarks), NULL);
	s_popup_menu.add_wb_bookmark = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Add to Project Bookmarks"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_add_to_project_bookmarks), NULL);
	s_popup_menu.add_prj_bookmark = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Remove from Bookmarks"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_remove_from_bookmarks), NULL);
	s_popup_menu.remove_bookmark = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Create file here..."));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_new_file), NULL);
	s_popup_menu.new_file = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Create directory here..."));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_new_directory), NULL);
	s_popup_menu.new_directory = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Remove..."));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_remove_file_or_dir), NULL);
	s_popup_menu.remove_file_or_dir = item;
}
