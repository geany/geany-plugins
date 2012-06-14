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

#ifndef GUI_H
#define GUI_H

#include <glib.h>
#include <gtk/gtk.h>

GtkWidget*	create_button(const gchar *icon, const gchar *tooltip);
GtkWidget*	create_stock_button(const gchar *stockid, const gchar *tooltip);
GtkWidget*	create_toggle_button(const gchar *stockid, const gchar *tooltip);

void			set_button_image(GtkWidget *btn, const gchar *icon);

#endif /* guard */
