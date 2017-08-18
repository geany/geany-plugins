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

extern GeanyPlugin *geany_plugin;

typedef struct
{
	GtkWidget *menu;
	GtkWidget *root_item;
	GtkWidget *item_new;
	GtkWidget *item_open;
	GtkWidget *item_save;
	GtkWidget *item_settings;
	GtkWidget *item_close;
}WB_MENU_DATA;
static WB_MENU_DATA menu_data;


/* The function handles the menu item "New workbench" */
static void item_new_workbench_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
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
		menu_item_new_deactivate();
		menu_item_open_deactivate();
		menu_item_save_activate();
		menu_item_settings_activate();
		menu_item_close_activate();
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
static void item_open_workbench_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
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
		menu_item_new_deactivate();
		menu_item_open_deactivate();
		menu_item_save_activate();
		menu_item_settings_activate();
		menu_item_close_activate();
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


/* The function handles the menu item "Save workbench" */
static void item_save_workbench_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	GError *error = NULL;

	if (wb_globals.opened_wb != NULL)
	{
		if (!workbench_save(wb_globals.opened_wb, &error))
		{
			dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Could not save workbench file: %s"), error->message);
		}
		sidebar_update(SIDEBAR_CONTEXT_WB_SAVED, NULL);
	}
}


/* The function handles the menu item "Settings" */
static void item_workbench_settings_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	if (wb_globals.opened_wb != NULL)
	{
		if (dialogs_workbench_settings(wb_globals.opened_wb))
		{
			sidebar_update(SIDEBAR_CONTEXT_WB_SETTINGS_CHANGED, NULL);
		}
	}
}


/* The function handles the menu item "Close workbench" */
static void item_close_workbench_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	workbench_free(wb_globals.opened_wb);
	wb_globals.opened_wb = NULL;

	menu_item_new_activate();
	menu_item_open_activate();
	menu_item_save_deactivate();
	menu_item_settings_deactivate();
	menu_item_close_deactivate();

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
	menu_data.item_new = gtk_menu_item_new_with_mnemonic(_("_New"));
	gtk_widget_show(menu_data.item_new);
	gtk_menu_append(GTK_MENU (menu_data.menu), menu_data.item_new);
	g_signal_connect(menu_data.item_new, "activate",
					 G_CALLBACK(item_new_workbench_activate_cb), NULL);
	geany_plugin_set_data(wb_globals.geany_plugin, menu_data.item_new, NULL);

	/* Create new menu item "Open Workbench" */
	menu_data.item_open = gtk_menu_item_new_with_mnemonic(_("_Open"));
	gtk_widget_show(menu_data.item_open);
	gtk_menu_append(GTK_MENU (menu_data.menu), menu_data.item_open);
	g_signal_connect(menu_data.item_open, "activate",
					 G_CALLBACK(item_open_workbench_activate_cb), NULL);
	geany_plugin_set_data(wb_globals.geany_plugin, menu_data.item_open, NULL);

	/* Create new menu item "Save Workbench" */
	menu_data.item_save = gtk_menu_item_new_with_mnemonic(_("_Save"));
	gtk_widget_show(menu_data.item_save);
	gtk_menu_append(GTK_MENU (menu_data.menu), menu_data.item_save);
	g_signal_connect(menu_data.item_save, "activate",
					 G_CALLBACK(item_save_workbench_activate_cb), NULL);
	geany_plugin_set_data(wb_globals.geany_plugin, menu_data.item_save, NULL);

	/* Create new menu item "Workbench Settings" */
	menu_data.item_settings = gtk_menu_item_new_with_mnemonic(_("S_ettings"));
	gtk_widget_show(menu_data.item_settings);
	gtk_menu_append(GTK_MENU (menu_data.menu), menu_data.item_settings);
	g_signal_connect(menu_data.item_settings, "activate",
					 G_CALLBACK(item_workbench_settings_activate_cb), NULL);
	geany_plugin_set_data(wb_globals.geany_plugin, menu_data.item_settings, NULL);

	/* Create new menu item "Close Workbench" */
	menu_data.item_close = gtk_menu_item_new_with_mnemonic(_("_Close"));
	gtk_widget_show(menu_data.item_close);
	gtk_menu_append(GTK_MENU (menu_data.menu), menu_data.item_close);
	g_signal_connect(menu_data.item_close, "activate",
					 G_CALLBACK(item_close_workbench_activate_cb), NULL);
	geany_plugin_set_data(wb_globals.geany_plugin, menu_data.item_close, NULL);

	/* Add our menu to the main window (left of the help menu) */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_data.root_item), menu_data.menu);
	gtk_menu_shell_insert
		(GTK_MENU_SHELL (ui_lookup_widget(wb_globals.geany_plugin->geany_data->main_widgets->window, "menubar1")),
		 menu_data.root_item, 8);

	return TRUE;
}


/** Cleanup menu data/mem.
 *
 **/
void menu_cleanup (void)
{
	gtk_container_remove (GTK_CONTAINER(ui_lookup_widget(wb_globals.geany_plugin->geany_data->main_widgets->window, "menubar1")),
						  menu_data.root_item);
}


/** Activate menu item "New".
 *
 **/
void menu_item_new_activate (void)
{
	gtk_widget_set_sensitive(menu_data.item_new, TRUE);
}


/** Deactivate menu item "New".
 *
 **/
void menu_item_new_deactivate (void)
{
	gtk_widget_set_sensitive(menu_data.item_new, FALSE);
}


/** Activate menu item "Open".
 *
 **/
void menu_item_open_activate (void)
{
	gtk_widget_set_sensitive(menu_data.item_open, TRUE);
}


/** Deactivate menu item "Open".
 *
 **/
void menu_item_open_deactivate (void)
{
	gtk_widget_set_sensitive(menu_data.item_open, FALSE);
}


/** Activate menu item "Save".
 *
 **/
void menu_item_save_activate (void)
{
	gtk_widget_set_sensitive(menu_data.item_save, TRUE);
}


/** Deactivate menu item "Save".
 *
 **/
void menu_item_save_deactivate (void)
{
	gtk_widget_set_sensitive(menu_data.item_save, FALSE);
}


/** Activate menu item "Settings".
 *
 **/
void menu_item_settings_activate (void)
{
	gtk_widget_set_sensitive(menu_data.item_settings, TRUE);
}

/** Deactivate menu item "Settings".
 *
 **/
void menu_item_settings_deactivate (void)
{
	gtk_widget_set_sensitive(menu_data.item_settings, FALSE);
}


/** Activate menu item "Close".
 *
 **/
void menu_item_close_activate (void)
{
	gtk_widget_set_sensitive(menu_data.item_close, TRUE);
}


/** Deactivate menu item "Close".
 *
 **/
void menu_item_close_deactivate (void)
{
	gtk_widget_set_sensitive(menu_data.item_close, FALSE);
}
