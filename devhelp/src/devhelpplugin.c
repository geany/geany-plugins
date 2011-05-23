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

#ifdef HAVE_BOOK_MANAGER /* for newer api */
#include <devhelp/dh-book-manager.h>
#endif

#include <webkit/webkitwebview.h>

#include "main-notebook.h"
#include "devhelpplugin.h"
#include "plugin.h"
#include "manpages.h"
#include "codesearch.h"


struct _DevhelpPluginPrivate
{
	DhBase*			dhbase;
    GtkWidget*		book_tree;			/* "Contents" in the sidebar */
    GtkWidget*		search;				/* "Search" in the sidebar */
    GtkWidget*		sb_notebook;		/* Notebook that holds contents/search */
    gint 			sb_notebook_tab;	/* Index of tab where devhelp sidebar is */
    GtkWidget*		webview;			/* Webkit that shows documentation */
    gint 			webview_tab;		/* Index of tab that contains the webview */
    GtkWidget*		main_notebook;		/* Notebook that holds Geany doc notebook and
										 *   and webkit view */
    GtkWidget*		doc_notebook;    	/* Geany's document notebook   */
    GtkWidget*		editor_menu_item;	/* Item in the editor's context menu */
    GtkWidget*		editor_menu_sep;	/* Separator item above menu item */
    gboolean		webview_active;		/* Tracks whether webview stuff is shown */
    gboolean		last_main_tab_id;	/* These track the last id of the tabs */
    gboolean		last_sb_tab_id;		/*   before toggling */
    gboolean		tabs_toggled;		/* Tracks state of whether to toggle to
										 *   Devhelp or back to code */
    gboolean		created_main_nb;	/* Track whether we created the main notebook */

    GtkPositionType	orig_sb_tab_pos;	/* The tab idx of the initial sidebar tab */
    gboolean		sidebar_tab_bottom;	/* whether the sidebar tabs are at the bottom */
    gboolean		in_message_window;	/* whether the webkit stuff is in the msgwin */

    gchar*			last_uri;			/* the last URI loaded before the current */
    gfloat			zoom_level;			/* the webkit zoom level */

    GtkToolItem*	btn_back;			/* the webkit browser back button in the toolbar */
    GtkToolItem*	btn_forward;		/* the webkit browser forward button in the toolbar */

};


G_DEFINE_TYPE(DevhelpPlugin, devhelp_plugin, G_TYPE_OBJECT)


static DhBase *dhbase = NULL;


/* Internal callbacks */
static void on_search_help_activate(GtkMenuItem * menuitem, DevhelpPlugin *self);
static void on_search_help_man_activate(GtkMenuItem * menuitem, DevhelpPlugin *self);
static void on_search_help_code_activate(GtkMenuItem *menuitem, DevhelpPlugin *self);
static void on_editor_menu_popup(GtkWidget * widget, DevhelpPlugin *self);
static void on_link_clicked(GObject * ignored, DhLink * dhlink, DevhelpPlugin *self);
static void on_back_button_clicked(GtkToolButton * btn, DevhelpPlugin *self);
static void on_forward_button_clicked(GtkToolButton * btn, DevhelpPlugin *self);
static void on_zoom_in_button_clicked(GtkToolButton * btn, DevhelpPlugin *self);
static void on_zoom_out_button_clicked(GtkToolButton *btn, DevhelpPlugin *self);
static void on_document_load_finished(WebKitWebView *view, WebKitWebFrame *frame, DevhelpPlugin *self);
static void on_uri_changed_notify(GObject *object, GParamSpec *pspec, DevhelpPlugin *self);
static void on_load_status_changed_notify(GObject *object, GParamSpec *pspec, DevhelpPlugin *self);

/* Misc internal functions */
static inline void update_history_buttons(DevhelpPlugin *self);


