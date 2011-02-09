/*
 *      ltree.c
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
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
 * 		Contains locals tree view functions.
 */

#include <gtk/gtk.h>

#include "watch_model.h"
#include "vtree.h"

/* pointer to a widget */
static GtkWidget *tree = NULL;

/*
 * init locals tree, and sets on expanded handler
 * arguments:
 * 		expanded - handler to call when tree item is expanded
 */
GtkWidget* ltree_init(watch_expanded_callback expanded, watch_button_pressed buttonpressed)
{
	tree = vtree_create(NULL, NULL);
	g_signal_connect(G_OBJECT(tree), "row-expanded", G_CALLBACK (expanded), NULL);
	g_signal_connect(G_OBJECT(tree), "button-press-event", G_CALLBACK (buttonpressed), NULL);
		
	return tree;
}
