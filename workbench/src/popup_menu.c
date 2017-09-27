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
#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include "wb_globals.h"
#include "dialogs.h"
#include "sidebar.h"
#include "popup_menu.h"

static struct
{
	GtkWidget *widget;

	GtkWidget *add_project;
	GtkWidget *save_project;
	GtkWidget *remove_project;
	GtkWidget *fold_unfold_project;
	GtkWidget *add_directory;
	GtkWidget *remove_directory;
	GtkWidget *rescan_directory;
	GtkWidget *directory_settings;
	GtkWidget *fold_unfold_directory;
	GtkWidget *expand_all;
	GtkWidget *collapse_all;
	GtkWidget *add_wb_bookmark;
	GtkWidget *add_prj_bookmark;
	GtkWidget *remove_bookmark;
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
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
		break;
		case POPUP_CONTEXT_DIRECTORY:
		case POPUP_CONTEXT_FOLDER:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
		break;
		case POPUP_CONTEXT_FILE:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
		break;
		case POPUP_CONTEXT_BACKGROUND:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, FALSE);
		break;
		case POPUP_CONTEXT_WB_BOOKMARK:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, TRUE);
		break;
		case POPUP_CONTEXT_PRJ_BOOKMARK:
			gtk_widget_set_sensitive (s_popup_menu.add_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_project, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.add_directory, TRUE);
			gtk_widget_set_sensitive (s_popup_menu.remove_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.rescan_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.directory_settings, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.fold_unfold_directory, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_wb_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.add_prj_bookmark, FALSE);
			gtk_widget_set_sensitive (s_popup_menu.remove_bookmark, TRUE);
		break;
	}
	gtk_menu_popup(GTK_MENU(s_popup_menu.widget), NULL, NULL, NULL, NULL,
				   event->button, event->time);
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
	}
	else
	{
		dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Could not add project file: %s"), filename);
	}
	g_free(filename);
}


/* Handle popup menu item "Save project" */
static void popup_menu_on_save_project(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GError *error = NULL;
	WB_PROJECT *project;

	project = sidebar_file_view_get_selected_project(NULL);
	if (project != NULL && wb_globals.opened_wb != NULL)
	{
		if (wb_project_save(project, &error))
		{
			SIDEBAR_CONTEXT context;

			memset(&context, 0, sizeof(context));
			context.project = project;
			sidebar_update(SIDEBAR_CONTEXT_PROJECT_SAVED, &context);
		}
	}
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
		}
	}
}


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
		if (dialogs_directory_settings(context.directory))
		{
			wb_project_set_modified(context.project, TRUE);
			wb_project_dir_rescan(context.project, context.directory);
			sidebar_update(SIDEBAR_CONTEXT_DIRECTORY_SETTINGS_CHANGED, &context);
		}
	}
}


/* Handle popup menu item "Directory settings" */
static void popup_menu_on_fold_unfold_directory(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	sidebar_toggle_selected_project_dir_expansion();
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
	}
	if (sidebar_file_view_get_selected_context(&context)
		&& context.project != NULL && context.prj_bookmark != NULL)
	{
		wb_project_remove_bookmark(context.project, context.prj_bookmark);
		sidebar_update(SIDEBAR_CONTEXT_PRJ_BOOKMARK_REMOVED, &context);
	}
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

	item = gtk_menu_item_new_with_mnemonic(_("_Save project"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_save_project), NULL);
	s_popup_menu.save_project = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Remove project"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_remove_project), NULL);
	s_popup_menu.remove_project = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Fold/unfold project"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_menu_on_fold_unfold_project), NULL);
	s_popup_menu.fold_unfold_project = item;

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
}