/* Put cleanup code in here */
static void devhelp_plugin_finalize(GObject * object)
{
	DevhelpPlugin *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(DEVHELP_IS_PLUGIN(object));

	self = DEVHELP_PLUGIN(object);

	gtk_widget_destroy(self->priv->sb_notebook);

	gtk_notebook_remove_page(GTK_NOTEBOOK(self->priv->main_notebook), self->priv->webview_tab);

	if (!self->priv->in_message_window)
	{
		g_debug("Destroying main notebook");
		main_notebook_destroy();
	}
	gtk_widget_destroy(self->priv->editor_menu_sep);
	gtk_widget_destroy(self->priv->editor_menu_item);

	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
		self->priv->orig_sb_tab_pos);

	if (self->priv->last_uri != NULL)
		g_free(self->priv->last_uri);

	devhelp_plugin_remove_manpages_temp_files();

	G_OBJECT_CLASS(devhelp_plugin_parent_class)->finalize(object);
}


static void devhelp_plugin_class_init(DevhelpPluginClass * klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = devhelp_plugin_finalize;
	g_type_class_add_private((gpointer) klass, sizeof(DevhelpPluginPrivate));
}


/*
 * Initialize the Devhelp library/widgets.
 * The Devhelp API isn't exactly stable, so handle quirks in here.
 */
static void devhelp_plugin_init_dh(DevhelpPlugin *self)
{
#ifdef HAVE_BOOK_MANAGER /* for newer api */
	DhBookManager *book_manager;
#else
	GNode *books;
	GList *keywords;
#endif

	if (dhbase == NULL)
		dhbase = dh_base_new();
	self->priv->dhbase = dhbase;

#ifdef HAVE_BOOK_MANAGER /* for newer api */
	book_manager = dh_base_get_book_manager(self->priv->dhbase);
	self->priv->book_tree = dh_book_tree_new(book_manager);
	self->priv->search = dh_search_new(book_manager);
#else
	books = dh_base_get_book_tree(self->priv->dhbase);
	keywords = dh_base_get_keywords(self->priv->dhbase);
	self->priv->book_tree = dh_book_tree_new(books);
	self->priv->search = dh_search_new(keywords);
#endif

	gtk_widget_show(self->priv->search);

	g_signal_connect(self->priv->book_tree, "link-selected", G_CALLBACK(on_link_clicked), self);
	g_signal_connect(self->priv->search, "link-selected", G_CALLBACK(on_link_clicked), self);
}


/* Initialize the stuff in the editor's context menu */
static void devhelp_plugin_init_edit_menu(DevhelpPlugin *self)
{
	GtkWidget *doc_menu, *devhelp_item, *man_item, *code_item;
	DevhelpPluginPrivate *p;

	g_return_if_fail(self != NULL);

	p = self->priv;

	p->editor_menu_sep = gtk_separator_menu_item_new();
	p->editor_menu_item = gtk_menu_item_new_with_label(_("Search for 'Tag' Documentation in"));
	doc_menu = gtk_menu_new();

	devhelp_item = gtk_menu_item_new_with_label(_("Devhelp"));
	man_item = gtk_menu_item_new_with_label(_("Manual Pages"));
	code_item = gtk_menu_item_new_with_label(_("Google Code"));

	gtk_menu_shell_append(GTK_MENU_SHELL(doc_menu), devhelp_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(doc_menu), man_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(doc_menu), code_item);

	g_signal_connect(geany->main_widgets->editor_menu, "show", G_CALLBACK(on_editor_menu_popup), self);
	g_signal_connect(devhelp_item, "activate", G_CALLBACK(on_search_help_activate), self);
	g_signal_connect(man_item, "activate", G_CALLBACK(on_search_help_man_activate), self);
	g_signal_connect(code_item, "activate", G_CALLBACK(on_search_help_code_activate), self);

	gtk_widget_show(p->editor_menu_sep);
	gtk_widget_show(devhelp_item);
	gtk_widget_show(man_item);
	gtk_widget_show(code_item);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(p->editor_menu_item), doc_menu);
	gtk_widget_show_all(p->editor_menu_item);

	gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu), p->editor_menu_sep);
	gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu), p->editor_menu_item);

}


