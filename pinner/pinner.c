/*
 * pinner.c
 *
 * Copyright 2024 Andy Alt <arch_stanton5995@proton.me>
 * With assistance from ChatGPT
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include <geanyplugin.h>

enum {
	DO_PIN,
	DO_UNPIN
};

static void destroy_widget(gpointer pdata);
static void clear_pinned_documents(void);
static GtkWidget *create_popup_menu(const gchar *file_name);
static gboolean on_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean is_duplicate(const gchar* file_name);
static void pin_activate_cb(GtkMenuItem *menuitem, gpointer pdata);
static void unpin_activate_cb(GtkMenuItem *menuitem, gpointer pdata);

static GtkWidget *pinned_view_vbox;
static gint page_number = 0;
static GHashTable *doc_to_widget_map = NULL;

static void destroy_widget(gpointer pdata)
{
	GtkWidget *widget = (GtkWidget *)pdata;
	gtk_widget_destroy(widget);
}


void clear_pinned_documents(void)
{
	if (doc_to_widget_map != NULL)
	{
		// Removes all keys and their associated values from the hash table.
		// This will also call the destroy functions specified for keys and values,
		// thus freeing the memory for the file names and destroying the widgets.
		g_hash_table_remove_all(doc_to_widget_map);
	}
}


static GtkWidget *create_popup_menu(const gchar *file_name) {
	GtkWidget *menu = gtk_menu_new();

	// Create a menu item without a label
	GtkWidget *clear_item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), clear_item);
	g_signal_connect_swapped(clear_item, "activate", G_CALLBACK(clear_pinned_documents), NULL);

	// Create a box to contain the icon and label
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); // 6 pixels spacing

	// Create the icon
	GtkWidget *clear_icon = gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_MENU);

	// Create the label
	GtkWidget *label = gtk_label_new("Clear List");

	// Pack the icon and label into the box
	gtk_box_pack_start(GTK_BOX(hbox), clear_icon, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	// Add the box to the menu item
	gtk_container_add(GTK_CONTAINER(clear_item), hbox);

	// Show all widgets
	gtk_widget_show_all(clear_item);

	if (file_name != NULL)
 	{
		// Create a menu item without a label for unpinning a document
		GtkWidget *unpin_item = gtk_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), unpin_item);
		g_signal_connect_data(unpin_item, "activate", G_CALLBACK(unpin_activate_cb), g_strdup(file_name), (GClosureNotify)g_free, 0);

		// Create a horizontal box to hold the icon and label
		GtkWidget *hbox_unpin = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); // 6 pixels spacing

		// Create the icon
		GtkWidget *unpin_icon = gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_MENU);

		// Create the label
		GtkWidget *label_unpin = gtk_label_new("Unpin Document");

		// Pack the icon and label into the horizontal box
		gtk_box_pack_start(GTK_BOX(hbox_unpin), unpin_icon, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox_unpin), label_unpin, FALSE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(unpin_item), hbox_unpin);

		gtk_widget_show_all(unpin_item);
	}
	gtk_widget_show_all(menu);

	return menu;
}


static gboolean is_duplicate(const gchar* file_name)
{
	return g_hash_table_contains(doc_to_widget_map, file_name);
}


static void pin_document_key_cb(guint key_id)
{
	pin_activate_cb(NULL, NULL);
}

static void unpin_document_key_cb(guint key_id)
{
	unpin_activate_cb(NULL, NULL);
}


static void pin_activate_cb(GtkMenuItem *menuitem, gpointer pdata)
{
	gchar *ptr_file_name = NULL;
	if (pdata == NULL)
	{
		GeanyDocument *doc = document_get_current();
		if (doc == NULL)
			return;
		else
			ptr_file_name = doc->file_name;
	}
	else
	{
		ptr_file_name = pdata;
	}

	if (is_duplicate(ptr_file_name))
		return;

	/* This must be freed when nodes are removed from the list */
	gchar *tmp_file_name = g_strdup(ptr_file_name);

	GtkWidget *event_box = gtk_event_box_new();
	g_hash_table_insert(doc_to_widget_map, tmp_file_name, event_box);

	GtkWidget *label = gtk_label_new(ptr_file_name);
	// Enable ellipsizing at the start of the filename
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_START);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 30);
	// Set the label's alignment to left
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	// Set margins
	gtk_widget_set_margin_start(label, 10);  // 20 pixels margin on the start (left)
	gtk_widget_set_margin_end(label, 10);    // 20 pixels margin on the end (right)

	gtk_container_add(GTK_CONTAINER(event_box), label);
	gtk_widget_show_all(event_box);
	gtk_box_pack_start(GTK_BOX(pinned_view_vbox), event_box, FALSE, FALSE, 0);
	// gtk_notebook_set_current_page(GTK_NOTEBOOK(plugin->geany_data->main_widgets->sidebar_notebook), page_number);

	g_signal_connect(event_box, "button-press-event",
		G_CALLBACK(on_button_press_cb), tmp_file_name);

	return;
}


