/*
 * gms_gui.c: miniscript plugin for geany editor
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
 * \file  gms_gui.c
 * \brief it is the graphical user interface of the geany miniscript plugin
 */
#include    "geany.h"

#include    <glib/gstdio.h>
#include    <glib/gprintf.h>

#include    <stdlib.h>
#include    <string.h>
#include    <sys/stat.h>
#include    <sys/types.h>
#include    <unistd.h>

#include    <pango/pango.h>

#ifdef HAVE_LOCALE_H
#include    <locale.h>
#endif

/* geany headers */
#include    "support.h"
#include    "plugindata.h"
#include    "editor.h"
#include    "document.h"
#include    "prefs.h"
#include    "utils.h"
#include    "ui_utils.h"
#include    "pluginmacros.h"

/* user header */
#include    "gms_debug.h"
#include    "gms.h"
#include    "gms_gui.h"
  
/*
 * *****************************************************************************
 *  Local Macro and new local type definition
 */

//! \brief Number of script type
#define GMS_NB_TYPE_SCRIPT  6
//! \brief Number of char of the line buffer
#define GMS_MAX_LINE        127

//! \brief macro uset to cast a gms_handle_t  to a gms_private_t pointer
#define GMS_PRIVATE(p) ((gms_private_t *) p)

//! \brief definition of gui data structure
typedef struct {
    GtkWidget   *dlg          ; //!< Dialog widget
    GtkWidget   *cb_st        ; //!< Script type combobox
    GtkWidget   *t_script     ; //!< script text
    GtkWidget   *rb_select    ; //!< radio button : filtering the selection
    GtkWidget   *rb_doc       ; //!< radio button : filtering the current document
    GtkWidget   *rb_session   ; //!< radio button : filtering all documents of the current session
    GtkWidget   *rb_cdoc      ; //!< radio button : the filter output is in the current document
    GtkWidget   *rb_ndoc      ; //!< radio button : the filter output is in the current document

    GtkWidget   *e_script[GMS_NB_TYPE_SCRIPT] ; //!< entry for script configuration
    GtkTooltips *tips         ; //!< tips of button of the top bar
    PangoFontDescription *fontdesc;
} gms_gui_t  ;

//! \brief definition of mini-script data structure
typedef struct {
    int         id ;                             //!< ID of the instance
	gchar      *config_dir  ;                    //!< path of configuration files
    GString    *cmd         ;                    //!< Command string of filtering
    GtkWidget  *mw          ;                    //!< MainWindow of Geany
    gms_gui_t   w           ;                    //!< Widgets of minis-script gui
    GString    *input_name  ;                    //!< filename of the filter input
    GString    *filter_name ;                    //!< filter filename
    GString    *output_name ;                    //!< filename of the filter output
    GString    *error_name  ;                    //!< errors filename
    GString    *script_cmd[GMS_NB_TYPE_SCRIPT];  //!< array of script command names
} gms_private_t  ;
/*
 * *****************************************************************************
 *  Global variables
 */

/*
 * *****************************************************************************
 *  Local variables
 */
static unsigned char inst_cnt = 0  ; //!< counter of instance
static gchar bufline[GMS_MAX_LINE+1]; //!< buffer used to read the configuration file

static const gchar pref_filename[]   = "gms.rc"    ; //!< preferences filename
static const gchar prefix_filename[] = "/tmp/gms"  ; //!< prefix filename
static const gchar in_ext[]          = ".in"       ; //!< filename extension for the input file
static const gchar out_ext[]         = ".out"      ; //!< filename extension for the output file
static const gchar filter_ext[]      = ".filter"   ; //!< filename extension for the filter file
static const gchar error_ext[]       = ".error"    ; //!< filename extension for the error file

///< \brief It's the default script command
static const gchar *default_script_cmd[GMS_NB_TYPE_SCRIPT] = {
    "${SHELL} ", "perl ", "python ", "sed -f ", "awk -f ", "cat - "  };

