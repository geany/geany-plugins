/*
 *      lineoperations.c - Line operations, remove duplicate lines, empty lines, lines with only whitespace, sort lines.
 *
 *      Copyright 2015 Sylvan Mostert <smostert.dev@gmail.com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "config.h"

#include "geanyplugin.h"
#include "Scintilla.h"
#include "linefunctions.h"


static GtkWidget *main_menu_item = NULL;


/* call custom func */
static void
action_func_item(GtkMenuItem *menuitem, gpointer gdata)
{
	void (*func)(GeanyDocument *) = gdata;
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) func(doc);
}


/* Sort Lines */
static void
action_sort_item(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) sortlines(doc, GPOINTER_TO_INT(gdata));
}


static gboolean
lo_init(GeanyPlugin *plugin, gpointer gdata)
{
	GeanyData *geany_data = plugin->geany_data;

	GtkWidget *submenu;
	guint i;
	struct {
		const gchar *label;
		GCallback cb_activate;
		gpointer cb_data;
	} menu_items[] = {
		{ N_("Remove Duplicate Lines, _Sorted"),
		  G_CALLBACK(action_func_item), (gpointer) rmdupst },
		{ N_("Remove Duplicate Lines, _Ordered"),
		  G_CALLBACK(action_func_item), (gpointer) rmdupln },
		{ N_("Remove _Unique Lines"),
		  G_CALLBACK(action_func_item), (gpointer) rmunqln },
		{ NULL },
		{ N_("Remove _Empty Lines"),
		  G_CALLBACK(action_func_item), (gpointer) rmemtyln },
		{ N_("Remove _Whitespace Lines"),
		  G_CALLBACK(action_func_item), (gpointer) rmwhspln },
		{ NULL },
		{ N_("Sort Lines _Ascending"),
		  G_CALLBACK(action_sort_item), GINT_TO_POINTER(1) },
		{ N_("Sort Lines _Descending"),
		  G_CALLBACK(action_sort_item), GINT_TO_POINTER(0) }
	};

	main_menu_item = gtk_menu_item_new_with_mnemonic(_("_Line Operations"));
	gtk_widget_show(main_menu_item);

	submenu = gtk_menu_new();
	gtk_widget_show(submenu);

	for (i = 0; i < G_N_ELEMENTS(menu_items); i++)
	{
		GtkWidget *item;

		if (! menu_items[i].label) /* separator */
			item = gtk_separator_menu_item_new();
		else
		{
			item = gtk_menu_item_new_with_mnemonic(_(menu_items[i].label));
			g_signal_connect(item, "activate", menu_items[i].cb_activate, menu_items[i].cb_data);
			ui_add_document_sensitive(item);
		}

		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	}

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_menu_item), submenu);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);

	return TRUE;
}


static void
lo_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	if(main_menu_item) gtk_widget_destroy(main_menu_item);
}


G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	plugin->info->name        = _("Line Operations");
	plugin->info->description = _("Line Operations provides a handful of functions that can be applied to a document such as, removing duplicate lines, removing empty lines, removing lines with only whitespace, and sorting lines.");
	plugin->info->version     = "0.1";
	plugin->info->author      = "Sylvan Mostert <smostert.dev@gmail.com>";

	plugin->funcs->init       = lo_init;
	plugin->funcs->cleanup    = lo_cleanup;

	GEANY_PLUGIN_REGISTER(plugin, 225);
}