/* Initialize the WebKit browser and associated widgets */
static void devhelp_plugin_init_webkit(DevhelpPlugin *self)
{
	GtkWidget *vbox, *label, *toolbar;
	GtkToolItem *btn_zoom_in, *btn_zoom_out, *tb_sep;
	GtkScrolledWindow *webview_sw;
	DevhelpPluginPrivate *p;

	g_return_if_fail(self != NULL);

	p = self->priv;

	p->webview = webkit_web_view_new();

	webview_sw = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
	gtk_scrolled_window_set_policy(webview_sw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(webview_sw, GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(webview_sw), p->webview);

	gtk_widget_show_all(GTK_WIDGET(webview_sw));

	vbox = gtk_vbox_new(FALSE, 0);
	toolbar = gtk_toolbar_new();

	p->btn_back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	p->btn_forward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	btn_zoom_in = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
	btn_zoom_out = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
	tb_sep = gtk_separator_tool_item_new();

	gtk_widget_set_tooltip_text(GTK_WIDGET(p->btn_back), _("Go back one page"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(p->btn_forward), _("Go forward one page"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoom_in), _("Zoom in"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoom_out), _("Zoom out"));

	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), p->btn_back, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), p->btn_forward, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_sep, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoom_in, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoom_out, -1);

	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(webview_sw), TRUE, TRUE, 0);

	label = gtk_label_new(_("Documentation"));
	gtk_notebook_append_page(GTK_NOTEBOOK(p->main_notebook), vbox, label);

	p->webview_tab = gtk_notebook_page_num(GTK_NOTEBOOK(p->main_notebook), vbox);

	gtk_widget_show_all(vbox);

	g_signal_connect(p->btn_back, "clicked", G_CALLBACK(on_back_button_clicked), self);
	g_signal_connect(p->btn_forward, "clicked", G_CALLBACK(on_forward_button_clicked), self);
	g_signal_connect(btn_zoom_in, "clicked", G_CALLBACK(on_zoom_in_button_clicked), self);
	g_signal_connect(btn_zoom_out, "clicked", G_CALLBACK(on_zoom_out_button_clicked), self);

	g_signal_connect(WEBKIT_WEB_VIEW(p->webview), "document-load-finished", G_CALLBACK(on_document_load_finished), self);
	g_signal_connect(WEBKIT_WEB_VIEW(p->webview), "notify::uri", G_CALLBACK(on_uri_changed_notify), self);
	g_signal_connect(WEBKIT_WEB_VIEW(p->webview), "notify::load-status", G_CALLBACK(on_load_status_changed_notify), self);

	p->last_uri = g_filename_to_uri(DHPLUG_WEBVIEW_HOME_FILE, NULL, NULL);
	if (p->last_uri != NULL)
		webkit_web_view_load_uri(WEBKIT_WEB_VIEW(p->webview), p->last_uri);
}


/* Initialize the stuff added to the sidebar. */
static void devhelp_plugin_init_sidebar(DevhelpPlugin *self)
{
	GtkWidget *label;
	GtkWidget *book_tree_sw;
	DevhelpPluginPrivate *p;

	g_return_if_fail(self != NULL);

	p = self->priv;

	p->sb_notebook = gtk_notebook_new();
	p->orig_sb_tab_pos = gtk_notebook_get_tab_pos(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));

	book_tree_sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(book_tree_sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(book_tree_sw), 6);
	gtk_container_add(GTK_CONTAINER(book_tree_sw), p->book_tree);
	gtk_widget_show(p->book_tree);

	label = gtk_label_new(_("Contents"));
	gtk_notebook_append_page(GTK_NOTEBOOK(p->sb_notebook), book_tree_sw, label);

	label = gtk_label_new(_("Search"));
	gtk_notebook_append_page(GTK_NOTEBOOK(p->sb_notebook), p->search, label);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(p->sb_notebook), 0);
	gtk_widget_show_all(p->sb_notebook);

	label = gtk_label_new(_("Devhelp"));
	gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), p->sb_notebook, label);

	p->sb_notebook_tab = gtk_notebook_page_num(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), p->sb_notebook);
}


