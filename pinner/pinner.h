/*
 * pinner.h
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

static struct pindata *init_pindata(void);
void slist_free_wrapper(gpointer pdata);
static GtkWidget *create_popup_menu(gpointer pdata);
bool is_duplicate(GSList *list, const gchar* file_name);
static void pin_activate_cb(GtkMenuItem *menuitem, gpointer pdata);
