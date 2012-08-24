/*
 *      gui.c
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
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

/*
 * 		function for creating GUI elements
 */

#include <gtk/gtk.h>

#include "gui.h"

/*
 * 	creates a button with an icon from file and a tooltip 
 */
GtkWidget* create_button(const gchar *icon, const gchar *tooltip)
{
	GtkWidget *btn = gtk_button_new();

	gchar *path = g_build_path(G_DIR_SEPARATOR_S, DBGPLUG_DATA_DIR, icon, NULL);
	GtkWidget *image =  gtk_image_new_from_file(path);
	g_free(path);

	gtk_widget_show(image);
	gtk_button_set_image(GTK_BUTTON(btn), image);

	gtk_widget_set_tooltip_text(btn, tooltip);
	
	return btn;
}

/*
 * 	creates a button with a stock icon and a tooltip 
 */
GtkWidget* create_stock_button(const gchar *stockid, const gchar *tooltip)
{
	GtkWidget *btn = gtk_button_new();
	GtkWidget *image = gtk_image_new_from_stock (stockid, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_button_set_image(GTK_BUTTON(btn), image);

	gtk_widget_set_tooltip_text(btn, tooltip);
	
	return btn;
}

/*
 * 	creates a toggle button with an icon from file and a tooltip 
 */
GtkWidget* create_toggle_button(const gchar *icon, const gchar *tooltip)
{
	GtkWidget *btn = gtk_toggle_button_new();

	gchar *path = g_build_path(G_DIR_SEPARATOR_S, DBGPLUG_DATA_DIR, icon, NULL);
	GtkWidget *image =  gtk_image_new_from_file(path);
	g_free(path);

	gtk_widget_show(image);
	gtk_button_set_image(GTK_BUTTON(btn), image);

	gtk_widget_set_tooltip_text(btn, tooltip);
	
	return btn;
}

/*
 * 	sets button icon from file 
 */
void set_button_image(GtkWidget *btn, const gchar *icon)
{
	gchar *path = g_build_path(G_DIR_SEPARATOR_S, DBGPLUG_DATA_DIR, icon, NULL);
	GtkWidget *image =  gtk_image_new_from_file(path);
	g_free(path);

	gtk_widget_show(image);
	gtk_button_set_image(GTK_BUTTON(btn), image);
}
