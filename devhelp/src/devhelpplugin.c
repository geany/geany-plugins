/*
 * devhelpplugin.c
 * 
 * Copyright 2011 Matthew Brush <mbrush@leftclick.ca>
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
 */

#include <gtk/gtk.h>
#include <geanyplugin.h>

#include <devhelp/dh-base.h>
#include <devhelp/dh-book-tree.h>
#include <devhelp/dh-search.h>
#include <devhelp/dh-link.h>

#ifdef HAVE_BOOK_MANAGER        /* for newer api */
#include <devhelp/dh-book-manager.h>
#endif

#include <webkit/webkitwebview.h>

#include "plugin.h"
#include "devhelpplugin.h"
#include "main-notebook.h"


/* Devhelp base object */
static DhBase *dhbase = NULL;

struct _DevhelpPluginPrivate
{
    /* add your private declarations here */
    gint tmp;
};

static void devhelp_plugin_finalize(GObject * object);

G_DEFINE_TYPE(DevhelpPlugin, devhelp_plugin, G_TYPE_OBJECT)

     static void devhelp_plugin_class_init(DevhelpPluginClass * klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = devhelp_plugin_finalize;
    g_type_class_add_private((gpointer) klass, sizeof(DevhelpPluginPrivate));
}


static void devhelp_plugin_finalize(GObject * object)
{
    DevhelpPlugin *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(DEVHELP_IS_PLUGIN(object));

    self = DEVHELP_PLUGIN(object);

    gtk_widget_destroy(self->sb_notebook);

    gtk_notebook_remove_page(GTK_NOTEBOOK(self->main_notebook),
                             self->webview_tab);

    if (!self->in_message_window)
        main_notebook_destroy();

    gtk_widget_destroy(self->editor_menu_sep);
    gtk_widget_destroy(self->editor_menu_item);

    gtk_notebook_set_tab_pos(GTK_NOTEBOOK
                             (geany->main_widgets->sidebar_notebook),
                             self->orig_sb_tab_pos);

    G_OBJECT_CLASS(devhelp_plugin_parent_class)->finalize(object);
}


static void devhelp_plugin_init(DevhelpPlugin * self)
{
    self->priv =
        G_TYPE_INSTANCE_GET_PRIVATE(self, DEVHELP_TYPE_PLUGIN,
                                    DevhelpPluginPrivate);

}

/* Called when the editor menu item is selected */
static void on_search_help_activate(GtkMenuItem * menuitem,
                                    gpointer user_data)
{
    DevhelpPlugin *dhplug = user_data;
    gchar *current_tag = devhelp_plugin_get_current_tag();

    if (current_tag == NULL)
        return;

    dh_search_set_search_string(DH_SEARCH(dhplug->search), current_tag, NULL);

    /* activate devhelp tabs with search tab active */
    devhelp_plugin_activate_tabs(dhplug, FALSE);

    g_free(current_tag);
}

/* 
 * Called when the editor context menu is shown so that the devhelp
 * search item can be disabled if there isn't a selected tag.
 */
static void on_editor_menu_popup(GtkWidget * widget, gpointer user_data)
{
    gchar *label_tag = NULL;
    gchar *curword = NULL;
    gchar *new_label = NULL;
    DevhelpPlugin *dhplug = user_data;

    curword = devhelp_plugin_get_current_tag();

    if (curword == NULL)
        gtk_widget_set_sensitive(dhplug->editor_menu_item, FALSE);
    else {
        if (strlen(curword) > DHPLUG_MAX_LABEL_TAG) {
            label_tag = g_strndup(curword, DHPLUG_MAX_LABEL_TAG - 3);
            new_label =
                g_strdup_printf(_("Search Devhelp for: %s..."),
                                g_strstrip(label_tag));
        }
        else {
            label_tag = g_strndup(curword, DHPLUG_MAX_LABEL_TAG);
            new_label =
                g_strdup_printf(_("Search Devhelp for %s"),
                                g_strstrip(label_tag));
        }
        gtk_menu_item_set_label(GTK_MENU_ITEM(dhplug->editor_menu_item),
                                new_label);
        g_free(new_label);
        g_free(label_tag);
        gtk_widget_set_sensitive(dhplug->editor_menu_item, TRUE);
    }

    g_free(curword);
}

/**
 * on_link_clicked:
 * @param ignored		Not used
 * @param link	  		The devhelp link object describing what was clicked.
 * @param user_data 	The current DevhelpPlugin struct.
 * 
 * Called when a link in either the contents or search areas on the sidebar 
 * have a link clicked on, meaning to load that file into the webview.
 */