/* Called when a new plugin is being initialized */
static void devhelp_plugin_init(DevhelpPlugin * self)
{
	DevhelpPluginPrivate *p;

	g_return_if_fail(self != NULL);

	self->priv = p = G_TYPE_INSTANCE_GET_PRIVATE(self, DEVHELP_TYPE_PLUGIN, DevhelpPluginPrivate);
	devhelp_plugin_init_dh(self);

	p->in_message_window = FALSE;
	p->tabs_toggled = FALSE;

	p->doc_notebook = geany->main_widgets->notebook;
	p->main_notebook = main_notebook_get(); /* see main-notebook.c */

	devhelp_plugin_init_edit_menu(self);
	devhelp_plugin_init_sidebar(self);
	devhelp_plugin_init_webkit(self);

	p->last_main_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(p->main_notebook));
	p->last_sb_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));
}


DevhelpPlugin *devhelp_plugin_new(void)
{
	return g_object_new(DEVHELP_TYPE_PLUGIN, NULL);
}


void devhelp_plugin_search(DevhelpPlugin *self, const gchar *term)
{
	/* todo: fallback on manpages */
	devhelp_plugin_search_books(self, term);
}


/**
 * Search for a term in Devhelp and activate/show the plugin's UI stuff.
 *
 * @param dhplug	Devhelp plugin
 * @param term		The string to search for
 */
void devhelp_plugin_search_books(DevhelpPlugin *self, const gchar *term)
{

	g_return_if_fail(self != NULL);
	g_return_if_fail(term != NULL);

	dh_search_set_search_string(DH_SEARCH(self->priv->search), term, NULL);

	devhelp_plugin_activate_all_tabs(self);
}


/**
 * Search for a term in Manual Pages and activate/show the plugin's UI stuff.
 *
 * @param dhplug	Devhelp plugin
 * @param term		The string to search for
 */
void devhelp_plugin_search_manpages(DevhelpPlugin *self, const gchar *term)
{
	gchar *man_fn;

	g_return_if_fail(self != NULL);
	g_return_if_fail(term != NULL);

	man_fn = devhelp_plugin_manpages_search(term, NULL);

	if (man_fn == NULL)
		return;

	webkit_web_view_open(WEBKIT_WEB_VIEW(self->priv->webview), man_fn);

	g_free(man_fn);

	devhelp_plugin_activate_webview_tab(self);
}


/**
 * Cleans up a word/tag before searching.
 *
 * Replaces non @c GEANY_WORDCHARS in str with spaces and then trims whitespace.
 * This function does not allocate a new string, it modifies @a str in place
 * and returns a pointer to @a str.
 * TODO: make this only remove stuff from the start or end of string.
 *
 * @param	str	String to clean
 *
 * @return Pointer to (cleaned) @a str.
 */
gchar *devhelp_plugin_clean_word(gchar* str)
{
	return g_strstrip(g_strcanon(str, GEANY_WORDCHARS, ' '));
}

/**
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
			devhelp_plugin_clean_word(sci_get_selection_contents(doc->editor->sci));

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
 * Activate the plugin's tabs in the UI.
 *
 * @param dhplug Devhelp plugin
 * @param search_tabs_shown If TRUE, show the sidebar search tab, otherwise
 * 	show the contents tab.
 */
void devhelp_plugin_activate_ui(DevhelpPlugin *self, gboolean show_search_tab)
{
	g_return_if_fail(self != NULL);
	devhelp_plugin_set_active(self, TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(self->priv->sb_notebook), (show_search_tab) ? 1 : 0);
}


/**
 * Get whether or not the plugin's tabs are shown/active in the UI.
 *
 * @param dhplug Devhelp plugin
 */
gboolean devhelp_plugin_get_active(DevhelpPlugin *dhplug)
{
	return dhplug->priv->tabs_toggled;
}


/**
 * Set whether or not the plugin's tabs are shown/active in the UI.
 *
 * @param dhplug Devhelp plugin
 * @param active Whether to show the plugin's tabs or the previous tabs.
 */
