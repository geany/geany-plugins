/*
 * pinner.c
 *
 * Copyright 2024 Andy Alt <arch_stanton5995@proton.me>
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
#include <stdbool.h>

enum {
	DO_PIN,
	DO_UNPIN
};

static GtkWidget *create_popup_menu(void);
static bool is_duplicate(const gchar* file_name);
static gboolean label_clicked_cb(GtkWidget *widget, GdkEventButton *event, gpointer data);
static void pin_activate_cb(GtkMenuItem *menuitem, gpointer pdata);
static void unpin_activate_cb(GtkMenuItem *menuitem, gpointer pdata);

static GtkWidget *pinned_view_vbox;
static gint page_number = 0;
static GHashTable *doc_to_widget_map = NULL;

void destroy_widget(gpointer pdata)
{
	GtkWidget *widget = (GtkWidget *)pdata;
	gtk_widget_destroy(widget);
}

/* A function adapted (pinched) from filebrowser.c */
static GtkWidget *create_popup_menu(void)
{
	GtkWidget *item, *menu;

	menu = gtk_menu_new();

	/* 3? What should the size be? */
	/* https://docs.gtk.org/gtk3/ctor.Image.new_from_icon_name.html */
	item = gtk_image_new_from_icon_name("edit-clear", 3);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	return menu;
}


static bool is_duplicate(const gchar* file_name) {
    // Check if the file name (document) is already a key in the hash map.
    // g_hash_table_contains returns TRUE if the key is found, FALSE otherwise.
    return g_hash_table_contains(doc_to_widget_map, file_name);
}


static void pin_activate_cb(GtkMenuItem *menuitem, gpointer pdata)
{
	GeanyDocument *doc = document_get_current();
	if (doc == NULL)
		return;

	if (is_duplicate(doc->file_name))
		return;

	/* This must be freed when nodes are removed from the list */
	gchar *tmp_file_name = g_strdup(doc->file_name);
	// pin_list = g_slist_append(pin_list, tmp_file_name);

	GtkWidget *event_box = gtk_event_box_new();
	g_hash_table_insert(doc_to_widget_map, tmp_file_name, event_box);
	GtkWidget *label = gtk_label_new_with_mnemonic(doc->file_name);
	gtk_container_add(GTK_CONTAINER(event_box), label);
	gtk_widget_show_all(event_box);
	gtk_box_pack_start(GTK_BOX(pinned_view_vbox), event_box, FALSE, FALSE, 0);
	// gtk_notebook_set_current_page(GTK_NOTEBOOK(plugin->geany_data->main_widgets->sidebar_notebook), page_number);

	g_signal_connect(event_box, "button-press-event",
		G_CALLBACK(label_clicked_cb), tmp_file_name);

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


static gboolean label_clicked_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 1)
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

	return FALSE;
}


//static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event)
//{
	////if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
	////{
		////on_open_clicked(NULL, NULL);
		////return TRUE;
	////}
	//if (event->button == 3)
	//{
		//static GtkWidget *popup_menu = NULL;

		//if (popup_menu == NULL)
			//popup_menu = create_popup_menu();

		//gtk_menu_popup_at_pointer(GTK_MENU(popup_menu), (GdkEvent *) event);
		///* don't return TRUE here, unless the selection won't be changed */
	//}
	//return FALSE;
//}

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