static void on_link_clicked(GObject * ignored, DhLink * link,
                            gpointer user_data)
{
    DevhelpPlugin *plug = user_data;
    if (plug->last_uri)
        g_free(plug->last_uri);
    plug->last_uri = dh_link_get_uri(link);
    webkit_web_view_open(WEBKIT_WEB_VIEW(plug->webview), plug->last_uri);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(plug->main_notebook),
                                  plug->webview_tab);
}

static void on_back_button_clicked(GtkToolButton * btn, gpointer user_data)
{
    DevhelpPlugin *plug = user_data;
    webkit_web_view_go_back(WEBKIT_WEB_VIEW(plug->webview));
}

static void on_forward_button_clicked(GtkToolButton * btn, gpointer user_data)
{
    DevhelpPlugin *plug = user_data;
    webkit_web_view_go_forward(WEBKIT_WEB_VIEW(plug->webview));
}

static void on_zoom_in_button_clicked(GtkToolButton * btn, gpointer user_data)
{
    DevhelpPlugin *plug = user_data;
    WebKitWebView *view = WEBKIT_WEB_VIEW(plug->webview);
    webkit_web_view_zoom_in(view);
    plug->zoom_level = webkit_web_view_get_zoom_level(view);
}

static void on_zoom_out_button_clicked(GtkToolButton * btn,
                                       gpointer user_data)
{
    DevhelpPlugin *plug = user_data;
    WebKitWebView *view = WEBKIT_WEB_VIEW(plug->webview);
    webkit_web_view_zoom_out(view);
    plug->zoom_level = webkit_web_view_get_zoom_level(view);
}

static void on_document_load_finished(WebKitWebView * view,
                                      WebKitWebFrame * frame,
                                      gpointer user_data)
{
    DevhelpPlugin *plug = user_data;

    if (webkit_web_view_can_go_back(view))
        gtk_widget_set_sensitive(GTK_WIDGET(plug->btn_back), TRUE);
    else
        gtk_widget_set_sensitive(GTK_WIDGET(plug->btn_back), FALSE);

    if (webkit_web_view_can_go_forward(view))
        gtk_widget_set_sensitive(GTK_WIDGET(plug->btn_forward), TRUE);
    else
        gtk_widget_set_sensitive(GTK_WIDGET(plug->btn_forward), FALSE);
}


/**
 * devhelp_plugin_new:
 * 
 * Creates a new DevhelpPlugin.  The returned structure is allocated dyamically
 * and must be freed with the devhelp_plugin_destroy() function.  This function
 * gets called from Geany's plugin_init() function.
 * 
 * @return A newly allocated DevhelpPlugin struct or null on error.
 */
