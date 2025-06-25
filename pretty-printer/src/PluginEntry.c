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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "PluginEntry.h"
#include <errno.h>


GeanyPlugin*           geany_plugin;
GeanyData*             geany_data;

/*========================================== PLUGIN INFORMATION ==========================================================*/

PLUGIN_VERSION_CHECK(247)
PLUGIN_SET_TRANSLATABLE_INFO(
    LOCALEDIR,
    GETTEXT_PACKAGE,
    _("XML PrettyPrinter"),
    _("Formats an XML and makes it human-readable. \nThis plugin currently "
      "has no maintainer. Would you like to help by contributing to this plugin?"),
    PRETTY_PRINTER_VERSION, "Cédric Tabin - http://www.astorm.ch")

/*========================================== DECLARATIONS ================================================================*/

static GtkWidget* main_menu_item = NULL; /*the main menu of the plugin*/

/* declaration of the functions */
static void xml_format(GtkMenuItem *menuitem, gpointer gdata);
static void kb_run_xml_pretty_print(G_GNUC_UNUSED guint key_id);
static void config_closed(GtkWidget* configWidget, gint response, gpointer data);

/*========================================== FUNCTIONS ===================================================================*/

static gchar *
get_config_file (void)
{
    gchar *dir;
    gchar *fn;

    dir = g_build_filename (geany_data->app->configdir, "plugins", "pretty-printer", NULL);
    fn = g_build_filename (dir, "prefs.conf", NULL);

    if (! g_file_test (fn, G_FILE_TEST_IS_DIR))
    {
        if (g_mkdir_with_parents (dir, 0755) != 0)
        {
            g_critical ("failed to create config dir '%s': %s", dir, g_strerror (errno));
            g_free (dir);
            g_free (fn);
            return NULL;
        }
    }

    g_free (dir);

    if (! g_file_test (fn, G_FILE_TEST_EXISTS))
    {
        GError *error = NULL;
        const gchar *def_config;

        def_config = getDefaultPrefs(&error);
        if (def_config == NULL)
        {
            g_critical ("failed to fetch default config data (%s)",
                        error->message);
            g_error_free (error);
            g_free (fn);
            return NULL;
        }
        if (!g_file_set_contents (fn, def_config, -1, &error))
        {
            g_critical ("failed to save default config to file '%s': %s",
                        fn, error->message);
            g_error_free (error);
            g_free (fn);
            return NULL;
        }
    }

    return fn;
}

void plugin_init(GeanyData *data)
{
    gchar         *conf_file;
    GError        *error = NULL;
    GeanyKeyGroup *key_group;

    /* load preferences */
    conf_file = get_config_file ();
    if (!prefsLoad (conf_file, &error))
    {
        g_critical ("failed to load preferences file '%s': %s", conf_file, error->message);
        g_error_free (error);
    }
    g_free (conf_file);

    /* initializes the libxml2 */
    LIBXML_TEST_VERSION

    /* put the menu into the Tools */
    main_menu_item = gtk_menu_item_new_with_mnemonic(_("PrettyPrinter XML"));
    ui_add_document_sensitive(main_menu_item);

    gtk_widget_show(main_menu_item);
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);

    /* init keybindings */
    key_group = plugin_set_key_group(geany_plugin, "prettyprinter", 1, NULL);
    keybindings_set_item(key_group, 0, kb_run_xml_pretty_print,
                         0, 0, "run_pretty_printer_xml", _("Run the PrettyPrinter XML"),
                         main_menu_item);

    /* add activation callback */
    g_signal_connect(main_menu_item, "activate", G_CALLBACK(xml_format), NULL);
}

void plugin_cleanup(void)
{
    /* destroys the plugin */
    gtk_widget_destroy(main_menu_item);
}

GtkWidget* plugin_configure(GtkDialog * dialog)
{
    /* creates the configuration widget */
    GtkWidget* widget = createPrettyPrinterConfigUI(dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(config_closed), NULL);
    return widget;
}

/*========================================== LISTENERS ===================================================================*/

void config_closed(GtkWidget* configWidget, gint response, gpointer gdata)
{
    /* if the user clicked OK or APPLY, then save the settings */
    if (response == GTK_RESPONSE_OK ||
        response == GTK_RESPONSE_APPLY)
    {
        gchar* conf_file;
        GError* error = NULL;

        conf_file = get_config_file ();
        if (! prefsSave (conf_file, &error))
        {
            g_critical ("failed to save preferences to file '%s': %s", conf_file, error->message);
            g_error_free (error);
        }
        g_free (conf_file);
    }
}

void kb_run_xml_pretty_print(G_GNUC_UNUSED guint key_id)
{
    xml_format(NULL, NULL);
}

void xml_format(GtkMenuItem* menuitem, gpointer gdata)
{
    /* retrieves the current document */
    GeanyDocument* doc = document_get_current();
    GeanyEditor* editor;
    ScintillaObject* sco;
    int input_length;
    gboolean has_selection;
    gchar* input_buffer;
    int output_length;
    gchar* output_buffer;
    xmlDoc* parsedDocument;
    int result;
    int xOffset;
    GeanyFiletype* fileType;

    g_return_if_fail(doc != NULL);

    editor = doc->editor;
    sco = editor->sci;

    /* default printing options */
    if (prettyPrintingOptions == NULL) { prettyPrintingOptions = createDefaultPrettyPrintingOptions(); }

    has_selection = sci_has_selection(sco);
    /* retrieves the text */
    input_buffer = (has_selection)?sci_get_selection_contents(sco):sci_get_contents(sco, -1);

    /* checks if the data is an XML format */
    parsedDocument = xmlParseDoc((const unsigned char*)input_buffer);

    /* this is not a valid xml => exit with an error message */
    if(parsedDocument == NULL)
    {
        g_free(input_buffer);
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Unable to parse the content as XML."));
        return;
    }

    /* free all */
    xmlFreeDoc(parsedDocument);

    /* process pretty-printing */
    input_length = (has_selection)?sci_get_selected_text_length2(sco):sci_get_length(sco);
    result = processXMLPrettyPrinting(input_buffer, input_length, &output_buffer, &output_length, prettyPrintingOptions);
    if (result != PRETTY_PRINTING_SUCCESS)
    {
        g_free(input_buffer);
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Unable to process PrettyPrinting on the specified XML because some features are not supported.\n\nSee Help > Debug messages for more details..."));
        return;
    }

    /* updates the document */
     if(has_selection){
        sci_replace_sel(sco, output_buffer);
    }
    else{
        sci_set_text(sco, output_buffer);
    }

    /* set the line */
    xOffset = scintilla_send_message(sco, SCI_GETXOFFSET, 0, 0);
    scintilla_send_message(sco, SCI_LINESCROLL, -xOffset, 0); /* TODO update with the right function-call for geany-0.19 */

    /* sets the type */
    if(!has_selection && (doc->file_type->id != GEANY_FILETYPES_HTML)){
        fileType = filetypes_index(GEANY_FILETYPES_XML);
        document_set_filetype(doc, fileType);
    }

    g_free(output_buffer);
}
