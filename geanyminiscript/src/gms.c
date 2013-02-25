/*
 * gms.c: miniscript plugin for geany editor
 *            Geany, a fast and lightweight IDE
 *
 * Copyright 2008 Pascal BURLOT <prublot(at)users(dot)sourceforge(dot)net>
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
 */

/**
 * gms is a tool to apply a script filter on a text selection, or on
 * the whole document, or all opened documents.
 *
 * note: the script filter could be : Unix shell, perl , python , sed ,awk ...
 */

#include    "config.h"

#include    <geanyplugin.h>

/* headers */
#include    <stdlib.h>
#include    <glib.h>
#include    <glib/gstdio.h>

/* user header */
#include    "gms.h"
#include    "gms_gui.h"
#include    "gms_debug.h"


GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;


/* Check that the running Geany supports the plugin API used below, and check
 * for binary compatibility. */
PLUGIN_VERSION_CHECK(100)

/* All plugins must set name, description, version and author. */
PLUGIN_SET_TRANSLATABLE_INFO(
    LOCALEDIR, GETTEXT_PACKAGE,
    _("Mini Script"),
    _("A tool to apply a script filter on a text selection or current document(s)"),
    "0.1" , _("Pascal BURLOT, a Geany user"))

static GtkWidget     *gms_item   = NULL ;
static gms_handle_t gms_hnd     = NULL ;
static gchar        *gms_command = NULL ;


/**
 * \brief the function creates from the current selection to the input file
 */
static void create_selection_2_input_file( ScintillaObject *sci )
{
    gchar  *contents = NULL;
    gint    size_buf = sci_get_selected_text_length(sci);

    contents = GMS_G_MALLOC( gchar, size_buf + 1  ) ;
    GMS_PNULL(contents) ;

    sci_get_selected_text( sci ,contents  );

    g_file_set_contents(gms_get_in_filename(gms_hnd), contents, -1 , NULL );
    GMS_G_FREE(contents);
}

/**
 * \brief the function reads the result file
 */
static gchar *read_result_file( gchar *filename )
{
    gchar *utf8     = NULL;
    gchar *contents = NULL;
    GError *error   = NULL ;
    if (g_file_get_contents(filename, &contents, NULL, &error ))
    {
        if ( contents )
        {
            utf8 = g_locale_to_utf8 (contents, -1, NULL, NULL, NULL);
            GMS_G_FREE(contents);
        }
    }
    return utf8 ;
}
/**
 * \brief the function select entirely the document
 */
static void select_entirely_doc( ScintillaObject *sci  )
{
    gint            size_buf = sci_get_length(sci);

    sci_set_selection_start( sci , 0 ) ;
    sci_set_selection_end( sci , size_buf ) ;
}

/**
 * \brief the function updates the current document
 */
static void update_doc( ScintillaObject *sci, gchar * contents )
{
    if (contents==NULL) return ;
    sci_replace_sel( sci, contents );
}
/**
 * \brief the function deletes the tempory files
 */
static void delete_tmp_files(void)
{
    if( g_file_test( gms_get_in_filename(gms_hnd),G_FILE_TEST_EXISTS) == TRUE )
        g_unlink( gms_get_in_filename(gms_hnd) ) ;
    if( g_file_test( gms_get_out_filename(gms_hnd),G_FILE_TEST_EXISTS) == TRUE )
        g_unlink( gms_get_out_filename(gms_hnd) ) ;
    if( g_file_test( gms_get_filter_filename(gms_hnd),G_FILE_TEST_EXISTS) == TRUE )
        g_unlink( gms_get_filter_filename(gms_hnd) ) ;
}

/**
 * \brief the function updates the current document
 */