///< \brief It's the label for the script command combobox
const gchar *label_script_cmd[GMS_NB_TYPE_SCRIPT] = {
    "Shell", "Perl", "Python", "Sed", "Awk", "User" };

///< \brief It's the information message about geany mini script
const char *geany_info = "<b>GMS : Geany Mini-Script filter Plugin</b>\n"
"This plugin is a tool to apply a script filter on :\n"
"   o the text selection,\n"
"   o the current document,\n"
"   o all documents of the current session.\n"
"\n"
"The filter type can be : \n"
"   o Unix shell script, \n"
"   o perl script, \n"
"   o python script, \n"
"   o sed commands,\n"
"   o awk script.\n"
"\n"
"<b>AUTHOR</b>\n"
"   Written by Pascal BURLOT (December,2008)\n"
"\n"
"<b>LICENSE:</b>\n"
"This program is free software; you can redistribute\n"
"it and/or modify it under the terms of the GNU \n"
"General Public License as published by the Free\n"
"Software Foundation; either version 2 of the License,\n"
"or (at your option) any later version." ;

/*
 * *****************************************************************************
 *  Local functions
 */

/**
 * \brief the function loads the preferences file
 */
static void load_prefs_file(
    gms_private_t *this  ///< pointer of mini-script data structure
    )
{
    GString *gms_pref = g_string_new("") ;

    g_string_printf(gms_pref , "%s/plugins/%s", this->config_dir,pref_filename );

    if ( g_file_test( gms_pref->str, G_FILE_TEST_EXISTS ) == TRUE )
    {
        FILE *fd = g_fopen( gms_pref->str, "r" ) ;
        if ( fd != NULL )
        {
            int  ii ;
            for ( ii = 0 ; ii <GMS_NB_TYPE_SCRIPT ;ii++ )
            {
                if ( fgets(bufline,GMS_MAX_LINE,fd) == NULL )
                    break ;
                if ( fgets(bufline,GMS_MAX_LINE,fd) == NULL )
                    break ;
                bufline[strlen(bufline)-1] = 0 ;
                g_string_assign(this->script_cmd[ii] , bufline ) ;
            }
            fclose(fd) ;
        }
     }
    g_string_free( gms_pref , TRUE) ;
}

/**
 * \brief the function saves the preferences file
 */
static void save_prefs_file(
        gms_private_t *this ///< pointer of mini-script data structure
    )
{
    GString *gms_pref = g_string_new(getenv("HOME"));

    g_string_printf(gms_pref , "%s/plugins", this->config_dir,pref_filename );

    if ( g_file_test( this->config_dir, G_FILE_TEST_EXISTS ) != TRUE )
        g_mkdir( this->config_dir, 0755 ) ;
		
    if ( g_file_test( gms_pref->str, G_FILE_TEST_EXISTS ) != TRUE )
        g_mkdir( gms_pref->str, 0755 ) ;

    if ( g_file_test( gms_pref->str, G_FILE_TEST_IS_DIR ) == TRUE )
    {
        FILE *fd ;
        g_string_append_c( gms_pref, '/' );
        g_string_append( gms_pref, pref_filename );

        fd = g_fopen( gms_pref->str, "w" ) ;
        if ( fd != NULL )
        {
            int  ii ;
            for ( ii = 0 ; ii <GMS_NB_TYPE_SCRIPT ;ii++ )
                fprintf(fd,"# %s\n%s\n",label_script_cmd[ii],this->script_cmd[ii]->str);

            fclose(fd) ;
        }
    }
    g_string_free( gms_pref , TRUE) ;
}

/**
 * \brief Callback associated to a button "new"
 */
static void gms_cb_new(
    GtkWidget *w  ,
    gpointer data
    )
{
    gms_private_t *this = GMS_PRIVATE(data) ;
    GtkTextIter start;
    GtkTextIter end;
    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW( this->w.t_script ) );
    gtk_text_buffer_get_start_iter(text_buffer,&start);
    gtk_text_buffer_get_end_iter(text_buffer,&end);
    gtk_text_buffer_delete( text_buffer ,&start , &end) ;
}

