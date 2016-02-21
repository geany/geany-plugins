/*
 * tableconvert_ui.c
 *
 * Copyright 2014 Frank Lanitz <frank@frank.uvena.de>
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

#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif

#include "tableconvert_ui.h"

GtkWidget *main_menu_item = NULL;
GtkWidget *menu_tableconvert = NULL;
GtkWidget *menu_tableconvert_menu = NULL;

void init_menuentries(void)
{
	int i = 0;
	GtkWidget *tmp = NULL;

	/* Build up menu entry for table_convert based on global file type*/
	main_menu_item = gtk_menu_item_new_with_mnemonic(_("_Convert to table"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);
	gtk_widget_set_tooltip_text(main_menu_item,
		_("Converts current marked list to a table."));
	g_signal_connect(G_OBJECT(main_menu_item), "activate", G_CALLBACK(cb_table_convert), NULL);
	gtk_widget_show_all(main_menu_item);
	ui_add_document_sensitive(main_menu_item);

	/* Build up menu entries for table convert based on explicit choice
	 * This is needed for e.g. different wiki-Syntax or differenz stiles
	 * within a special file type */

	menu_tableconvert = gtk_image_menu_item_new_with_mnemonic(_("_More TableConvert"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_tableconvert);

	menu_tableconvert_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tableconvert),
							  menu_tableconvert_menu);

	for (i = 0; i < TC_END; i++)
	{
		tmp = NULL;
		tmp = gtk_menu_item_new_with_mnemonic(_(tablerules[i].type));
		gtk_container_add(GTK_CONTAINER(menu_tableconvert_menu), tmp);
		g_signal_connect(G_OBJECT(tmp), "activate",
			G_CALLBACK(cb_table_convert_type), GINT_TO_POINTER(i));
	}
	ui_add_document_sensitive(menu_tableconvert);

	/* Show the menu */
	gtk_widget_show_all(menu_tableconvert);
}

