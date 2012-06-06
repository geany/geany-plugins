/*
 *
 *		tabs.c
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

#ifndef TABS_H
#define TABS_H

typedef enum _tab_id
{
	TID_TARGET,
	TID_BREAKS,
	TID_WATCH,
	TID_AUTOS,
	TID_STACK,
	TID_TERMINAL,
	TID_MESSAGES
} tab_id;

extern GtkWidget *tab_target;
extern GtkWidget *tab_breaks;
extern GtkWidget *tab_watch;
extern GtkWidget *tab_autos;
extern GtkWidget *tab_call_stack;
extern GtkWidget *tab_terminal;
extern GtkWidget *tab_messages;

GtkWidget*		tabs_get_tab(tab_id id);
tab_id			tabs_get_tab_id(GtkWidget* tab);
const gchar*	tabs_get_label(tab_id id);

#endif /* guard */
