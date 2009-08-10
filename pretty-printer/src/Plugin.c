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

/*
 * Basic plugin structure, based of Geany Plugin howto :
 *       http://www.geany.org/manual/reference/howto.html
 */

#include <stdlib.h>
#include <stdio.h>
#include <geany.h>
#include <ui_utils.h>
#include <plugindata.h>
#include <editor.h>
#include <document.h>
#include <filetypes.h>
#include <geanyfunctions.h>
#include <Scintilla.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <glib/gmacros.h>
#include "PrettyPrinter.h"

GeanyPlugin*           geany_plugin;
GeanyData*             geany_data;
GeanyFunctions*        geany_functions;
PrettyPrintingOptions* prettyPrintingOptions;

//plugin information
PLUGIN_VERSION_CHECK(130)
PLUGIN_SET_INFO("XML Formatter", "Format an XML file",
                "1.0", "Cédric Tabin - http://www.astorm.ch");

static GtkWidget *main_menu_item = NULL;

static void item_activate_cb(GtkMenuItem *menuitem, gpointer gdata)
{
	//default printing options
	if (prettyPrintingOptions == NULL) { prettyPrintingOptions = createDefaultPrettyPrintingOptions(); }
	
	//retrieves the current document
	GeanyDocument* doc = document_get_current();
	GeanyEditor* editor = doc->editor;
	ScintillaObject* sco = editor->sci;

	//allocate a new pointer
	int length = sci_get_length(sco)+1;
	char* buffer = (char*)malloc(length*sizeof(char));
	if (buffer == NULL) { exit(-1); } //malloc error
	
	//retrieves the text
	sci_get_text(sco, length, buffer);
	
	//checks if the data is an XML format
	xmlDoc* xmlDoc = xmlParseDoc((unsigned char*)buffer);
	
	if(xmlDoc == NULL) //this is not a valid xml
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to parse the content as XML.");
		return;
	}

	//process pretty-printing
	int result = processXMLPrettyPrinting(&buffer, &length, prettyPrintingOptions);
	if (result != 0) 
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Unable to process PrettyPrinting on the specified XML because some features are not supported.");
		return;
	}

	//updates the document
	sci_set_text(sco, buffer);
	
	//sets the type
	GeanyFiletype* fileType = filetypes_index(GEANY_FILETYPES_XML); 
	document_set_filetype(doc, fileType);
	
	//free all
	xmlFreeDoc(xmlDoc);
}

void plugin_init(GeanyData *data)
{
    //Initializes the libxml2
    LIBXML_TEST_VERSION

    //put the menu into the Tools
    main_menu_item = gtk_menu_item_new_with_mnemonic("XML Formatter");
    gtk_widget_show(main_menu_item);
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);

    //add activation callback
    g_signal_connect(main_menu_item, "activate", G_CALLBACK(item_activate_cb), NULL);
}

void plugin_cleanup(void)
{
    //destroys the plugin
    gtk_widget_destroy(main_menu_item);
}

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
