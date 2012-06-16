/*
 *      btnpanel.h
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

#ifndef BTNPANEL_H
#define BTNPANEL_H

#include <glib.h>
#include <gtk/gtk.h>

#include "debug_module.h"

typedef void (*on_toggle)(GtkToggleButton *button, gpointer user_data);

GtkWidget*		btnpanel_create(on_toggle cb);
void			btnpanel_set_debug_state(enum dbs state);

#endif /* guard */
