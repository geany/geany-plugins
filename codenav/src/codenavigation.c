/*
 *      codenavigation.c - this file is part of "codenavigation", which is
 *      part of the "geany-plugins" project.
 *
 *      Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/**
 * Code navigation plugin - plugin which adds facilities for navigating between files.
 * 2009 Lionel Fuentes.
 */

#include "codenavigation.h"
#include "switch_head_impl.h"
#include "goto_file.h"

/************************* Global variables ***************************/

static GtkWidget* edit_menu_item_separator = NULL;

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;

/* Check that the running Geany supports the plugin API used below, and check
 * for binary compatibility. */
PLUGIN_VERSION_CHECK(112)

/* All plugins must set name, description, version and author. */
PLUGIN_SET_INFO(_("Code navigation"),
	_(	"This plugin adds features to facilitate navigation between source files.\n"
		"As for the moment, it implements :\n"
		"- switching between a .cpp file and the corresponding .h file\n"
		"- [opening a file by typing its name -> TODO]"), CODE_NAVIGATION_VERSION, "Lionel Fuentes")

/* Declare "GeanyKeyGroupInfo plugin_key_group_info[1]" and "GeanyKeyGroup *plugin_key_group",
 * for Geany to find the keybindings */
PLUGIN_KEY_GROUP(code_navigation, NB_KEY_IDS)

/***************************** Functions ******************************/

static void
on_configure_response(GtkDialog *dialog, gint response, gpointer user_data);

/* ---------------------------------------------------------------------
 * Called by Geany to initialize the plugin.
 * Note: data is the same as geany_data.
 * ---------------------------------------------------------------------
 */
void plugin_init(GeanyData *data)
{
	GtkWidget* edit_menu = ui_lookup_widget(geany->main_widgets->window, "edit1_menu");

	log_func();

	/* Add a separator to the "Edit" menu : */
	edit_menu_item_separator = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(edit_menu), edit_menu_item_separator);

	/* Initialize the features */
	switch_head_impl_init();
	goto_file_init();
}

/* ---------------------------------------------------------------------
 * Called by Geany to show the plugin's configure dialog. This function
 * is always called after plugin_init() is called.
 * ---------------------------------------------------------------------
 */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	log_func();

	GtkWidget *vbox;

	vbox = gtk_vbox_new(FALSE, 6);

	/* Switch header/implementation widget */
	gtk_box_pack_start(GTK_BOX(vbox), switch_head_impl_config_widget(), TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);

	/* Connect a callback for when the user clicks a dialog button */
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}

/* ---------------------------------------------------------------------
 * Called by Geany before unloading the plugin.
 * ---------------------------------------------------------------------
 */
void plugin_cleanup(void)
{
	GtkWidget* edit_menu = ui_lookup_widget(geany->main_widgets->window, "edit1_menu");

	log_func();

	/* Cleanup the features */
	goto_file_cleanup();
	switch_head_impl_cleanup();

	/* Remove the separator in the "Edit" menu */
	gtk_widget_destroy(edit_menu_item_separator);
}

/* ---------------------------------------------------------------------
 * Callback called when validating the configuration of the plug-in
 * ---------------------------------------------------------------------
 */
static void
on_configure_response(GtkDialog* dialog, gint response, gpointer user_data)
{
	/* TODO */
#if 0
	GKeyFile* key_file = NULL;
	gchar* config_dir = NULL;
	gchar* config_filename = NULL;

	if(response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		/* Write the settings into a file, using GLib's GKeyFile API.
		 * File name :
		 * geany->app->configdir G_DIR_SEPARATOR_S "plugins" G_DIR_SEPARATOR_S "codenav" G_DIR_SEPARATOR_S "codenav.conf"
		 * e.g. this could be: ~/.config/geany/plugins/codenav/codenav.conf
		 */

		/* Open the GKeyFile */
		key_file = g_key_file_new();

		config_dir = g_strconcat(geany->app->configdir,
			G_DIR_SEPARATOR_S "plugins" G_DIR_SEPARATOR_S "codenav" G_DIR_SEPARATOR_S, NULL);

		config_filename = g_strconcat(config_dir, "codenav.conf", NULL);

		/* Load configuration */
		g_key_file_load_from_file(key_file, config_filename, G_KEY_FILE_NONE, NULL);

		/* Write configuration */
		write_switch_head_impl_config(key_file);

		/* Cleanup */
		g_free(config_filename);
		g_free(config_dir);
		g_key_file_free(key_file);
	}
#endif
}
