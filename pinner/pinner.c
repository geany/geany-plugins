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

static GtkWidget *pinned_view_vbox;
static gint page_number = 0;

struct pindata {
		GeanyPlugin *plugin;
    GSList *list;
};

static struct pindata *init_pindata(void)
{
	static struct pindata container = {
		NULL,
		NULL
		};

	return &container;
}

bool is_duplicate(GSList *list, const gchar* file_name)
{
	GSList *iter;
	for (iter = list; iter != NULL; iter = g_slist_next(iter)) {
		if (g_strcmp0((const gchar *)iter->data, file_name) == 0) {
			/* We'll probably want to alert the user the document already
			 * is pinned */
			return true;
		}
	}
	return false;
}

static void pin_activate_cb(GtkMenuItem *menuitem, gpointer pdata)
{
	struct pindata *container = pdata;
	GeanyPlugin *plugin = container->plugin;
	GSList *list = container->list;

	GeanyDocument *doc = document_get_current();
	if (doc == NULL)
		return;

	// DEBUG: Print the elements in the list
	//printf("List elements:\n");
	//for (GSList *iter = list; iter != NULL; iter = g_slist_next(iter))
		//printf("%s\n", (gchar *)iter->data);

	if (is_duplicate(list, doc->file_name))
		return;

	/* This must be freed when nodes are removed from the list */
	gchar *tmp_file_name = g_strdup(doc->file_name);

	container->list = g_slist_append(list, tmp_file_name);
	GtkWidget *label = gtk_label_new_with_mnemonic(doc->file_name);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(pinned_view_vbox), label, FALSE, FALSE, 0);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(plugin->geany_data->main_widgets->sidebar_notebook), page_number);

	return;
}


static gboolean pin_init(GeanyPlugin *plugin, gpointer pdata)
{
	GtkWidget *main_menu_item;

	struct pindata *container = init_pindata();
	container->plugin = plugin;

	// Create a new menu item and show it
	main_menu_item = gtk_menu_item_new_with_mnemonic("Pin Document");
	gtk_widget_show(main_menu_item);
	gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu),
		main_menu_item);
	g_signal_connect(main_menu_item, "activate",
		G_CALLBACK(pin_activate_cb), container);

	geany_plugin_set_data(plugin, main_menu_item, NULL);

	pinned_view_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show_all(pinned_view_vbox);
	page_number = gtk_notebook_append_page(GTK_NOTEBOOK(plugin->geany_data->main_widgets->sidebar_notebook),
		pinned_view_vbox, gtk_label_new(_("Pinned")));
	return TRUE;
}


static void pin_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	GtkWidget *main_menu_item = (GtkWidget *) pdata;
	//  g_slist_free(list);

	gtk_widget_destroy(main_menu_item);
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