DevhelpPlugin *devhelp_plugin_new(gboolean sb_tabs_bottom,
                                  gboolean show_in_msgwin, gchar * last_uri)
{
    GtkWidget *book_tree_sw;
    GtkWidget *webview_sw;
    GtkWidget *contents_label;
    GtkWidget *search_label;
    GtkWidget *dh_sidebar_label;
    GtkWidget *doc_label;
    GtkWidget *vbox;
    GtkWidget *toolbar;
    GtkToolItem *btn_zoom_in;
    GtkToolItem *btn_zoom_out;
    GtkToolItem *tb_sep;

    DevhelpPlugin *dhplug;

    dhplug = g_object_new(DEVHELP_TYPE_PLUGIN, NULL);

    if (dhplug == NULL) {
        g_printerr(_("Cannot create a new Devhelp plugin, out of memory.\n"));
        return NULL;
    }

#ifdef HAVE_BOOK_MANAGER        /* for newer api */
    DhBookManager *book_manager;
#else
    GNode *books;
    GList *keywords;
#endif

    if (dhbase == NULL)
        dhbase = dh_base_new();

#ifdef HAVE_BOOK_MANAGER        /* for newer api */
    book_manager = dh_base_get_book_manager(dhbase);
    dhplug->book_tree = dh_book_tree_new(book_manager);
    dhplug->search = dh_search_new(book_manager);
#else
    books = dh_base_get_book_tree(dhbase);
    keywords = dh_base_get_keywords(dhbase);
    dhplug->book_tree = dh_book_tree_new(books);
    dhplug->search = dh_search_new(keywords);
#endif

    dhplug->in_message_window = show_in_msgwin;

    /* create/grab notebooks */
    dhplug->sb_notebook = gtk_notebook_new();
    dhplug->doc_notebook = geany->main_widgets->notebook;

    if (dhplug->in_message_window)
        dhplug->main_notebook = geany->main_widgets->message_window_notebook;
    else
        dhplug->main_notebook = main_notebook_get();

    /* editor menu items */
    dhplug->editor_menu_sep = gtk_separator_menu_item_new();
    dhplug->editor_menu_item =
        gtk_menu_item_new_with_label(_("Search Documentation for Tag"));

    /* tab labels */
    contents_label = gtk_label_new(_("Contents"));
    search_label = gtk_label_new(_("Search"));
    dh_sidebar_label = gtk_label_new(_("Devhelp"));
    doc_label = gtk_label_new(_("Documentation"));

    dhplug->orig_sb_tab_pos =
        gtk_notebook_get_tab_pos(GTK_NOTEBOOK
                                 (geany->main_widgets->sidebar_notebook));
    devhelp_plugin_sidebar_tabs_bottom(dhplug, sb_tabs_bottom);

    /* sidebar contents/book tree */
    book_tree_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(book_tree_sw),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(book_tree_sw), 6);
    gtk_container_add(GTK_CONTAINER(book_tree_sw), dhplug->book_tree);
    gtk_widget_show(dhplug->book_tree);

    /* sidebar search */
    gtk_widget_show(dhplug->search);

    /* webview to display documentation */
    dhplug->webview = webkit_web_view_new();
    webview_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(webview_sw),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    /*gtk_container_set_border_width(GTK_CONTAINER(webview_sw), 6); */
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(webview_sw),
                                        GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(webview_sw), dhplug->webview);
    gtk_widget_show_all(webview_sw);

    /* setup the sidebar notebook */
    gtk_notebook_append_page(GTK_NOTEBOOK(dhplug->sb_notebook), book_tree_sw,
                             contents_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(dhplug->sb_notebook),
                             dhplug->search, search_label);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->sb_notebook), 0);

    gtk_widget_show_all(dhplug->sb_notebook);
    gtk_notebook_append_page(GTK_NOTEBOOK
                             (geany->main_widgets->sidebar_notebook),
                             dhplug->sb_notebook, dh_sidebar_label);
    dhplug->sb_notebook_tab =
        gtk_notebook_page_num(GTK_NOTEBOOK
                              (geany->main_widgets->sidebar_notebook),
                              dhplug->sb_notebook);

    /* put the webview and toolbar stuff into the main notebook */
    vbox = gtk_vbox_new(FALSE, 0);
    toolbar = gtk_toolbar_new();
    dhplug->btn_back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
    dhplug->btn_forward =
        gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
    btn_zoom_in = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
    btn_zoom_out = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
    tb_sep = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), dhplug->btn_back, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), dhplug->btn_forward, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_sep, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoom_in, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoom_out, -1);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), webview_sw, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(dhplug->main_notebook), vbox,
                             doc_label);
    dhplug->webview_tab =
        gtk_notebook_page_num(GTK_NOTEBOOK(dhplug->main_notebook), vbox);
    gtk_widget_show_all(vbox);

    /* add menu item to editor popup menu */
    /* todo: make this an image menu item with devhelp icon */
    gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu),
                          dhplug->editor_menu_sep);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu),
                          dhplug->editor_menu_item);
    gtk_widget_show(dhplug->editor_menu_sep);
    gtk_widget_show(dhplug->editor_menu_item);

    /* connect signals */
    g_signal_connect(geany->main_widgets->editor_menu, "show",
                     G_CALLBACK(on_editor_menu_popup), dhplug);
    g_signal_connect(dhplug->editor_menu_item, "activate",
                     G_CALLBACK(on_search_help_activate), dhplug);
    g_signal_connect(dhplug->book_tree, "link-selected",
                     G_CALLBACK(on_link_clicked), dhplug);
    g_signal_connect(dhplug->search, "link-selected",
                     G_CALLBACK(on_link_clicked), dhplug);
    g_signal_connect(dhplug->btn_back, "clicked",
                     G_CALLBACK(on_back_button_clicked), dhplug);
    g_signal_connect(dhplug->btn_forward, "clicked",
                     G_CALLBACK(on_forward_button_clicked), dhplug);
    g_signal_connect(btn_zoom_in, "clicked",
                     G_CALLBACK(on_zoom_in_button_clicked), dhplug);
    g_signal_connect(btn_zoom_out, "clicked",
                     G_CALLBACK(on_zoom_out_button_clicked), dhplug);

    /* TODO: find the right signal, this doesn't work on inter-document
     *       links since the page doesn't reload. */
    g_signal_connect(WEBKIT_WEB_VIEW(dhplug->webview),
                     "document-load-finished",
                     G_CALLBACK(on_document_load_finished), dhplug);

    /* toggle state tracking */
    dhplug->last_main_tab_id =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(dhplug->main_notebook));
    dhplug->last_sb_tab_id =
        gtk_notebook_get_current_page(GTK_NOTEBOOK
                                      (geany->main_widgets->
                                       sidebar_notebook));
    dhplug->tabs_toggled = FALSE;

    dhplug->last_uri = last_uri;
    if (dhplug->last_uri)
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(dhplug->webview),
                                 dhplug->last_uri);
    else {
        dhplug->last_uri =
            g_filename_to_uri(DHPLUG_WEBVIEW_HOME_FILE, NULL, NULL);
        if (dhplug->last_uri)
            webkit_web_view_load_uri(WEBKIT_WEB_VIEW(dhplug->webview),
                                     dhplug->last_uri);
    }

    return dhplug;
}

