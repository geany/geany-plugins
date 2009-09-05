/**
 *   Copyright (C) 2009  Cedric Tabin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ConfigUI.h"

GtkWidget* createPrettyPrinterConfigUI(GtkDialog* dialog)
{
	//default printing options
	if (prettyPrintingOptions == NULL) { prettyPrintingOptions = createDefaultPrettyPrintingOptions(); }
	
	//TODO create configuration widget
}

void saveSettings()
{
	//TODO save settings into a file
}


/*
GtkWidget* plugin_configure(GtkDialog * dialog)   	
{
	//default printing options
	if (prettyPrintingOptions == NULL) { prettyPrintingOptions = createDefaultPrettyPrintingOptions(); }
	
	GtkWidget* globalBox = gtk_hbox_new(TRUE, 4);
	GtkWidget* rightBox = gtk_vbox_new(FALSE, 6);
	GtkWidget* centerBox = gtk_vbox_new(FALSE, 6);
	GtkWidget* leftBox = gtk_vbox_new(FALSE, 6);
	
	GtkWidget* textLabel = gtk_label_new("Text nodes");
	GtkWidget* textOneLine =   gtk_check_button_new_with_label("One line");
	GtkWidget* textInline =   gtk_check_button_new_with_label("Inline");
	
	GtkWidget* cdataLabel = gtk_label_new("CDATA nodes");
	GtkWidget* cdataOneLine =   gtk_check_button_new_with_label("One line");
	GtkWidget* cdataInline =   gtk_check_button_new_with_label("Inline");
	
	GtkWidget* commentLabel = gtk_label_new("Comments");
	GtkWidget* commentOneLine =   gtk_check_button_new_with_label("One line");
	GtkWidget* commentInline =   gtk_check_button_new_with_label("Inline");
	
	gtk_box_pack_start(GTK_BOX(globalBox), leftBox, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(globalBox), centerBox, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(globalBox), rightBox, FALSE, FALSE, 3);

	gtk_box_pack_start(GTK_BOX(leftBox), textLabel, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(centerBox), textOneLine, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(rightBox), textInline, FALSE, FALSE, 3);

	gtk_box_pack_start(GTK_BOX(leftBox), cdataLabel, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(centerBox), cdataOneLine, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(rightBox), cdataInline, FALSE, FALSE, 3);
	
	gtk_box_pack_start(GTK_BOX(leftBox), commentLabel, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(centerBox), commentOneLine, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(rightBox), commentInline, FALSE, FALSE, 3);

	gtk_widget_show_all(globalBox);
	return globalBox;
}
*/
