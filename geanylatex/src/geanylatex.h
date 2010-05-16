 /*
 *      geanylatex.h
 *
 *      Copyright 2008-2010 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

/* LaTeX plugin */
/* This plugin improves the work with LaTeX and Geany.*/

#ifndef GEANYLATEX_H
#define GEANYLATEX_H

#include "geanyplugin.h"
#include <gtk/gtk.h>
#include "datatypes.h"
#include "templates.h"
#include "letters.h"
#include "latexencodings.h"
#include "bibtex.h"
#include "latexutils.h"
#include "reftex.h"
#include "latexenvironments.h"
#include "formatutils.h"
#include "latexstructure.h"
#include "latexkeybindings.h"

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include <string.h>
typedef void (*MenuCallback) (G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata);


extern GeanyPlugin	*geany_plugin;
extern GeanyData	*geany_data;
extern GeanyFunctions	*geany_functions;


#define create_sub_menu(base_menu, menu, item, title) \
		(menu) = gtk_menu_new(); \
		(item) = gtk_menu_item_new_with_mnemonic(_(title)); \
		gtk_menu_item_set_submenu(GTK_MENU_ITEM((item)), (menu)); \
		gtk_container_add(GTK_CONTAINER(base_menu), (item)); \
		gtk_widget_show((item));

#define MAX_MENU_ENTRIES 20

extern LaTeXWizard glatex_wizard;
gint glatex_count_menu_entries(SubMenuTemplate *tmp, gint categorie);
void glatex_wizard_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
	 G_GNUC_UNUSED gpointer gdata);
void glatex_insert_label_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
	 G_GNUC_UNUSED gpointer gdata);
void glatex_insert_ref_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
	 G_GNUC_UNUSED gpointer gdata);
void glatex_replace_special_character();
void glatex_insert_usepackage_dialog(G_GNUC_UNUSED GtkMenuItem * menuitem,
	 G_GNUC_UNUSED gpointer gdata);
void glatex_insert_command_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
	 G_GNUC_UNUSED gpointer gdata);
#endif