/**
 * \brief Callback associated to a button "load"
 */
static void gms_cb_load(
    GtkWidget *w  ,
    gpointer  data
    )
{
    gms_private_t *this = GMS_PRIVATE(data) ;
    GtkWidget    *p_dialog ;
    
    p_dialog = gtk_file_chooser_dialog_new (_("Load Mini-Script File"),
                                    GTK_WINDOW(this->mw) ,
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                    NULL);
    if ( p_dialog == NULL )
        return ;

    if (gtk_dialog_run (GTK_DIALOG (p_dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar         *filename = NULL;

        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (p_dialog));

        if(filename != NULL)
        {
            gchar *contents = NULL;
            GError *error = NULL ;

            if (g_file_get_contents(filename, &contents, NULL, &error ))
            {
                gchar         *utf8 = NULL;
                GtkTextIter   start;
                GtkTextIter   end;
                GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW( this->w.t_script ) );

                /* destroy the old text of buffer */
                gtk_text_buffer_get_start_iter(text_buffer,&start);
                gtk_text_buffer_get_end_iter(text_buffer,&end);
                gtk_text_buffer_delete( text_buffer ,&start , &end) ;

                /* Copy file contents in GtkTextView */
                gtk_text_buffer_get_start_iter(text_buffer,&start);
                utf8 = g_locale_to_utf8 (contents, -1, NULL, NULL, NULL);
                GMS_G_FREE(contents);
                gtk_text_buffer_insert (text_buffer, &start, utf8, -1);
                GMS_G_FREE(utf8);
            }
            GMS_G_FREE(filename);
        }
    }
    gtk_widget_destroy (p_dialog);
}


/**
 * \brief Callback associated to a button "load"
 */
static void gms_cb_save(
    GtkWidget *w  ,
    gpointer  data
    )
{
    gms_private_t *this = GMS_PRIVATE(data) ;
    GtkWidget    *p_dialog ;
    
    p_dialog = gtk_file_chooser_dialog_new (_("Save Mini-Script File"),
                                    GTK_WINDOW(this->mw) ,
                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                    GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                    NULL);
    if ( p_dialog == NULL )
        return ;

    if (gtk_dialog_run (GTK_DIALOG (p_dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar         *filename = NULL;

        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (p_dialog));

        if(filename != NULL)
        {
            gchar        *contents = NULL;
            GtkTextIter   start;
            GtkTextIter   end;
            GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW( this->w.t_script ) );
            gtk_text_buffer_get_start_iter(text_buffer,&start);
            gtk_text_buffer_get_end_iter(text_buffer,&end);
            contents=gtk_text_buffer_get_text(text_buffer,&start,&end,FALSE);
            g_file_set_contents(filename, contents, -1 , NULL );
            GMS_G_FREE(contents);
            GMS_G_FREE(filename);
       }
    }
    gtk_widget_destroy (p_dialog);
}
/**
 * \brief Callback associated to a button "load"
 */
static void gms_cb_info(
    GtkWidget *w  ,
    gpointer  data
    )
{
    gms_private_t *this = GMS_PRIVATE(data) ;

    GtkWidget *dlg = gtk_message_dialog_new_with_markup( GTK_WINDOW(this->mw),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_INFO,
                                GTK_BUTTONS_CLOSE,
                                _(geany_info),NULL );
                                
    gtk_dialog_run(GTK_DIALOG(dlg));
    GMS_FREE_WIDGET(dlg);
}

/*
 * *****************************************************************************
 *  Global functions
 */

static GtkWidget  *new_button_from_stock( gboolean withtext, const gchar *stock_id )
{
    GtkWidget  *button ;
    if ( withtext )
        button   = gtk_button_new_from_stock( stock_id  ) ;
    else
    {
        button   = gtk_button_new() ;
        gtk_container_add( GTK_CONTAINER(button),gtk_image_new_from_stock(stock_id , GTK_ICON_SIZE_SMALL_TOOLBAR ) );
    }
    return button ;
}

