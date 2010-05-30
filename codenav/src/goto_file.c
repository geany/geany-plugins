/*
 *      goto_file.c - this file is part of "codenavigation", which is
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

#include "goto_file.h"

/******************* Global variables for the feature *****************/

static GtkWidget* menu_item = NULL;

/********************** Functions for the feature *********************/

/* ---------------------------------------------------------------------
 * Callback when the menu item is clicked.
 * ---------------------------------------------------------------------
 */
static void
menu_item_activate(guint key_id);

/* ---------------------------------------------------------------------
 * Initialization
 * ---------------------------------------------------------------------
 */
void
goto_file_init()
{
	log_func();

	GtkWidget* edit_menu = ui_lookup_widget(geany->main_widgets->window, "edit1_menu");

	/* Add the menu item, sensitive only when a document is opened */
	menu_item = gtk_menu_item_new_with_mnemonic(_("Goto file"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(edit_menu), menu_item);
	ui_add_document_sensitive(menu_item);

	/* Callback connection */
	g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(menu_item_activate), NULL);

	/* Initialize the key binding : */
	keybindings_set_item(	plugin_key_group,
 							KEY_ID_GOTO_FILE,
 							(GeanyKeyCallback)(&menu_item_activate),
 							GDK_g, GDK_MOD1_MASK | GDK_SHIFT_MASK,
 							"goto_file",
 							_("Goto file"),	/* used in the Preferences dialog */
 							menu_item);
}

/* ---------------------------------------------------------------------
 * Cleanup
 * ---------------------------------------------------------------------
 */
void
goto_file_cleanup()
{
	log_func();

	gtk_widget_destroy(menu_item);
}

/* ---------------------------------------------------------------------
 * Callback when the menu item is clicked.
 * ---------------------------------------------------------------------
 */
static void
menu_item_activate(guint key_id)
{
	log_func();

	/* TODO */

	GtkWidget *dialog = gtk_message_dialog_new(
		GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		"Open by name : TODO");

	gtk_window_set_title(GTK_WINDOW(dialog), "Goto file...");

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		_("(From the %s plugin)"), geany_plugin->info->name);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}