void devhelp_plugin_set_active(DevhelpPlugin *dhplug, gboolean active)
{
	DevhelpPluginPrivate *p;
	GtkNotebook *main_nb, *sbar_nb;

	g_return_if_fail(dhplug != NULL);

	p = dhplug->priv;
	main_nb = GTK_NOTEBOOK(p->main_notebook);
	sbar_nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);

	/* just return if the active state is already correct */
	if ((active && p->tabs_toggled) || (!active && !p->tabs_toggled))
		return;

	if (active)
	{
		p->last_main_tab_id = gtk_notebook_get_current_page(main_nb);
		p->last_sb_tab_id = gtk_notebook_get_current_page(sbar_nb);
		gtk_notebook_set_current_page(main_nb, p->webview_tab);
		gtk_notebook_set_current_page(sbar_nb, p->sb_notebook_tab);
		p->tabs_toggled = TRUE;
	}
	else
	{
		gtk_notebook_set_current_page(main_nb, p->last_main_tab_id);
		gtk_notebook_set_current_page(sbar_nb, p->last_sb_tab_id);
		p->tabs_toggled = FALSE;
	}
}


/**
 * Gets whether the Geany sidebar tabs are in the bottom or their original position.
 *
 * @param self Devhelp plugin.
 */
gboolean devhelp_plugin_get_sidebar_tabs_bottom(DevhelpPlugin *self)
{
	GtkNotebook *nb;
	g_return_val_if_fail(self != NULL, FALSE);
	nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
	return gtk_notebook_get_tab_pos(nb) == GTK_POS_BOTTOM;
}


/**
 * Sets whether the Geany sidebar tabs are at the bottom or in their original position.
 *
 * @param self Devhelp plugin.
 * @param bottom If TRUE, set the sidebar tab position to @c GTK_POS_BOTTOM
 * otherwise set back to original position.
 */
void devhelp_plugin_set_sidebar_tabs_bottom(DevhelpPlugin *self, gboolean bottom)
{
	GtkNotebook *nb;
	g_return_if_fail(self != NULL);
	nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
	gtk_notebook_set_tab_pos(nb, bottom ? GTK_POS_BOTTOM : self->priv->orig_sb_tab_pos);
}


/**
 * Gets the last URI loaded in the webview.
 *
 * @param self Devhelp plugin.
 *
 * @return The last URI loaded in the webview.  This is an internal pointer,
 * do not modify or free.
 */
const gchar *devhelp_plugin_get_last_uri(DevhelpPlugin *self)
{
	g_return_val_if_fail(self != NULL, NULL);
	/* return webkit_web_view_get_uri(WEBKIT_WEB_VIEW(self->priv->webview)); */
	return (const gchar *)self->priv->last_uri;
}


/**
 * Gets whether the Documentation tab is in a main_notebook or in the
 * message window.
 *
 * @param self Devhelp plugin.
 *
 * @return TRUE if the Documentation tab is in a main_notebok or FALSE if it's
 * in the message window notebook.
 */
gboolean devhelp_plugin_get_is_in_msgwin(DevhelpPlugin *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	return self->priv->in_message_window;
}


/**
 * Sets whether the Documentation tab is in the message window notebook or is
 * in a main_notebook.
 *
 * @param self Devhelp plugin.
 * @param in_msgwin TRUE to move the Documentation tab to the message window
 * notebook or FALSE to create a main_notebook and put it there.
 */