/**
 * \brief the function initializes the mini-script gui structure.
 */
gms_handle_t gms_new(
    GtkWidget *mw         , ///< Geany Main windows
    gchar     *font       , ///< Geany editor font
    gint       tabs       , ///< Geany editor tabstop
    gchar     *config_dir   ///< Geany Configuration Path
    )
{
    gms_private_t *this = GMS_G_MALLOC0(gms_private_t,1);

    if ( this != NULL )
    {
        GtkBox     *vb_dlg        ; //!< vbox of dialog box
        GtkWidget  *hb_st         ; //!< Hbox for script type
        GtkWidget  *b_open        ; //!< button for loading a existing script
        GtkWidget  *b_save        ; //!< button for saving the current script
        GtkWidget  *b_new         ; //!< button for erasing the script text box
        GtkWidget  *b_info        ; //!< button for info box
        GtkWidget  *sb_script     ; //!< Scroll box for script text
        GtkWidget  *hb_rb         ; //!< Hbox for radio buttons
        GtkWidget  *f_rbi         ; //!< frame for radio buttons : input filter
        GtkWidget  *hb_rbi        ; //!< Hbox for radio buttons
        GtkWidget  *f_rbo         ; //!< frame for radio buttons : input filter
        GtkWidget  *hb_rbo        ; //!< Hbox for radio buttons

        PangoTabArray* tabsarray  ;
        GdkScreen  *ecran  = gdk_screen_get_default();
        gint        width  = gdk_screen_get_width(ecran) ;
        gint        height = gdk_screen_get_height(ecran) ;
        gint        i , size_pid ;
        gboolean    mode_txt_icon = FALSE ;

        this->mw = mw ;
        this->cmd = g_string_new("");
        this->config_dir =config_dir ;

        this->w.dlg = gtk_dialog_new_with_buttons(
                        _("Mini-Script Filter"),
                        GTK_WINDOW( mw ),
                        GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_MODAL,
                        GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
                        GTK_STOCK_EXECUTE,GTK_RESPONSE_APPLY,
                        NULL
                         ) ;
        vb_dlg   = GTK_BOX (GTK_DIALOG(this->w.dlg)->vbox)  ;

        if ( width > 800 )
            width = 800 ;

        if ( height > 600 )
            height = 600 ;

        gtk_window_set_default_size( GTK_WINDOW(this->w.dlg) , width/2 , height/2 ) ;

 
        this->w.tips = gtk_tooltips_new ();
        
     // Hbox : type de script
        hb_st = gtk_hbox_new (FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (hb_st), 0);
        gtk_box_pack_start( vb_dlg , hb_st, FALSE, FALSE, 0);

        b_new   = new_button_from_stock( mode_txt_icon, GTK_STOCK_CLEAR  ) ;
        gtk_box_pack_start( GTK_BOX (hb_st), b_new, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (b_new), "clicked",G_CALLBACK (gms_cb_new), (gpointer) this );
        gtk_tooltips_set_tip (GTK_TOOLTIPS (this->w.tips), b_new, _("Clear the mini-script window") , "");

        b_open   = new_button_from_stock( mode_txt_icon, GTK_STOCK_OPEN  ) ;
        gtk_box_pack_start( GTK_BOX (hb_st), b_open, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (b_open), "clicked",G_CALLBACK (gms_cb_load), (gpointer) this );
        gtk_tooltips_set_tip (GTK_TOOLTIPS (this->w.tips), b_open, _("Load a mini-script into this window"), "");

        b_save   = new_button_from_stock( mode_txt_icon, GTK_STOCK_SAVE_AS  ) ;
        gtk_box_pack_start( GTK_BOX (hb_st),b_save, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (b_save), "clicked",G_CALLBACK (gms_cb_save), (gpointer) this );
        gtk_tooltips_set_tip (GTK_TOOLTIPS (this->w.tips), b_save, _("Save the mini-script into a file"), "");

        b_info   = new_button_from_stock( mode_txt_icon, GTK_STOCK_INFO  ) ;
        gtk_box_pack_end( GTK_BOX (hb_st), b_info, FALSE, FALSE, 0);
        g_signal_connect (G_OBJECT (b_info), "clicked",G_CALLBACK (gms_cb_info), (gpointer) this );
        gtk_tooltips_set_tip (GTK_TOOLTIPS (this->w.tips), b_info, _("Display a information about the mini-script plugin"), "");

        this->w.cb_st = gtk_combo_box_new_text() ;
        for ( i=0;i<GMS_NB_TYPE_SCRIPT ; i++ )
           gtk_combo_box_append_text( GTK_COMBO_BOX(this->w.cb_st), label_script_cmd[i] ) ;
        gtk_combo_box_set_active(GTK_COMBO_BOX(this->w.cb_st), 0 );
        gtk_box_pack_start(GTK_BOX(hb_st), this->w.cb_st, FALSE, FALSE, 0);
        GTK_WIDGET_SET_FLAGS (this->w.cb_st, GTK_CAN_DEFAULT);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (this->w.tips), this->w.cb_st, _("select the mini-script type"), "");

    // Scroll Box : script
        sb_script =  gtk_scrolled_window_new (NULL,NULL);
        gtk_container_set_border_width (GTK_CONTAINER (sb_script), 0);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sb_script),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
        gtk_box_pack_start(vb_dlg, sb_script, TRUE, TRUE, 0);

        this->w.t_script =  gtk_text_view_new();
        this->w.fontdesc = pango_font_description_from_string(font);
        gtk_widget_modify_font( this->w.t_script, this->w.fontdesc );
        gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW (sb_script), this->w.t_script);
        { // find the width of the space character
            gint largeur,hauteur ;
            PangoLayout *layout = gtk_widget_create_pango_layout (this->w.t_script, " ");
            pango_layout_set_font_description (layout, this->w.fontdesc);
            pango_layout_get_pixel_size(layout, &largeur, &hauteur);
            tabs = tabs*largeur ;
            g_object_unref (layout);
        }
        tabsarray = pango_tab_array_new_with_positions ( 1,  TRUE  , PANGO_TAB_LEFT, tabs ) ;
        gtk_text_view_set_tabs(GTK_TEXT_VIEW(this->w.t_script),tabsarray);

    // Hbox : Radio bouttons for choosing the input/output:
        hb_rb = gtk_hbox_new (FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (hb_rb), 0);
        gtk_box_pack_start( vb_dlg, hb_rb, FALSE, FALSE, 0);

    // Hbox : Radio bouttons for choosing the input:
    //                   selection/current document/all documents of the current session
        f_rbi = gtk_frame_new (_("filter input") );
        gtk_box_pack_start( GTK_BOX (hb_rb), f_rbi, FALSE, FALSE, 0);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (this->w.tips), f_rbi, _("select the input of mini-script filter"), "");

        hb_rbi = gtk_hbox_new (FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (hb_rbi), 0);
        gtk_container_add (GTK_CONTAINER (f_rbi), hb_rbi );

        this->w.rb_select  = gtk_radio_button_new_with_label( NULL, _("selection") ) ;
        this->w.rb_doc     = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON( this->w.rb_select) ,_("document") ) ;
        this->w.rb_session = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON( this->w.rb_select) ,_("session") ) ;
        gtk_box_pack_start(GTK_BOX(hb_rbi), this->w.rb_select, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hb_rbi), this->w.rb_doc, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hb_rbi), this->w.rb_session, TRUE, TRUE, 0);
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(this->w.rb_doc) ,TRUE) ;


    // Hbox : Radio bouttons for choosing the output:
    //                   current document/ or new document
        f_rbo = gtk_frame_new (_("filter output") );
        gtk_box_pack_start( GTK_BOX(hb_rb), f_rbo, FALSE, FALSE, 0);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (this->w.tips), f_rbo, _("select the output of mini-script filter"), "");

        hb_rbo = gtk_hbox_new (FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(hb_rbo), 0);
        gtk_container_add (GTK_CONTAINER(f_rbo), hb_rbo );

        this->w.rb_cdoc    = gtk_radio_button_new_with_label( NULL, _("Current Doc.") ) ;
        this->w.rb_ndoc    = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON( this->w.rb_cdoc) ,_("New Doc.") ) ;
        gtk_box_pack_start(GTK_BOX(hb_rbo), this->w.rb_cdoc, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hb_rbo), this->w.rb_ndoc, TRUE, TRUE, 0);
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(this->w.rb_ndoc) ,TRUE) ;

        gtk_widget_show_all(GTK_WIDGET(vb_dlg));
        this->id  = ++inst_cnt ;

        this->input_name = g_string_new(prefix_filename) ;
        this->filter_name= g_string_new(prefix_filename) ;
        this->output_name= g_string_new(prefix_filename) ;
        this->error_name = g_string_new(prefix_filename) ;

        size_pid = (gint)(2*sizeof(pid_t)) ;
        g_string_append_printf( this->input_name,"%02x_%0*x%s",
                    this->id,size_pid, getpid(), in_ext ) ;

        g_string_append_printf(this->filter_name,"%02x_%0*x%s",
                    this->id,size_pid, getpid(), filter_ext ) ;

        g_string_append_printf(this->output_name,"%02x_%0*x%s",
                    this->id,size_pid, getpid(), out_ext ) ;

        g_string_append_printf(this->error_name,"%02x_%0*x%s",
                    this->id,size_pid, getpid(), error_ext ) ;

        for ( i=0;i<GMS_NB_TYPE_SCRIPT ; i++ )
        {
            this->script_cmd[i]=g_string_new(default_script_cmd[i] ) ;
            this->w.e_script[i]=NULL;
        }
        load_prefs_file(this) ;

    }

    return GMS_HANDLE(this) ;
}
/**
 * \brief the function destroys the mini-script gui structure.
 */

