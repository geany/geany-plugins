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


#include "geanyplugin.h"
#include "Scintilla.h"
#include "linefunctions.h"


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_INFO(_("LineOperations"),
				 _("Line operations, remove duplicate lines, empty lines, lines with only whitespace, sort lines."),
				 "0.1",
				 "Sylvan Mostert")


static GtkWidget *main_menu_item = NULL;


// Remove Duplicate Lines, sorted
static void
action_rmdupst_item(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) rmdupst(doc);
}


// Remove Duplicate Lines, ordered
static void
action_rmdupln_item(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) rmdupln(doc);
}


// Remove Unique Lines
static void
action_rmunqln_item(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) rmunqln(doc);
}


// Remove Empty Lines
static void
action_rmemtyln_item(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) rmemtyln(doc);
}


// Remove Whitespace Lines
static void
action_rmwhspln_item(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) rmwhspln(doc);
}


// Sort Lines Ascending
static void
action_sortasc_item(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) sortlines(doc, 1);
}


// Sort Lines Descending
static void
action_sortdesc_item(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if(doc) sortlines(doc, 0);
}


void plugin_init(GeanyData *data)
{
	GtkWidget *submenu;
	GtkWidget *sep1;
	GtkWidget *sep2;
	GtkWidget *rmdupst_item;
	GtkWidget *rmdupln_item;
	GtkWidget *rmunqln_item;
	GtkWidget *rmemtyln_item;
	GtkWidget *rmwhspln_item;
	GtkWidget *sortasc_item;
	GtkWidget *sortdesc_item;

	/* Add an item to the Tools menu */
	main_menu_item = gtk_menu_item_new_with_mnemonic(_("Line Operations"));
	gtk_widget_show(main_menu_item);
	ui_add_document_sensitive(main_menu_item);

	submenu = gtk_menu_new();
	gtk_widget_show(submenu);
	sep1 = gtk_separator_menu_item_new();
	sep2 = gtk_separator_menu_item_new();
	rmdupst_item  = gtk_menu_item_new_with_mnemonic(_("Remove Duplicate Lines, Sorted"));
	rmdupln_item  = gtk_menu_item_new_with_mnemonic(_("Remove Duplicate Lines, Ordered"));
	rmunqln_item  = gtk_menu_item_new_with_mnemonic(_("Remove Unique Lines"));
	rmemtyln_item = gtk_menu_item_new_with_mnemonic(_("Remove Empty Lines"));
	rmwhspln_item = gtk_menu_item_new_with_mnemonic(_("Remove Whitespace Lines"));
	sortasc_item  = gtk_menu_item_new_with_mnemonic(_("Sort Lines Ascending"));
	sortdesc_item = gtk_menu_item_new_with_mnemonic(_("Sort Lines Descending"));

	gtk_widget_show(sep1);
	gtk_widget_show(sep2);
	gtk_widget_show(rmdupst_item);
	gtk_widget_show(rmdupln_item);
	gtk_widget_show(rmunqln_item);
	gtk_widget_show(rmemtyln_item);
	gtk_widget_show(rmwhspln_item);
	gtk_widget_show(sortasc_item);
	gtk_widget_show(sortdesc_item);

	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), rmdupst_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), rmdupln_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), rmunqln_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), sep1);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), rmemtyln_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), rmwhspln_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), sep2);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), sortasc_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), sortdesc_item);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_menu_item), submenu);

	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);
	g_signal_connect(rmdupst_item, "activate", G_CALLBACK(action_rmdupst_item), NULL);
	g_signal_connect(rmdupln_item, "activate", G_CALLBACK(action_rmdupln_item), NULL);
	g_signal_connect(rmunqln_item, "activate", G_CALLBACK(action_rmunqln_item), NULL);
	g_signal_connect(rmemtyln_item, "activate", G_CALLBACK(action_rmemtyln_item), NULL);
	g_signal_connect(rmwhspln_item, "activate", G_CALLBACK(action_rmwhspln_item), NULL);
	g_signal_connect(sortasc_item, "activate", G_CALLBACK(action_sortasc_item), NULL);
	g_signal_connect(sortdesc_item, "activate", G_CALLBACK(action_sortdesc_item), NULL);

	ui_add_document_sensitive(rmdupst_item);
	ui_add_document_sensitive(rmdupln_item);
	ui_add_document_sensitive(rmunqln_item);
	ui_add_document_sensitive(rmemtyln_item);
	ui_add_document_sensitive(rmwhspln_item);
	ui_add_document_sensitive(sortasc_item);
	ui_add_document_sensitive(sortdesc_item);
}


void
plugin_cleanup(void)
{
	if(main_menu_item) gtk_widget_destroy(main_menu_item);
}