void devhelp_plugin_set_is_in_msgwin(DevhelpPlugin *self, gboolean in_msgwin)
{
	GtkWidget *docs_notebook, *parent;

	g_return_if_fail(self != NULL);

	if (self->priv->in_message_window && !in_msgwin)
	{
		/* move from message win to main notebook */
		self->priv->main_notebook = main_notebook_get();

		//gtk_notebook_append_page(GTK_NOTEBOOK(self->priv->main_notebook),
		//	self
	}
	else if (!self->priv->in_message_window && in_msgwin)
	{
		/* move from main notebook to message win */
		gtk_widget_reparent(
			gtk_notebook_get_nth_page(GTK_NOTEBOOK(self->priv->main_notebook), self->priv->webview_tab),
			geany->main_widgets->message_window_notebook);

		/* remove the main notebook and put the documents notebook back */
		parent = gtk_widget_get_parent(self->priv->main_notebook);
		docs_notebook = gtk_widget_ref(geany->main_widgets->notebook);
		gtk_container_remove(GTK_CONTAINER(parent), self->priv->main_notebook);
		main_notebook_destroy();
		gtk_container_add(GTK_CONTAINER(parent), docs_notebook);
		gtk_widget_unref(docs_notebook);

		self->priv->main_notebook = geany->main_widgets->message_window_notebook;
	}

	self->priv->in_message_window = in_msgwin;
}


/**
 * Gets the zoom level of the WebKit WebView.
 *
 * @param self Devhelp plugin.
 *
 * @return The zoom level.
 */
gfloat devhelp_plugin_get_zoom_level(DevhelpPlugin *self)
{
	g_return_val_if_fail(self != NULL, 0.0F);
	self->priv->zoom_level = webkit_web_view_get_zoom_level(WEBKIT_WEB_VIEW(self->priv->webview));
	return self->priv->zoom_level;
}


/**
 * Sets the zoom level of the WebKit WebView.
 *
 * @param self Devhelp plugin.
 * @param zoom_level The zoom level to set.
 */
void devhelp_plugin_set_zoom_level(DevhelpPlugin *self, gfloat zoom_level)
{
	g_return_if_fail(self != NULL);
	self->priv->zoom_level = zoom_level;
	webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(self->priv->webview), zoom_level);
}


/**
 * Get the current URI from the WebKit WebView.
 *
 * @param self Devhelp plugin.
 *
 * @return The URI in the webview.  This is an internal value, do not free or
 * modify it.
 */
const gchar* devhelp_plugin_get_webview_uri(DevhelpPlugin *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return webkit_web_view_get_uri(WEBKIT_WEB_VIEW(self->priv->webview));
}


/**
 * Loads the specified @a uri in the WebKit WebView.
 *
 * @param self Devhelp plugin.
 * @param uri The URI to load in the webview.
 */
void devhelp_plugin_set_webview_uri(DevhelpPlugin *self, const gchar *uri)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(uri != NULL);

	webkit_web_view_open(WEBKIT_WEB_VIEW(self->priv->webview), uri);
}


WebKitWebView* devhelp_plugin_get_webview(DevhelpPlugin *self)
{
	g_return_val_if_fail(self != NULL, NULL);
	return WEBKIT_WEB_VIEW(self->priv->webview);
}


/* Activates (brings to top/makes visible) the Devhelp plugin's sidebar tab. */
static inline void devhelp_plugin_activate_sidebar_tab(DevhelpPlugin *self)
{
	gint current_tab_id;

	g_return_if_fail(self != NULL);

	current_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));
	if (current_tab_id != self->priv->sb_notebook_tab)
		self->priv->last_sb_tab_id = current_tab_id;

	gtk_widget_set_visible(self->priv->sb_notebook, TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
		self->priv->sb_notebook_tab);
}


/**
 * Activates the Devhelp plugin's sidebar tab, and then the internal notebook's
 * Search tab.
 *
 * @param self Devhelp plugin.
 */
void devhelp_plugin_activate_search_tab(DevhelpPlugin *self)
{
	g_return_if_fail(self != NULL);
	devhelp_plugin_activate_sidebar_tab(self);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(self->priv->sb_notebook), 1);
}


/**
 * Activates the Devhelp plugin's sidebar tab, and then the internal notebook's
 * Contents tab.
 *
 * @param self Devhelp plugin.
 */
void devhelp_plugin_activate_contents_tab(DevhelpPlugin *self)
{
	g_return_if_fail(self != NULL);
	devhelp_plugin_activate_sidebar_tab(self);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(self->priv->sb_notebook), 0);
}


/**
 * Activates the Devhelp plugin's main notebook's Documentation tab.
 *
 * @param self Devhelp plugin.
 */