void gms_delete(
    gms_handle_t *hnd  ///< handle of mini-script data structure
    )
{
    if ( hnd != NULL )
    {
        gms_private_t *this = GMS_PRIVATE( *hnd ) ;
        gint i ;
        gboolean flag = TRUE ;

        GMS_FREE_FONTDESC(this->w.fontdesc );
        GMS_FREE_WIDGET(this->w.dlg);

        g_string_free( this->input_name  ,flag) ;
        g_string_free( this->output_name ,flag) ;
        g_string_free( this->filter_name ,flag) ;
        g_string_free( this->cmd         ,flag) ;

        for ( i=0;i<GMS_NB_TYPE_SCRIPT ; i++ )
            g_string_free(this->script_cmd[i] ,flag) ;
        
        GMS_G_FREE( this )  ;
    }
}

/**
 * \brief the function runs the mini-script dialog.
 */
int gms_dlg(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    gint ret = 0 ;

    if ( this == NULL )
        return 0 ;

    gtk_widget_show(this->w.dlg);
    ret = gtk_dialog_run(GTK_DIALOG(this->w.dlg));
    gtk_widget_hide(this->w.dlg);

    if ( ret == GTK_RESPONSE_APPLY )
        return 1 ;
    else
        return 0 ;
}

/**
 * \brief the function get the input mode.
 */
