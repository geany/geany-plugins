/*
 * Copyright 2026 WrapEarnPass
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

#include <geanyplugin.h>

#include "autorun.h"
#include "menu.h"

typedef struct {
	GtkWidget* menu;
	GtkWidget* root_item;
	GtkWidget* item_reload;
} AUTORUN_MENU_DATA;
static AUTORUN_MENU_DATA menu_data;

/* The function handles the menu item "Reload" */
static void autorun_menu_reload_cb(G_GNUC_UNUSED GtkMenuItem* menuitem, G_GNUC_UNUSED gpointer user_data) {
	// force unloading of any loaded project configs
	autorun_cmd_list_free(autorun_globals->project_commands);
	autorun_globals->project_commands = NULL;
	// force unloading of any filedef configs.
	autorun_cmd_list_free(autorun_globals->filedef_commands);
	autorun_globals->filedef_commands = NULL;
	msgwin_status_add(_("Reloading Auto-run filetypes.* configs"));
	// load filedefs
	load_filedefs();
	// load project
	if(autorun_globals->data->app->project && autorun_globals->data->app->project->file_name){
		GKeyFile* config = g_key_file_new();
		gboolean ret = g_key_file_load_from_file(config, autorun_globals->data->app->project->file_name, G_KEY_FILE_NONE, NULL);
		if (ret) {
			msgwin_status_add(_("Reloading Auto-run project configs"));
			load_projectdefs(config);
		}
		g_key_file_free(config);
	}
}

/* initialize the Auto-run Tool menu */
gboolean menu_init(void) {
	// Create menu and root item/label
	menu_data.menu = gtk_menu_new();
	menu_data.root_item = gtk_menu_item_new_with_label("Auto-run");
	gtk_widget_show(menu_data.root_item);

	// Create new menu item "Reload"
	GtkWidget* icon = gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_MENU);
	/*
	 * deprecated since gtk3.10, but it looks like crap using
	 * the "functionally equivalent" menu( box (img,label) )
	 */
	menu_data.item_reload = gtk_image_menu_item_new_with_label(_("Reload"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_data.item_reload), icon);
	gtk_widget_show_all(menu_data.item_reload);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_data.menu), menu_data.item_reload);
	g_signal_connect(menu_data.item_reload, "activate", G_CALLBACK(autorun_menu_reload_cb), NULL);

	/* Add our menu to the main window (left of the help menu) */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_data.root_item), menu_data.menu);
	gtk_container_add(GTK_CONTAINER(autorun_globals->data->main_widgets->tools_menu), menu_data.root_item);

	return TRUE;
}

/* Gtk widget cleanup for Auto-run Tool menu */
void menu_cleanup(void) {
	gtk_widget_destroy(GTK_WIDGET(menu_data.item_reload));
	menu_data.item_reload = NULL;
	gtk_widget_destroy(GTK_WIDGET(menu_data.root_item));
	menu_data.root_item = NULL;
	// since this is attached to main_widgets, attempting to destroy it creates an assertion error
	menu_data.menu = NULL;
}