void devhelp_plugin_activate_webview_tab(DevhelpPlugin *self)
{
	gint current_tab_id;

	g_return_if_fail(self != NULL);

	current_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(self->priv->main_notebook));
	if (current_tab_id != self->priv->webview_tab)
		self->priv->last_main_tab_id = current_tab_id;

	gtk_notebook_set_current_page(GTK_NOTEBOOK(self->priv->main_notebook),
		self->priv->webview_tab);
}


/* Toggles the Devhelp tab in the Geany sidebar notebook. */
static inline void devhelp_plugin_toggle_sidebar_tab(DevhelpPlugin *self, gint tab)
{
	GtkNotebook *sbar_nb;
	gint current_tab_id;

	g_return_if_fail(self != NULL);

	sbar_nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
	current_tab_id = gtk_notebook_get_current_page(sbar_nb);

	if (current_tab_id == self->priv->sb_notebook_tab)
		gtk_notebook_set_current_page(sbar_nb, self->priv->last_sb_tab_id);
	else
	{
		if (tab == 0)
			devhelp_plugin_activate_contents_tab(self);
		else if (tab == 1)
			devhelp_plugin_activate_search_tab(self);
		else
			g_warning("Can't toggle to unknown tab ID: %d", tab);
	}
}


/**
 * Toggle's between the Devhelp plugin's search tab and the previous Geany
 * sidebar tab.
 *
 * @param self Devhelp plugin.
 */
void devhelp_plugin_toggle_search_tab(DevhelpPlugin *self)
{
	devhelp_plugin_toggle_sidebar_tab(self, 1);
}


/**
 * Toggle's between the Devhelp plugin's contents tab and the previous Geany
 * sidebar tab.
 *
 * @param self Devhelp plugin.
 */
void devhelp_plugin_toggle_contents_tab(DevhelpPlugin *self)
{
	devhelp_plugin_toggle_sidebar_tab(self, 0);
}


/**
 * Toggle's between the Devhelp plugin's main notebook tab and the previous
 * main notebook tab.
 *
 * @param self Devhelp plugin.
 */
void devhelp_plugin_toggle_webview_tab(DevhelpPlugin *self)
{
	gint current_tab_id;

	g_return_if_fail(self != NULL);

	current_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(self->priv->main_notebook));
	if (current_tab_id == self->priv->last_main_tab_id)
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(self->priv->main_notebook),
			self->priv->last_main_tab_id);
	}
	else
		devhelp_plugin_activate_webview_tab(self);
}


/**
 * Activates/shows all the Devhelp plugins tabs.
 *
 * @param self Devhelp plugin.
 */
void devhelp_plugin_activate_all_tabs(DevhelpPlugin *self)
{
	devhelp_plugin_activate_sidebar_tab(self);
	devhelp_plugin_activate_webview_tab(self);
}


/* Called when the editor menu item is selected */
static void on_search_help_activate(GtkMenuItem * menuitem, DevhelpPlugin *self)
{
	gchar *current_tag;

	g_return_if_fail(self != NULL);

	if ((current_tag = devhelp_plugin_get_current_tag()) == NULL)
		return;

	devhelp_plugin_search_books(self, current_tag);

	g_free(current_tag);
}


/* Called when the editor menu item is selected */
static void on_search_help_man_activate(GtkMenuItem * menuitem, DevhelpPlugin *self)
{
	gchar *current_tag;

	g_return_if_fail(self != NULL);

	if ((current_tag = devhelp_plugin_get_current_tag()) == NULL)
		return;

	devhelp_plugin_search_manpages(self, current_tag);

	g_free(current_tag);
}


static void on_search_help_code_activate(GtkMenuItem *menuitem, DevhelpPlugin *self)
{
	gchar *current_tag;
	const gchar *lang = NULL;
	GeanyDocument *doc;

	g_return_if_fail(self != NULL);

	if ((current_tag = devhelp_plugin_get_current_tag()) == NULL)
		return;

	doc = document_get_current();
	if (doc == NULL || doc->file_type == NULL || doc->file_type->name == NULL)
		lang = doc->file_type->name;

	devhelp_plugin_search_code(self, current_tag, lang);

	g_free(current_tag);
}


