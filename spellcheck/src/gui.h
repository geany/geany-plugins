/*
 *      gui.h - this file is part of Spellcheck, a Geany plugin
 *
 *      Copyright 2008-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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


#ifndef SC_GUI_H
#define SC_GUI_H 1



gboolean sc_gui_key_release_cb(GtkWidget *widget, GdkEventKey *ev, gpointer user_data);

gboolean sc_gui_key_release_cb(GtkWidget *widget, GdkEventKey *ev, gpointer data);

void sc_gui_kb_run_activate_cb(guint key_id);

void sc_gui_kb_toggle_typing_activate_cb(guint key_id);

void sc_gui_create_edit_menu(void);

void sc_gui_update_editor_menu_cb(GObject *obj, const gchar *word, gint pos,
								  GeanyDocument *doc, gpointer user_data);

void sc_gui_update_toolbar(void);

void sc_gui_update_menu(void);

void sc_gui_init(void);

void sc_gui_free(void);

#endif
