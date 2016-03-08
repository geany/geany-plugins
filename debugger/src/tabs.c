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

/*
 * 		tabs widgets
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

extern GeanyData		*geany_data;

#include "tabs.h"

/* tab widgets */
GtkWidget *tab_target = NULL;
GtkWidget *tab_breaks = NULL;
GtkWidget *tab_watch = NULL;
GtkWidget *tab_autos = NULL;
GtkWidget *tab_call_stack = NULL;
GtkWidget *tab_terminal = NULL;
GtkWidget *tab_messages = NULL;

/*
 *	searches ID for a given widget
 *	arguments:
 * 		tab - widget to search an id for	
 */
tab_id tabs_get_tab_id(GtkWidget* tab)
{
	tab_id id = TID_TARGET;
	if (tab_target == tab)
	{
		id = TID_TARGET;
	}
	else if (tab_breaks == tab)
	{
		id = TID_BREAKS;
	}
	else if (tab_watch == tab)
	{
		id = TID_WATCH;
	}
	else if (tab_autos == tab)
	{
		id = TID_AUTOS;
	}
	else if (tab_call_stack == tab)
	{
		id = TID_STACK;
	}
	else if (tab_terminal == tab)
	{
		id = TID_TERMINAL;
	}
	else if (tab_messages == tab)
	{
		id = TID_MESSAGES;
	}

	return id;
}

/*
 *	searches a widget for a given ID
 *	arguments:
 * 		id - ID to search a widget for	
 */
GtkWidget* tabs_get_tab(tab_id id)
{
	GtkWidget *tab = NULL;
	switch(id)
	{
		case TID_TARGET:
			tab = tab_target;
			break;
		case TID_BREAKS:
			tab = tab_breaks;
			break;
		case TID_WATCH:
			tab = tab_watch;
			break;
		case TID_AUTOS:
			tab = tab_autos;
			break;
		case TID_STACK:
			tab = tab_call_stack;
			break;
		case TID_TERMINAL:
			tab = tab_terminal;
			break;
		case TID_MESSAGES:
			tab = tab_messages;
			break;
	}
	return tab;
}

/*
 *	searches a label for a given ID
 *	arguments:
 * 		id - ID to search a label for	
 */
const gchar* tabs_get_label(tab_id id)
{
	const gchar *label = NULL;
	switch(id)
	{
		case TID_TARGET:
			label = _("Target");
			break;
		case TID_BREAKS:
			label = _("Breakpoints");
			break;
		case TID_WATCH:
			label = _("Watch");
			break;
		case TID_AUTOS:
			label = _("Autos");
			break;
		case TID_STACK:
			label = _("Call Stack");
			break;
		case TID_TERMINAL:
			label = _("Debug Terminal");
			break;
		case TID_MESSAGES:
			label = _("Debugger Messages");
			break;
	}
	return label;
}