/*
 * Called when the editor context menu is shown so that the devhelp
 * search item can be disabled if there isn't a selected tag.
 */
static void on_editor_menu_popup(GtkWidget * widget, DevhelpPlugin *self)
{
	gchar *label_tag, *curword, *new_label;

	g_return_if_fail(self != NULL);

	curword = devhelp_plugin_get_current_tag();

	if (curword == NULL)
	{
		gtk_widget_set_sensitive(self->priv->editor_menu_item, FALSE);
		return;
	}

	label_tag = g_strstrip(g_strndup(curword, DHPLUG_MAX_LABEL_TAG));
	new_label = g_strdup_printf(_("Search for '%s' Documentation in"), label_tag);

	gtk_menu_item_set_label(GTK_MENU_ITEM(self->priv->editor_menu_item), new_label);

	g_free(new_label);
	g_free(label_tag);

	gtk_widget_set_sensitive(self->priv->editor_menu_item, TRUE);

	g_free(curword);
}


/*
 * Called when a link in either the contents or search areas on the sidebar
 * have a link clicked on, meaning to load that file into the webview.
 */
static void on_link_clicked(GObject * ignored, DhLink * dhlink, DevhelpPlugin *self)
{
	g_return_if_fail(self != NULL);

	if (self->priv->last_uri != NULL)
		g_free(self->priv->last_uri);

	self->priv->last_uri = dh_link_get_uri(dhlink);
	webkit_web_view_open(WEBKIT_WEB_VIEW(self->priv->webview), self->priv->last_uri);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(self->priv->main_notebook), self->priv->webview_tab);
}


static void on_back_button_clicked(GtkToolButton * btn, DevhelpPlugin *self)
{
	g_return_if_fail(self != NULL);
	webkit_web_view_go_back(WEBKIT_WEB_VIEW(self->priv->webview));
}


static void on_forward_button_clicked(GtkToolButton * btn, DevhelpPlugin *self)
{
	g_return_if_fail(self != NULL);
	webkit_web_view_go_forward(WEBKIT_WEB_VIEW(self->priv->webview));
}


static void on_zoom_in_button_clicked(GtkToolButton * btn, DevhelpPlugin *self)
{
	WebKitWebView *view;

	g_return_if_fail(self != NULL);

	view = WEBKIT_WEB_VIEW(self->priv->webview);
	webkit_web_view_zoom_in(view);
	self->priv->zoom_level = webkit_web_view_get_zoom_level(view);
}


static void on_zoom_out_button_clicked(GtkToolButton *btn, DevhelpPlugin *self)
{
	WebKitWebView *view;

	g_return_if_fail(self != NULL);

	view = WEBKIT_WEB_VIEW(self->priv->webview);
	webkit_web_view_zoom_out(view);
	self->priv->zoom_level = webkit_web_view_get_zoom_level(view);
}


/* Controls the sensitivity of the back/forward buttons when webkit page changes. */
static inline void update_history_buttons(DevhelpPlugin *self)
{
	WebKitWebView *view;

	g_return_if_fail(self != NULL);

	view = WEBKIT_WEB_VIEW(self->priv->webview);

	gtk_widget_set_sensitive(GTK_WIDGET(self->priv->btn_back),
		webkit_web_view_can_go_back(view));
	gtk_widget_set_sensitive(GTK_WIDGET(self->priv->btn_forward),
		webkit_web_view_can_go_forward(view));
}


static void on_document_load_finished(WebKitWebView *view, WebKitWebFrame *frame, DevhelpPlugin *self)
{
	update_history_buttons(self);
}


static void on_uri_changed_notify(GObject *object, GParamSpec *pspec, DevhelpPlugin *self)
{
	update_history_buttons(self);
}


static void on_load_status_changed_notify(GObject *object, GParamSpec *pspec, DevhelpPlugin *self)
{
	update_history_buttons(self);
}