/** 
 * devhelp_plugin_clean_word:
 * @param	str	String to clean
 * 
 * Replaces non GEANY_WORDCHARS in str with spaces and then trims whitespace.
 * This function does not allocate a new string, it modifies str in place
 * and returns a pointer to str. 
 * TODO: make this only remove stuff from the start or end of string.
 * 
 * @return Pointer to (cleaned) @str.
 */
gchar *devhelp_plugin_clean_word(gchar * str)
{
    return g_strstrip(g_strcanon(str, GEANY_WORDCHARS, ' '));
}

/**
 * devhelp_plugin_get_current_tag:
 * 
 * Gets either the current selection or the word at the current selection.
 * 
 * @return Newly allocated string with current tag or NULL no tag.
 */
gchar *devhelp_plugin_get_current_tag(void)
{
    gint pos;
    gchar *tag = NULL;
    GeanyDocument *doc = document_get_current();

    if (sci_has_selection(doc->editor->sci))
        return
            devhelp_plugin_clean_word(sci_get_selection_contents
                                      (doc->editor->sci));

    pos = sci_get_current_position(doc->editor->sci);
    tag = editor_get_word_at_pos(doc->editor, pos, GEANY_WORDCHARS);

    if (tag == NULL)
        return NULL;

    if (tag[0] == '\0') {
        g_free(tag);
        return NULL;
    }

    return devhelp_plugin_clean_word(tag);
}


/**
 * devhelp_plugin_activate_tabs:
 * @param	dhplug		The current DevhelpPlugin struct.
 * @param	contents	If TRUE then select the devhelp Contents tab
 *						otherwise select the devhelp Search tab.
 * 
 * Toggles devhelp related tabs to be current tabs or back to the
 * previously selected tabs.
 */
void devhelp_plugin_activate_tabs(DevhelpPlugin * dhplug, gboolean contents)
{
    if (!dhplug->tabs_toggled) {
        /* toggle state tracking */
        dhplug->last_main_tab_id =
            gtk_notebook_get_current_page(GTK_NOTEBOOK
                                          (dhplug->main_notebook));
        dhplug->last_sb_tab_id =
            gtk_notebook_get_current_page(GTK_NOTEBOOK
                                          (geany->main_widgets->
                                           sidebar_notebook));
        dhplug->tabs_toggled = TRUE;

        gtk_notebook_set_current_page(GTK_NOTEBOOK
                                      (geany->main_widgets->sidebar_notebook),
                                      dhplug->sb_notebook_tab);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->main_notebook),
                                      dhplug->webview_tab);
        if (contents)
            gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->sb_notebook),
                                          0);
        else
            gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->sb_notebook),
                                          1);
    }
    else {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->main_notebook),
                                      dhplug->last_main_tab_id);
        gtk_notebook_set_current_page(GTK_NOTEBOOK
                                      (geany->main_widgets->sidebar_notebook),
                                      dhplug->last_sb_tab_id);
        dhplug->tabs_toggled = FALSE;
    }
}

/**
 * devhelp_plugin_sidebar_tabs_bottom:
 * @param dhplug	The current DevhelpPlugin struct.
 * @param bottom	Whether to move the sidebar tabs to the bottom or not.
 * 
 * Changes the sidebar tab position from its default position to the bottom or
 * vice versa.
 */
void devhelp_plugin_sidebar_tabs_bottom(DevhelpPlugin * dhplug,
                                        gboolean bottom)
{
    if (bottom)
        gtk_notebook_set_tab_pos(GTK_NOTEBOOK
                                 (geany->main_widgets->sidebar_notebook),
                                 GTK_POS_BOTTOM);
    else
        gtk_notebook_set_tab_pos(GTK_NOTEBOOK
                                 (geany->main_widgets->sidebar_notebook),
                                 dhplug->orig_sb_tab_pos);
}