static void unpin_activate_cb(GtkMenuItem *menuitem, gpointer pdata)
{
	GeanyDocument *doc = document_get_current();
	if (doc == NULL)
		return;

	gboolean removed = g_hash_table_remove(doc_to_widget_map, doc->file_name);
	// If removed
	if (!removed)
	{
		// Handle the case where the document was not found in the map
	}

	return;
}


static gboolean on_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY)
	{
		// Check if the clicked widget is an event box
		if (GTK_IS_EVENT_BOX(widget))
		{
			GtkWidget *label = gtk_bin_get_child(GTK_BIN(widget));

			if (GTK_IS_LABEL(label))
			{
				const gchar *file_name = gtk_label_get_text(GTK_LABEL(label));
				document_open_file(file_name, FALSE, NULL, NULL);
			}
		}
	}
	else if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY)
	{
		// Check if the clicked widget is an event box
		if (GTK_IS_EVENT_BOX(widget))
		{
			GtkWidget *label = gtk_bin_get_child(GTK_BIN(widget));

			if (GTK_IS_LABEL(label))
			{
				const gchar *file_name = gtk_label_get_text(GTK_LABEL(label));
				GtkWidget *menu = create_popup_menu(file_name);
				gtk_menu_popup_at_pointer(GTK_MENU(menu), (const GdkEvent *)event);
				return TRUE;
			}
		}
		else
		{
			GtkWidget *menu = create_popup_menu(NULL);
			gtk_menu_popup_at_pointer(GTK_MENU(menu), (const GdkEvent *)event);
			return TRUE;
		}
	}
	return FALSE;
}


static gboolean pin_init(GeanyPlugin *plugin, gpointer pdata)
{
	GtkWidget **tools_item = g_new0(GtkWidget*, 3); // Allocate memory for 3 pointers (2 items + NULL terminator)
	tools_item[DO_PIN] = gtk_menu_item_new_with_mnemonic("Pin Document");
	tools_item[DO_UNPIN] = gtk_menu_item_new_with_mnemonic("Unpin Document");
	tools_item[2] = NULL; // NULL sentinel

	doc_to_widget_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, destroy_widget);

	gtk_widget_show(tools_item[DO_PIN]);
	gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu),
		tools_item[DO_PIN]);
	g_signal_connect(tools_item[DO_PIN], "activate",
		G_CALLBACK(pin_activate_cb), NULL);

	gtk_widget_show(tools_item[DO_UNPIN]);
	gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu),
		tools_item[DO_UNPIN]);
	g_signal_connect(tools_item[DO_UNPIN], "activate",
		G_CALLBACK(unpin_activate_cb), NULL);

	//g_signal_connect(event_box, "button-press-event",
		//G_CALLBACK(on_button_press), NULL);
	//g_signal_connect(pinned_view_vbox, "button-press-event",
		//G_CALLBACK(on_button_press), NULL);

	geany_plugin_set_data(plugin, tools_item, NULL);

	pinned_view_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show_all(pinned_view_vbox);
	page_number = gtk_notebook_append_page(GTK_NOTEBOOK(plugin->geany_data->main_widgets->sidebar_notebook),
		pinned_view_vbox, gtk_label_new(_("Pinned")));

	// Keybinding setup
	GeanyKeyGroup *key_group = plugin_set_key_group(plugin, "pinner_keys", 2, NULL);
	keybindings_set_item(key_group, DO_PIN, pin_document_key_cb, 0, 0, "pin_document", "Pin Document", NULL);
	keybindings_set_item(key_group, DO_UNPIN, unpin_document_key_cb, 0, 0, "unpin_document", "Unpin Document", NULL);

	return TRUE;
}


static void pin_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	if (doc_to_widget_map != NULL)
	{
		g_hash_table_destroy(doc_to_widget_map);
		doc_to_widget_map = NULL;
	}

	GtkWidget **tools_item = pdata;
	while (*tools_item != NULL) {
		gtk_widget_destroy(*tools_item);
		tools_item++;
	}
	g_free(pdata);
}


G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	plugin->info->name = "Pinner";
	plugin->info->description = "Pin a document";
	plugin->info->version = "0.1.0";
	plugin->info->author = "Andy Alt <arch_stanton5995@proton.me>";

	plugin->funcs->init = pin_init;
	plugin->funcs->cleanup = pin_cleanup;

	GEANY_PLUGIN_REGISTER(plugin, 225);
}
