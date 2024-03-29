/*
 *      addons.h - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *
 * $Id$
 */


#ifndef ADDONS_H
#define ADDONS_H 1

/* Keybinding(s) */
enum
{
	KB_FOCUS_BOOKMARK_LIST,
	KB_FOCUS_TASKS,
	KB_UPDATE_TASKS,
	KB_XMLTAGGING,
	KB_COPYFILEPATH,
	KB_COUNT
};


extern GeanyPlugin		*geany_plugin;
extern GeanyData		*geany_data;

GtkWidget *ao_image_menu_item_new(const gchar *stock_id, const gchar *label);

#endif
