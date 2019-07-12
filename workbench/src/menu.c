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
 * Code for the "Workbench" menu.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "wb_globals.h"
#include "dialogs.h"
#include "menu.h"
#include "sidebar.h"
#include "search_projects.h"

extern GeanyPlugin *geany_plugin;

typedef struct
{
	GtkWidget *menu;
	GtkWidget *root_item;
	GtkWidget *item_new;
	GtkWidget *item_open;
	GtkWidget *item_settings;
	GtkWidget *item_search_projects;
	GtkWidget *item_close;
}WB_MENU_DATA;
static WB_MENU_DATA menu_data;

/** Set the context of the workbench menu.
 *
 * The context set controls which items in the menu will be active and 
 * which will be deactive.
 *
 * @param context The context/situation in which the menu is/shall be
 *
 **/
void menu_set_context(MENU_CONTEXT context)
{
	switch (context)
	{
		case MENU_CONTEXT_WB_CREATED:
		case MENU_CONTEXT_WB_OPENED:
			gtk_widget_set_sensitive(menu_data.item_new, FALSE);
			gtk_widget_set_sensitive(menu_data.item_open, FALSE);
			gtk_widget_set_sensitive(menu_data.item_settings, TRUE);
			gtk_widget_set_sensitive(menu_data.item_search_projects, TRUE);
			gtk_widget_set_sensitive(menu_data.item_close, TRUE);
		break;
		case MENU_CONTEXT_SEARCH_PROJECTS_SCANING:
			gtk_widget_set_sensitive(menu_data.item_new, FALSE);
			gtk_widget_set_sensitive(menu_data.item_open, FALSE);
			gtk_widget_set_sensitive(menu_data.item_settings, TRUE);
			gtk_widget_set_sensitive(menu_data.item_search_projects, FALSE);
			gtk_widget_set_sensitive(menu_data.item_close, FALSE);
		break;
		case MENU_CONTEXT_WB_CLOSED:
			gtk_widget_set_sensitive(menu_data.item_new, TRUE);
			gtk_widget_set_sensitive(menu_data.item_open, TRUE);
			gtk_widget_set_sensitive(menu_data.item_settings, FALSE);
			gtk_widget_set_sensitive(menu_data.item_search_projects, FALSE);
			gtk_widget_set_sensitive(menu_data.item_close, FALSE);
		break;
	}
}

/* The function handles the menu item "New workbench" */
static void item_new_workbench_activate_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *filename;
	GError *error = NULL;

	filename = dialogs_create_new_workbench();
	if (filename == NULL)
	{
		return;
	}
	wb_globals.opened_wb = workbench_new();
	workbench_set_filename(wb_globals.opened_wb, filename);
	if (workbench_save(wb_globals.opened_wb, &error))
	{
		menu_set_context(MENU_CONTEXT_WB_CREATED);
		sidebar_update(SIDEBAR_CONTEXT_WB_CREATED, NULL);
	}
	else
	{
		dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Could not create new workbench file: %s"), error->message);
		workbench_free(wb_globals.opened_wb);
		wb_globals.opened_wb = NULL;
	}
	g_free(filename);
}


/* The function handles the menu item "Open workbench" */
static void item_open_workbench_activate_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *filename;
	GError *error = NULL;

	filename = dialogs_open_workbench();
	if (filename == NULL)
	{
		return;
	}
	wb_globals.opened_wb = workbench_new();
	if (workbench_load(wb_globals.opened_wb, filename, &error))
	{
		menu_set_context(MENU_CONTEXT_WB_OPENED);
		sidebar_update(SIDEBAR_CONTEXT_WB_OPENED, NULL);
	}
	else
	{
		dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Could not open workbench file: %s"), error->message);
		workbench_free(wb_globals.opened_wb);
		wb_globals.opened_wb = NULL;
	}
	g_free(filename);
}