gms_input_t  gms_get_input_mode(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    gms_input_t mode  = IN_CURRENT_DOC ;

    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(this->w.rb_select) ) == TRUE )
        mode  = IN_SELECTION ;
    else  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(this->w.rb_session) ) == TRUE )
        mode  = IN_DOCS_SESSION ;

    return mode ;
}

/**
 * \brief the function get the output mode.
 */
gms_output_t  gms_get_output_mode(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    gms_output_t mode  = OUT_CURRENT_DOC ;

    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(this->w.rb_ndoc) ) == TRUE )
        mode  = OUT_NEW_DOC ;

    return mode ;
}

/**
 * \brief the function get the input filename for filter input.
 */
gchar *gms_get_in_filename(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    return this->input_name->str ;
}

/**
 * \brief the function get the output filename for filter result.
 */
gchar *gms_get_out_filename(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    return this->output_name->str ;
}

/**
 * \brief the function get the output filename for filter script.
 */
gchar *gms_get_filter_filename( gms_handle_t hnd )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    return this->filter_name->str ;
}

/**
 * \brief the function get the error filename for filter script.
 */
gchar *gms_get_error_filename(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    return this->error_name->str ;
}

/**
 * \brief the function creates the filter file.
 */
void  gms_create_filter_file(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    gchar           *contents = NULL;
    GtkTextIter      start;
    GtkTextIter      end;
    GtkTextBuffer   *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW( this->w.t_script ) );

    gtk_text_buffer_get_start_iter(text_buffer,&start);
    gtk_text_buffer_get_end_iter(text_buffer,&end);
    contents=gtk_text_buffer_get_text(text_buffer,&start,&end,FALSE);
    g_file_set_contents(this->filter_name->str, contents, -1 , NULL );
    GMS_G_FREE(contents);
}