static gint run_filter( ScintillaObject *sci )
{
    int r , ret = 0 ;
    gchar *result = NULL ;

    gms_command = gms_get_str_command(gms_hnd);
    r = system( gms_command ) ;
    if ( r != 0 )
    {
        GtkWidget *dlg ;
        result = read_result_file( gms_get_error_filename(gms_hnd) ) ;

        dlg = gtk_message_dialog_new( GTK_WINDOW(geany->main_widgets->window),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_CLOSE,
                        "%s", result);

        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(GTK_WIDGET(dlg)) ;
		ret = -1 ;
    }
    else
    {
        result = read_result_file( gms_get_out_filename(gms_hnd) ) ;

        if ( gms_get_output_mode( gms_hnd) == OUT_CURRENT_DOC )
        {
            if ( gms_get_input_mode( gms_hnd) != IN_SELECTION )
                select_entirely_doc(  sci  ) ;

            update_doc( sci, result ) ;
        }
        else
        {
            document_new_file( NULL, NULL, result ) ;
        }
    }
    GMS_G_FREE( result ) ;

	return ret ;
}

/**
 * \brief Callback when the menu item is clicked.
 */
static void item_activate(GtkMenuItem *menuitem, gpointer gdata)
{
    GeanyDocument   *doc = document_get_current();
    ScintillaObject *sci = doc->editor->sci ;
    if ( gms_hnd  == NULL )
        return ;

    if ( gms_dlg( gms_hnd ) == 0 )
        return ;

    gms_create_filter_file( gms_hnd ) ;

    switch ( gms_get_input_mode(gms_hnd) )
    {
        case IN_CURRENT_DOC :
            select_entirely_doc(  sci  ) ;
            create_selection_2_input_file(sci) ;
            run_filter( sci ) ;
            delete_tmp_files() ;
            break;
        case IN_SELECTION :
            create_selection_2_input_file(sci) ;
            run_filter( sci ) ;
            delete_tmp_files() ;
            break;
        case IN_DOCS_SESSION :
            {
                guint nb_doc = 0  , i=0;

				/* find the opened document in the geany session */
                while ( (doc = document_get_from_page(nb_doc))!=NULL ) nb_doc++;

                /* For each document */
                for( i=0; i<nb_doc;i++)
                {
                    doc = document_get_from_page(i) ;
                    sci = doc->editor->sci ;
                    select_entirely_doc(  sci  ) ;
                    create_selection_2_input_file(sci) ;
                    if ( run_filter( sci ) )
						break ; /* if error then stop the loop */
                }
            }
            delete_tmp_files() ;
            break;
        default:
            delete_tmp_files() ;
            return ;
    }

}


/**
 * \brief Called by Geany to initialize the plugin.
 * \note data is the same as geany_data.
 */
void plugin_init(GeanyData *data)
{
    gms_hnd = gms_new(geany->main_widgets->window,
                    data->interface_prefs->editor_font ,
                    data->editor_prefs->indentation->width,
					geany->app->configdir
                    ) ;

    /* Add an item to the Tools menu */
    gms_item = gtk_menu_item_new_with_mnemonic(_("_Mini-Script"));
    gtk_widget_show(gms_item);
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), gms_item);
    g_signal_connect(gms_item, "activate", G_CALLBACK(item_activate), NULL);

    /* make the menu item sensitive only when documents are open */
    ui_add_document_sensitive(gms_item);

}


/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * plugin_init() was called.
 * You can omit this function if the plugin doesn't need to be configured.
 * Note: parent is the parent window which can be used as the transient window for the created
 *  dialog. */
#if 1
GtkWidget *plugin_configure(GtkDialog *dialog)
{
    g_signal_connect(dialog, "response", G_CALLBACK(on_gms_configure_response), gms_hnd );
    return gms_configure_gui( gms_hnd ) ;
}
#endif

/**
 * \brief Called by Geany before unloading the plugin.
 */
void plugin_cleanup(void)
{
    if ( gms_hnd != NULL )
       gms_delete( &gms_hnd ) ;

    /* remove the menu item added in plugin_init() */
    gtk_widget_destroy(gms_item);
}