/* The function handles the menu item "Settings" */
static void item_workbench_settings_activate_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	if (wb_globals.opened_wb != NULL)
	{
		gboolean enable_live_update_old, enable_live_update;

		enable_live_update_old = workbench_get_enable_live_update(wb_globals.opened_wb);
		if (dialogs_workbench_settings(wb_globals.opened_wb))
		{
			GError *error = NULL;

			sidebar_update(SIDEBAR_CONTEXT_WB_SETTINGS_CHANGED, NULL);

			/* Save the new settings to the workbench file (.geanywb). */
			if (!workbench_save(wb_globals.opened_wb, &error))
			{
				dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Could not save workbench file: %s"), error->message);
			}
			sidebar_update(SIDEBAR_CONTEXT_WB_SAVED, NULL);

			enable_live_update = workbench_get_enable_live_update(wb_globals.opened_wb);
			if (enable_live_update != enable_live_update_old)
			{
				if (enable_live_update == TRUE)
				{
					/* Start/create all file monitors. */
					workbench_enable_live_update(wb_globals.opened_wb);
				}
				else
				{
					/* Stop/free all file monitors. */
					workbench_disable_live_update(wb_globals.opened_wb);
				}
			}
		}
	}
}


/* The function handles the menu item "Search projects" */
static void item_workbench_search_projects_activate_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	if (wb_globals.opened_wb != NULL)
	{
		search_projects(wb_globals.opened_wb);
	}
	sidebar_update(SIDEBAR_CONTEXT_PROJECT_ADDED, NULL);
}


/* The function handles the menu item "Close workbench" */
static void item_close_workbench_activate_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	workbench_free(wb_globals.opened_wb);
	wb_globals.opened_wb = NULL;

	menu_set_context(MENU_CONTEXT_WB_CLOSED);
	sidebar_update(SIDEBAR_CONTEXT_WB_CLOSED, NULL);
}


/** Setup the workbench menu.
 *
 **/
gboolean menu_init(void)
{
	/* Create menu and root item/label */
	menu_data.menu = gtk_menu_new();
	menu_data.root_item = gtk_menu_item_new_with_label(_("Workbench"));
	gtk_widget_show(menu_data.root_item);

	/* Create new menu item "New Workbench" */
	menu_data.item_new = gtk_menu_item_new_with_mnemonic(_("_New..."));
	gtk_widget_show(menu_data.item_new);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu_data.menu), menu_data.item_new);
	g_signal_connect(menu_data.item_new, "activate",
					 G_CALLBACK(item_new_workbench_activate_cb), NULL);

	/* Create new menu item "Open Workbench" */
	menu_data.item_open = gtk_menu_item_new_with_mnemonic(_("_Open..."));
	gtk_widget_show(menu_data.item_open);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu_data.menu), menu_data.item_open);
	g_signal_connect(menu_data.item_open, "activate",
					 G_CALLBACK(item_open_workbench_activate_cb), NULL);

	/* Create new menu item "Workbench Settings" */
	menu_data.item_settings = gtk_menu_item_new_with_mnemonic(_("S_ettings"));
	gtk_widget_show(menu_data.item_settings);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu_data.menu), menu_data.item_settings);
	g_signal_connect(menu_data.item_settings, "activate",
					 G_CALLBACK(item_workbench_settings_activate_cb), NULL);

	/* Create new menu item "Search Projects" */
	menu_data.item_search_projects = gtk_menu_item_new_with_mnemonic(_("Search _projects"));
	gtk_widget_show(menu_data.item_search_projects);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu_data.menu), menu_data.item_search_projects);
	g_signal_connect(menu_data.item_search_projects, "activate",
					 G_CALLBACK(item_workbench_search_projects_activate_cb), NULL);

	/* Create new menu item "Close Workbench" */
	menu_data.item_close = gtk_menu_item_new_with_mnemonic(_("_Close"));
	gtk_widget_show(menu_data.item_close);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu_data.menu), menu_data.item_close);
	g_signal_connect(menu_data.item_close, "activate",
					 G_CALLBACK(item_close_workbench_activate_cb), NULL);

	/* Add our menu to the main window (left of the help menu) */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_data.root_item), menu_data.menu);
	gtk_container_add(GTK_CONTAINER(wb_globals.geany_plugin->geany_data->main_widgets->tools_menu), menu_data.root_item);

	return TRUE;
}


/** Cleanup menu data/mem.
 *
 **/
void menu_cleanup (void)
{
	gtk_widget_destroy(menu_data.root_item);
}