/**
 * \brief the function creates the command string.
 */
gchar *gms_get_str_command(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;
    gint ii_script = gtk_combo_box_get_active(GTK_COMBO_BOX(this->w.cb_st) ) ;

    g_string_printf( this->cmd,"cat %s | %s %s > %s 2> %s",
                                this->input_name->str,
                                    this->script_cmd[ii_script]->str,
                                        this->filter_name->str,
                                            this->output_name->str,
                                                this->error_name->str );
    return this->cmd->str  ;
}

/**
 * \brief the function creates the configuration gui.
 */

GtkWidget   *gms_configure_gui(
    gms_handle_t hnd ///< handle of mini-script data structure
    )
{
    gms_private_t *this = GMS_PRIVATE( hnd ) ;

    volatile gint ii ;
    GtkWidget *vb_pref        ; //!< vbox for mini-script configuration
    GtkWidget  *f_script      ; //!< frame for configuration script
    GtkWidget  *t_script      ; //!< table for configuration script
    GtkWidget  *w ;

    vb_pref= gtk_vbox_new(FALSE, 6);
    f_script = gtk_frame_new (_("script configuration") );
    gtk_box_pack_start( GTK_BOX (vb_pref), f_script, FALSE, FALSE, 0);

    t_script = gtk_table_new( GMS_NB_TYPE_SCRIPT ,3,FALSE) ;
    gtk_container_add (GTK_CONTAINER (f_script), t_script );

    for ( ii = 0 ; ii <GMS_NB_TYPE_SCRIPT ;ii++ )
    {
        w = gtk_label_new(label_script_cmd[ii]);
        gtk_table_attach_defaults(GTK_TABLE(t_script),w, 0,1,ii,ii+1 );

        this->w.e_script[ii] = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(this->w.e_script[ii]), this->script_cmd[ii]->str);
        gtk_table_attach_defaults(GTK_TABLE(t_script),this->w.e_script[ii], 1,2,ii,ii+1 );
    }

    gtk_widget_show_all(vb_pref);
    return vb_pref ;
}

/**
 * \brief Callback associated to a button "new"
 */
void on_gms_configure_response(GtkDialog *dialog, gint response, gpointer user_data   )
{
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
    {
        int  ii ;
        gms_private_t *this = GMS_PRIVATE(user_data) ;

        for ( ii = 0 ; ii <GMS_NB_TYPE_SCRIPT ;ii++ )
            if (this->w.e_script[ii]!=NULL )
                g_string_assign( this->script_cmd[ii] , gtk_entry_get_text(GTK_ENTRY(this->w.e_script[ii])));
        save_prefs_file(this);
    }
}
