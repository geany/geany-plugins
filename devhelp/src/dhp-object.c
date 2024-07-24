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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <geanyplugin.h>

#include <devhelp/dh-book-tree.h>
#include <devhelp/dh-link.h>
#include <devhelp/dh-web-view.h>
#include <devhelp/dh-search-bar.h>
#include <devhelp/dh-sidebar.h>


#ifdef HAVE_BOOK_MANAGER /* for newer api */
#include <devhelp/dh-book-manager.h>
#endif


#include "dhp.h"
#include "dhp-plugin.h"


struct _DevhelpPluginPrivate
{
	/* Devhelp stuff */
	GtkWidget*		book_tree;						/* "Contents" in the sidebar */
    GtkWidget*		grid;								/* "grid" in the sidebar */
    GtkWidget*		sidebar;							/* "DH_Sidebar" in the sidebar */
    GtkWidget*		search;							/* "Search" in the sidebar */
    GtkWidget*		sb_notebook;					/* "Notebook" that holds contents/search */

    /* Webview stuff */
    GtkWidget*		webview;							/*	Webkit that shows documentation */
    GtkWidget*		webview_tab;					/* The widget that has webview related stuff */
    GtkToolItem*	btn_back;						/*	the webkit browser back button in the toolbar */
    GtkToolItem*	btn_forward;					/*	the webkit browser forward button in the toolbar */
    DevhelpPluginWebViewLocation location; 	/*	where to pack the webview */

	/* Other widgets */
    GtkWidget*		main_notebook;					/*	Notebook that holds Geany doc notebook and and webkit view */
    GtkWidget*		editor_menu_item;				/*	Item in the editor's context menu */
    GtkWidget*		editor_menu_sep;				/*	Separator item above menu item */

	/* Position/tab number tracking */
    gboolean		last_main_tab_id;				/*	These track the last id of the tabs */
    gboolean		last_sb_tab_id;				/*	before toggling */
    gboolean		tabs_toggled;					/*	Tracks state of whether to toggle to Devhelp or back to code */
    GtkPositionType	orig_sb_tab_pos;			/*	The tab idx of the initial sidebar tab */
    gboolean		in_message_window;			/*	whether the webkit stuff is in the msgwin */

    GList*			temp_files;						/*	Tracks temp files made by the plugin to delete later. */

	 GKeyFile*	kf;
	 gboolean	focus_webview_on_search;
	 gboolean	focus_sidebar_on_search;
	 gchar*		custom_homepage;
	 gboolean	use_devhelp;
	 gboolean	use_man;
	 gboolean	use_codesearch;

	 gchar*		man_prog_path;
	 gchar*		man_pager_prog;
	 gchar*		man_section_order;

	 gchar*		codesearch_base_uri;
	 gchar*		codesearch_params;
	 gboolean	codesearch_use_lang;
	 gfloat		zoom_default;		

    GtkPositionType main_nb_tab_pos;
};


G_DEFINE_TYPE(DevhelpPlugin, devhelp_plugin, G_TYPE_OBJECT)


enum
{
	PROP_0,
	PROP_CURRENT_WORD,
	PROP_WEBVIEW_URI,
	PROP_SIDEBAR_TABS_BOTTOM,
	PROP_UI_ACTIVE,
	PROP_IN_MESSAGE_WINDOW,
	PROP_ZOOM_LEVEL,
	PROP_WEBVIEW,
	PROP_TEMP_FILES,
	PROP_MAN_PROG_PATH,
	PROP_HAVE_MAN_PROG,
	PROP_LAST
};

/* Causes problems if it gets re-initialized so leave global for now */
//static DhBase *dhbase = NULL;


/* Internal callbacks */
static void on_search_help_activate(GtkMenuItem * menuitem, DevhelpPlugin *self);
static void on_search_help_man_activate(GtkMenuItem * menuitem, DevhelpPlugin *self);
static void on_editor_menu_popup(GtkWidget * widget, DevhelpPlugin *self);
static void on_link_clicked(GObject * ignored, DhLink * dhlink, DevhelpPlugin *self);
static void on_back_button_clicked(GtkToolButton * btn, DevhelpPlugin *self);
static void on_forward_button_clicked(GtkToolButton * btn, DevhelpPlugin *self);
static void on_zoom_in_button_clicked(GtkToolButton * btn, DevhelpPlugin *self);
static void on_zoom_out_button_clicked(GtkToolButton *btn, DevhelpPlugin *self);
static void on_zoom_default_button_clicked(GtkToolButton *btn, DevhelpPlugin *self);
static void on_document_load_finished(WebKitWebView *view, DevhelpPlugin *self);
static void on_uri_changed_notify(GObject *object, GParamSpec *pspec, DevhelpPlugin *self);
static void on_load_status_changed_notify(GObject *object, GParamSpec *pspec, DevhelpPlugin *self);

/* Misc internal functions */
static inline void update_history_buttons(DevhelpPlugin *self);
static inline gchar* clean_word(gchar *str);
static GtkWidget* devhelp_plugin_ref_unpack_webview_tab(DevhelpPlugin *self);
static void devhelp_plugin_set_webview_location(DevhelpPlugin *self, DevhelpPluginWebViewLocation location);


/* include long boring code from dhp-settings.c */
#include "dhp-settings.c"


static void devhelp_plugin_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DevhelpPlugin *self = DEVHELP_PLUGIN(object);

	switch(property_id)
	{
		case PROP_WEBVIEW_URI:
			devhelp_plugin_set_webview_uri(self, g_value_get_string(value));
			break;
		case PROP_SIDEBAR_TABS_BOTTOM:
			devhelp_plugin_set_sidebar_tabs_bottom(self, g_value_get_boolean(value));
			break;
		case PROP_UI_ACTIVE:
			devhelp_plugin_set_ui_active(self, g_value_get_boolean(value));
			break;
		case PROP_IN_MESSAGE_WINDOW:
			devhelp_plugin_set_in_message_window(self, g_value_get_boolean(value));
			break;
		case PROP_ZOOM_LEVEL:
			devhelp_plugin_set_zoom_level(self, g_value_get_float(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}


static void devhelp_plugin_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DevhelpPlugin *self = DEVHELP_PLUGIN(object);

	switch(property_id)
	{
		case PROP_CURRENT_WORD:
			g_value_set_string(value, devhelp_plugin_get_current_word(self));
			break;
		case PROP_WEBVIEW_URI:
			g_value_set_string(value, devhelp_plugin_get_webview_uri(self));
			break;
		case PROP_SIDEBAR_TABS_BOTTOM:
			g_value_set_boolean(value, devhelp_plugin_get_sidebar_tabs_bottom(self));
			break;
		case PROP_UI_ACTIVE:
			g_value_set_boolean(value, devhelp_plugin_get_ui_active(self));
			break;
		case PROP_IN_MESSAGE_WINDOW:
			g_value_set_boolean(value, devhelp_plugin_get_in_message_window(self));
			break;
		case PROP_ZOOM_LEVEL:
			g_value_set_float(value, devhelp_plugin_get_zoom_level(self));
			break;
		case PROP_WEBVIEW:
			g_value_set_object(value, devhelp_plugin_get_webview(self));
			break;
		case PROP_TEMP_FILES:
			g_value_set_pointer(value, devhelp_plugin_get_temp_files(self));
			break;
		case PROP_MAN_PROG_PATH:
			g_value_set_string(value, devhelp_plugin_get_man_prog_path(self));
			break;
		case PROP_HAVE_MAN_PROG:
			g_value_set_boolean(value, devhelp_plugin_get_have_man_prog(self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}


/* Put cleanup code in here */
static void devhelp_plugin_finalize(GObject * object)
{
	DevhelpPlugin *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(DEVHELP_IS_PLUGIN(object));

	self = DEVHELP_PLUGIN(object);

	devhelp_plugin_set_sidebar_tabs_bottom(self, FALSE);
	devhelp_plugin_remove_manpages_temp_files(self);

	gtk_widget_destroy(self->priv->sb_notebook);

	g_object_unref(devhelp_plugin_ref_unpack_webview_tab(self));

	gtk_widget_destroy(self->priv->editor_menu_sep);
	gtk_widget_destroy(self->priv->editor_menu_item);

	g_free(self->priv->man_prog_path);
	g_free(self->priv->man_pager_prog);
	g_free(self->priv->man_section_order);
	g_free(self->priv->codesearch_base_uri);
	g_free(self->priv->codesearch_params);
	g_free(self->priv->custom_homepage);

	G_OBJECT_CLASS(devhelp_plugin_parent_class)->finalize(object);
}


static void devhelp_plugin_class_init(DevhelpPluginClass * klass)
{
	GObjectClass *g_object_class;
	GParamSpec *pspec;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->set_property = devhelp_plugin_set_property;
	g_object_class->get_property = devhelp_plugin_get_property;
	g_object_class->finalize = devhelp_plugin_finalize;

	pspec = g_param_spec_string(
				"current-word",
				"Current word in editor",
				"Gets the currently selected word, or the word where the cursor is.",
				NULL,
				G_PARAM_READABLE);
	g_object_class_install_property(g_object_class, PROP_CURRENT_WORD, pspec);

	pspec = g_param_spec_string(
				"webview-uri",
				"WebKit WebView URI",
				"Gets or sets the URI loaded in the WebView widget.",
				DHPLUG_WEBVIEW_HOME_FILE,
				G_PARAM_READWRITE);
	g_object_class_install_property(g_object_class, PROP_WEBVIEW_URI, pspec);

	pspec = g_param_spec_boolean(
				"sidebar-tabs-bottom",
				"Sidebar tabs on bottom",
				"Gets or sets whether Geany's sidebar tabs are positioned on the bottom or not.",
				FALSE,
				G_PARAM_READWRITE);
	g_object_class_install_property(g_object_class, PROP_SIDEBAR_TABS_BOTTOM, pspec);

	pspec = g_param_spec_boolean(
				"ui-active",
				"UI widgets are active",
				"Gets or sets whether the plugin's UI widgets are active/selected/visible.",
				FALSE,
				G_PARAM_READWRITE);
	g_object_class_install_property(g_object_class, PROP_UI_ACTIVE, pspec);

	pspec = g_param_spec_boolean(
				"in-message-window",
				"Documentation in the message window notebook",
				"Gets or sets whether the notebook page with the WebKit WebView is located "
					"in the main documents notebook area or in the message window notebook.",
				FALSE,
				G_PARAM_READWRITE);
	g_object_class_install_property(g_object_class, PROP_IN_MESSAGE_WINDOW, pspec);

	pspec = g_param_spec_float(
				"zoom-level",
				"Zoom level of the WebKit WebView",
				"Gets or sets the zoom level (font size, etc) of the contents of the "
					"WebKit WebView.",
				G_MINFLOAT, G_MAXFLOAT/* FIXME */, 1.0F,
				G_PARAM_READWRITE);
	g_object_class_install_property(g_object_class, PROP_ZOOM_LEVEL, pspec);

	pspec = g_param_spec_object(
				"webview",
				"WebKit WebView",
				"Gets the WebKit WebView used to display documentation.",
				WEBKIT_TYPE_WEB_VIEW,
				G_PARAM_READABLE);
	g_object_class_install_property(g_object_class, PROP_WEBVIEW, pspec);

	pspec = g_param_spec_pointer(
				"temp-files",
				"Temporary files",
				"Gets the GList used to track temporary files so they can be deleted later.",
				G_PARAM_READABLE);
	g_object_class_install_property(g_object_class, PROP_TEMP_FILES, pspec);

	pspec = g_param_spec_string(
				"man-prog-path",
				"Manual page program path",
				"Gets the path to the system's 'man' program if present.",
				NULL,
				G_PARAM_READABLE);
	g_object_class_install_property(g_object_class, PROP_MAN_PROG_PATH, pspec);

	pspec = g_param_spec_boolean(
				"have-man-prog",
				"Have manual page program",
				"Gets whether a manual page program was found on the system.",
				FALSE,
				G_PARAM_READABLE);
	g_object_class_install_property(g_object_class, PROP_HAVE_MAN_PROG, pspec);

	g_type_class_add_private((gpointer) klass, sizeof(DevhelpPluginPrivate));
}


/*
 * Initialize the Devhelp library/widgets.
 * The Devhelp API isn't exactly stable, so handle quirks in here.
 */
static void devhelp_plugin_init_dh(DevhelpPlugin *self)
{	
	DhProfile *dh_profile;
	
	dh_profile = dh_profile_get_default();
	
		
	/* Sidebar */
    self->priv->sidebar = GTK_WIDGET(dh_sidebar_new2 (dh_profile));
    gtk_widget_show (GTK_WIDGET (self->priv->sidebar));
    
	//Grid for sidebar
	self->priv->grid = gtk_grid_new();
	gtk_grid_attach (GTK_GRID (self->priv->grid), self->priv->sidebar, 0, 0, 1, 1);

		
	self->priv->book_tree = GTK_WIDGET(dh_book_tree_new(dh_profile));
	self->priv->search = GTK_WIDGET(self->priv->grid);
	
	/* Focus search in sidebar by default. */
    //dh_sidebar_set_search_focus (self->priv->sidebar);

	
	gtk_widget_show(self->priv->search);
		
	g_signal_connect(self->priv->book_tree, "link-selected", G_CALLBACK(on_link_clicked), self);
	g_signal_connect(self->priv->sidebar, "link-selected", G_CALLBACK(on_link_clicked), self);

}


/* Initialize the stuff in the editor's context menu */
static void devhelp_plugin_init_edit_menu(DevhelpPlugin *self)
{
	GtkWidget *doc_menu, *devhelp_item, *man_item;
	DevhelpPluginPrivate *p;

	g_return_if_fail(self != NULL);

	p = self->priv;

	p->editor_menu_sep = gtk_separator_menu_item_new();
	p->editor_menu_item = gtk_menu_item_new_with_label(_("Search for 'Tag' Documentation in"));
	doc_menu = gtk_menu_new();

	devhelp_item = gtk_menu_item_new_with_label(_("Devhelp"));
	gtk_menu_shell_append(GTK_MENU_SHELL(doc_menu), devhelp_item);
	g_signal_connect(devhelp_item, "activate", G_CALLBACK(on_search_help_activate), self);
	gtk_widget_show(devhelp_item);

	if (devhelp_plugin_get_have_man_prog(self))
	{
		man_item = gtk_menu_item_new_with_label(_("Manual Pages"));
		gtk_menu_shell_append(GTK_MENU_SHELL(doc_menu), man_item);
		g_signal_connect(man_item, "activate", G_CALLBACK(on_search_help_man_activate), self);
		gtk_widget_show(man_item);
	}

	plugin_signal_connect(geany_plugin, G_OBJECT(geany->main_widgets->editor_menu), "show", TRUE,
		G_CALLBACK(on_editor_menu_popup), self);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(p->editor_menu_item), doc_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu), p->editor_menu_sep);
	gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu), p->editor_menu_item);
	gtk_widget_show(p->editor_menu_sep);
	gtk_widget_show_all(p->editor_menu_item);
}


/* Initialize the WebKit browser and associated widgets */
static void devhelp_plugin_init_webkit(DevhelpPlugin *self)
{
	GtkWidget *vbox, *toolbar;
	GtkToolItem *btn_zoom_in, *btn_zoom_out,*btn_zoom_default, *tb_sep;
	GtkScrolledWindow *webview_sw;
	DevhelpPluginPrivate *p;

	g_return_if_fail(self != NULL);

	p = self->priv;

	p->webview = webkit_web_view_new();
	
	p->zoom_default = devhelp_plugin_get_zoom_level(self);

	webview_sw = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
	gtk_scrolled_window_set_policy(webview_sw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(webview_sw, GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(webview_sw), p->webview);

	gtk_widget_show_all(GTK_WIDGET(webview_sw));

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	toolbar = gtk_toolbar_new();

	p->btn_back = gtk_tool_button_new(gtk_image_new_from_icon_name ("go-previous", GTK_ICON_SIZE_SMALL_TOOLBAR),NULL);
	p->btn_forward = gtk_tool_button_new(gtk_image_new_from_icon_name ("go-next", GTK_ICON_SIZE_SMALL_TOOLBAR),NULL);
	btn_zoom_in = gtk_tool_button_new(gtk_image_new_from_icon_name ("zoom-in", GTK_ICON_SIZE_SMALL_TOOLBAR),NULL);
	btn_zoom_out = gtk_tool_button_new(gtk_image_new_from_icon_name ("zoom-out", GTK_ICON_SIZE_SMALL_TOOLBAR),NULL);
	btn_zoom_default = gtk_tool_button_new(gtk_image_new_from_icon_name ("zoom-original", GTK_ICON_SIZE_SMALL_TOOLBAR),NULL);
	tb_sep = gtk_separator_tool_item_new();

	gtk_widget_set_tooltip_text(GTK_WIDGET(p->btn_back), _("Go back one page"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(p->btn_forward), _("Go forward one page"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoom_in), _("Zoom in"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoom_out), _("Zoom out"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoom_default), _("Normal Size"));

	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), p->btn_back, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), p->btn_forward, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_sep, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoom_in, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoom_out, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoom_default, -1);

	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(webview_sw), TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);
	p->webview_tab = vbox;
	devhelp_plugin_set_webview_location(self, DEVHELP_PLUGIN_WEBVIEW_LOCATION_MAIN_NOTEBOOK);

	g_signal_connect(p->btn_back, "clicked", G_CALLBACK(on_back_button_clicked), self);
	g_signal_connect(p->btn_forward, "clicked", G_CALLBACK(on_forward_button_clicked), self);
	g_signal_connect(btn_zoom_in, "clicked", G_CALLBACK(on_zoom_in_button_clicked), self);
	g_signal_connect(btn_zoom_out, "clicked", G_CALLBACK(on_zoom_out_button_clicked), self);
	g_signal_connect(btn_zoom_default, "clicked", G_CALLBACK(on_zoom_default_button_clicked), self);

	g_signal_connect(WEBKIT_WEB_VIEW(p->webview), "document-load-finished", G_CALLBACK(on_document_load_finished), self);
	g_signal_connect(WEBKIT_WEB_VIEW(p->webview), "notify::uri", G_CALLBACK(on_uri_changed_notify), self);
	g_signal_connect(WEBKIT_WEB_VIEW(p->webview), "notify::load-status", G_CALLBACK(on_load_status_changed_notify), self);

	devhelp_plugin_set_webview_uri(self, NULL);
}


/* Initialize the stuff added to the sidebar. */
static void devhelp_plugin_init_sidebar(DevhelpPlugin *self)
{
	GtkWidget *label;
	GtkWidget *book_tree_sw, *search_sw;
	DevhelpPluginPrivate *p;

	g_return_if_fail(self != NULL);

	p = self->priv;

	p->sb_notebook = gtk_notebook_new();
	p->orig_sb_tab_pos = gtk_notebook_get_tab_pos(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));

	book_tree_sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(book_tree_sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(book_tree_sw), 6);
	gtk_container_add(GTK_CONTAINER(book_tree_sw), GTK_WIDGET(p->book_tree));
	gtk_widget_show(p->book_tree);
	
	search_sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(search_sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(search_sw), 6);
	gtk_container_add(GTK_CONTAINER(search_sw), GTK_WIDGET(p->search));
	gtk_widget_show(p->search);


	label = gtk_label_new(_("Contents"));
	gtk_notebook_append_page(GTK_NOTEBOOK(p->sb_notebook), book_tree_sw, label);

	label = gtk_label_new(_("Search"));
	gtk_notebook_append_page(GTK_NOTEBOOK(p->sb_notebook), search_sw, label);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(p->sb_notebook), 0);
	gtk_widget_show_all(p->sb_notebook);

	label = gtk_label_new(_("Devhelp"));
	gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), p->sb_notebook, label);
	
}


/* Called when a new plugin is being initialized */
static void devhelp_plugin_init(DevhelpPlugin * self)
{
	DevhelpPluginPrivate *p;

	g_return_if_fail(self != NULL);
	p = G_TYPE_INSTANCE_GET_PRIVATE(self, DEVHELP_TYPE_PLUGIN, DevhelpPluginPrivate);
	self->priv = p;
	devhelp_plugin_init_dh(self);

	p->in_message_window = FALSE;
	p->tabs_toggled = FALSE;
	p->location = DEVHELP_PLUGIN_WEBVIEW_LOCATION_NONE;
	p->main_notebook = NULL;

	p->kf = NULL;
	p->focus_webview_on_search = TRUE;
	p->focus_sidebar_on_search = TRUE;
	p->custom_homepage = NULL;
	p->use_devhelp = TRUE;
	p->use_man = TRUE;
	p->use_codesearch = TRUE;

	p->man_prog_path = g_find_program_in_path("man");
	p->man_pager_prog = g_strdup("col -b");
	p->man_section_order = g_strdup("3:2:1:8:5:4:7:6");

	p->codesearch_base_uri = g_strdup("http://www.google.com/codesearch");
	p->codesearch_params = NULL;
	p->codesearch_use_lang = TRUE;

    p->main_nb_tab_pos = GTK_POS_BOTTOM;

	devhelp_plugin_init_edit_menu(self);
	devhelp_plugin_init_sidebar(self);
	devhelp_plugin_init_webkit(self);

	p->last_main_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(p->main_notebook));
	p->last_sb_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));
}


/** Create a new Devhelp plugin instance. */
DevhelpPlugin *devhelp_plugin_new(void)
{
	return g_object_new(DEVHELP_TYPE_PLUGIN, NULL);
}


/**
 * Gets the WebKit WebView object used by the plugin.
 *
 * @param self Devhelp plugin.
 *
 * @return The webview used by the plugin.
 */
const gchar *devhelp_plugin_get_webview_uri(DevhelpPlugin *self)
{
	//WebKitWebFrame *frame;

	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), NULL);

	//frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(self->priv->webview));
	if (self->priv->webview)
		return webkit_web_view_get_uri(WEBKIT_WEB_VIEW(self->priv->webview));
	else
		return NULL;
}


/**
 * Loads the specified @a uri in the WebKit WebView.
 *
 * @param self Devhelp plugin.
 * @param uri The URI to load in the webview or NULL for default homepage.
 */
void devhelp_plugin_set_webview_uri(DevhelpPlugin *self, const gchar *uri)
{
	/* stolen from Webhelper plugin */
	gchar *real_uri;
	gchar *scheme;

	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	if (uri == NULL)
		real_uri = g_filename_to_uri(DHPLUG_WEBVIEW_HOME_FILE, NULL, NULL);
	else
		real_uri = g_strdup(uri);

	scheme = g_uri_parse_scheme(real_uri);
	if (!scheme)
	{
		gchar *tmp = g_strconcat("http://", uri, NULL);
		g_free(real_uri);
		real_uri = tmp;
	}
	g_free(scheme);

	if (g_strcmp0(real_uri, devhelp_plugin_get_webview_uri(self)) != 0)
	{
		webkit_web_view_load_uri(WEBKIT_WEB_VIEW(self->priv->webview), real_uri);
		g_object_notify(G_OBJECT(self), "webview-uri");
	}
	g_free(real_uri);
}


/**
 * Same as devhelp_plugin_search_books().
 *
 * @param self Devhelp plugin.
 * @param term The search term to look up.
 */
void devhelp_plugin_search(DevhelpPlugin *self, const gchar *term)
{
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

	dh_sidebar_set_search_string(DH_SIDEBAR(self->priv->sidebar), term);

	devhelp_plugin_activate_all_tabs(self);
}


/*
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
static inline gchar *clean_word(gchar* str)
{
	return g_strstrip(g_strcanon(str, GEANY_WORDCHARS, ' '));
}


/**
 * Gets either the current selection or the word at the current selection.
 *
 * @return Newly allocated string with current tag or NULL no tag.
 */
gchar *devhelp_plugin_get_current_word(DevhelpPlugin *self)
{
	gint pos;
	gchar *tag = NULL;
	GeanyDocument *doc = document_get_current();

	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), NULL);

	if (doc == NULL || doc->editor == NULL || doc->editor->sci == NULL)
		return NULL;

	if (sci_has_selection(doc->editor->sci))
		return clean_word(sci_get_selection_contents(doc->editor->sci));

	pos = sci_get_current_position(doc->editor->sci);
	tag = editor_get_word_at_pos(doc->editor, pos, GEANY_WORDCHARS);

	if (tag == NULL || strlen(tag) == 0)
	{
		g_free(tag /* might be null, ok */);
		return NULL;
	}

	return clean_word(tag);
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
	devhelp_plugin_set_ui_active(self, TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(self->priv->sb_notebook), (show_search_tab) ? 1 : 0);
}


/**
 * Get whether or not the plugin's tabs are shown/active in the UI.
 *
 * @param self Devhelp plugin.
 *
 * @return TRUE if the UI elements are active FALSE if not.
 */
gboolean devhelp_plugin_get_ui_active(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), FALSE);
	return self->priv->tabs_toggled;
}


/**
 * Set whether or not the plugin's tabs are shown/active in the UI.
 *
 * @param self Devhelp plugin
 * @param active Whether to show the plugin's tabs or the previous tabs.
 */
void devhelp_plugin_set_ui_active(DevhelpPlugin *self, gboolean active)
{
	GtkNotebook *main_nb, *sbar_nb;

	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	main_nb = GTK_NOTEBOOK(self->priv->main_notebook);
	sbar_nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);

	if (active && !devhelp_plugin_get_ui_active(self))
	{
		self->priv->last_main_tab_id = gtk_notebook_get_current_page(main_nb);
		self->priv->last_sb_tab_id = gtk_notebook_get_current_page(sbar_nb);
		gtk_notebook_set_current_page(main_nb, gtk_notebook_page_num(main_nb, self->priv->webview_tab));
		gtk_notebook_set_current_page(sbar_nb, gtk_notebook_page_num(sbar_nb, self->priv->sb_notebook));
		self->priv->tabs_toggled = TRUE;
		g_object_notify(G_OBJECT(self), "ui-active");
	}
	else if (!active && devhelp_plugin_get_ui_active(self))
	{
		gtk_notebook_set_current_page(main_nb, self->priv->last_main_tab_id);
		gtk_notebook_set_current_page(sbar_nb, self->priv->last_sb_tab_id);
		self->priv->tabs_toggled = FALSE;
		g_object_notify(G_OBJECT(self), "ui-active");
	}
}


/**
 * Gets whether the Geany sidebar tabs are in the bottom or their original position.
 *
 * @param self Devhelp plugin.
 */
gboolean devhelp_plugin_get_sidebar_tabs_bottom(DevhelpPlugin *self)
{
	GtkPositionType pos;
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), FALSE);
	pos = gtk_notebook_get_tab_pos(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));
	return  pos == GTK_POS_BOTTOM;
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

	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);

	if (!devhelp_plugin_get_sidebar_tabs_bottom(self) && bottom)
	{
		self->priv->orig_sb_tab_pos = gtk_notebook_get_tab_pos(nb);
		gtk_notebook_set_tab_pos(nb, GTK_POS_BOTTOM);
		g_object_notify(G_OBJECT(self), "sidebar-tabs-bottom");
	}
	else if (devhelp_plugin_get_sidebar_tabs_bottom(self) && !bottom)
	{
		gtk_notebook_set_tab_pos(nb, self->priv->orig_sb_tab_pos);
		g_object_notify(G_OBJECT(self), "sidebar-tabs-bottom");
	}
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
gboolean devhelp_plugin_get_in_message_window(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), FALSE);
	return self->priv->in_message_window;
}


/* Refs the webkit tab stuff and removes it from parents, returns pointer to
 * widget that was ref'd. */
static GtkWidget* devhelp_plugin_ref_unpack_webview_tab(DevhelpPlugin *self)
{
	GtkWidget *parent, *doc_nb;

	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), NULL);

	/* Prevents a segfault bug from popping up when messing with doc notebook */
	gtk_widget_set_sensitive(geany->main_widgets->notebook, FALSE);

	g_object_ref(self->priv->webview_tab);

	if (self->priv->location != DEVHELP_PLUGIN_WEBVIEW_LOCATION_NONE)
	{
		parent = gtk_widget_get_parent(self->priv->webview_tab);
		gtk_container_remove(GTK_CONTAINER(parent), self->priv->webview_tab);
	}

	/* If we were using the "main notebook", put the UI back to normal */
	if (self->priv->location == DEVHELP_PLUGIN_WEBVIEW_LOCATION_MAIN_NOTEBOOK)
	{
		parent = gtk_widget_get_parent(self->priv->main_notebook);
		doc_nb = g_object_ref(geany->main_widgets->notebook);
		gtk_container_remove(GTK_CONTAINER(self->priv->main_notebook), doc_nb);
		gtk_container_remove(GTK_CONTAINER(parent), self->priv->main_notebook);
		gtk_container_add(GTK_CONTAINER(parent), doc_nb);
		g_object_unref(doc_nb);
		self->priv->main_notebook = NULL;
	}

	self->priv->location = DEVHELP_PLUGIN_WEBVIEW_LOCATION_NONE;

	gtk_widget_set_sensitive(geany->main_widgets->notebook, TRUE);

	return self->priv->webview_tab;
}


static void devhelp_plugin_set_webview_location(DevhelpPlugin *self, DevhelpPluginWebViewLocation location)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	if (location == self->priv->location) /* no change, so do nothing */
		return;

	switch(location)
	{
		case DEVHELP_PLUGIN_WEBVIEW_LOCATION_SIDEBAR:
			gtk_notebook_append_page(
				GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
				devhelp_plugin_ref_unpack_webview_tab(self),
				gtk_label_new(DEVHELP_PLUGIN_WEBVIEW_TAB_LABEL));
			g_object_unref(self->priv->webview_tab);
			self->priv->location = DEVHELP_PLUGIN_WEBVIEW_LOCATION_SIDEBAR;
			break;
		case DEVHELP_PLUGIN_WEBVIEW_LOCATION_MESSAGE_WINDOW:
			gtk_notebook_append_page(
				GTK_NOTEBOOK(geany->main_widgets->message_window_notebook),
				devhelp_plugin_ref_unpack_webview_tab(self),
				gtk_label_new(DEVHELP_PLUGIN_WEBVIEW_TAB_LABEL));
			g_object_unref(self->priv->webview_tab);
			self->priv->location = DEVHELP_PLUGIN_WEBVIEW_LOCATION_MESSAGE_WINDOW;
			break;
		case DEVHELP_PLUGIN_WEBVIEW_LOCATION_MAIN_NOTEBOOK:
		{
			GtkWidget *parent, *doc_nb, *main_notebook, *webview_tab;

			webview_tab = devhelp_plugin_ref_unpack_webview_tab(self);
			doc_nb = geany->main_widgets->notebook;
			parent = gtk_widget_get_parent(doc_nb);

			g_object_ref(doc_nb);

			gtk_container_remove(GTK_CONTAINER(parent), doc_nb);
			main_notebook = gtk_notebook_new();
			self->priv->main_notebook = main_notebook;

			gtk_notebook_append_page(GTK_NOTEBOOK(main_notebook),
				doc_nb, gtk_label_new(DEVHELP_PLUGIN_DOCUMENTS_TAB_LABEL));
			gtk_notebook_append_page(GTK_NOTEBOOK(main_notebook),
				webview_tab, gtk_label_new(DEVHELP_PLUGIN_WEBVIEW_TAB_LABEL));

			gtk_container_add(GTK_CONTAINER(parent), main_notebook);

			gtk_widget_show_all(doc_nb);
			gtk_widget_show_all(webview_tab);
			gtk_widget_show_all(main_notebook);

			g_object_unref(doc_nb);
			g_object_unref(webview_tab);

			self->priv->location = DEVHELP_PLUGIN_WEBVIEW_LOCATION_MAIN_NOTEBOOK;
			break;
		}
		default:
			g_warning("Unable to set location of webview.");
			break;
	}
}


/**
 * Sets whether the Documentation tab is in the message window notebook or is
 * in a main_notebook.
 *
 * @param self Devhelp plugin.
 * @param in_msgwin TRUE to move the Documentation tab to the message window
 * notebook or FALSE to create a main_notebook and put it there.
 */
void devhelp_plugin_set_in_message_window(DevhelpPlugin *self, gboolean in_msgwin)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	if (in_msgwin && !self->priv->in_message_window)
	{
		devhelp_plugin_set_webview_location(self,
			DEVHELP_PLUGIN_WEBVIEW_LOCATION_MESSAGE_WINDOW);
		self->priv->in_message_window = TRUE;
		g_object_notify(G_OBJECT(self), "in-message-window");
	}
	else if (!in_msgwin && self->priv->in_message_window)
	{
		devhelp_plugin_set_webview_location(self,
			DEVHELP_PLUGIN_WEBVIEW_LOCATION_MAIN_NOTEBOOK);
		self->priv->in_message_window = FALSE;
		g_object_notify(G_OBJECT(self), "in-message-window");
	}
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
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), 0.0F);
	return webkit_web_view_get_zoom_level(WEBKIT_WEB_VIEW(self->priv->webview));
}


/**
 * Sets the zoom level of the WebKit WebView.
 *
 * @param self Devhelp plugin.
 * @param zoom_level The zoom level to set.
 */
void devhelp_plugin_set_zoom_level(DevhelpPlugin *self, gfloat zoom_level)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	if (devhelp_plugin_get_zoom_level(self) != zoom_level )
	{
		webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(self->priv->webview), zoom_level);
		g_object_notify(G_OBJECT(self), "zoom-level");
	}
}


WebKitWebView* devhelp_plugin_get_webview(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), NULL);
	return WEBKIT_WEB_VIEW(self->priv->webview);
}


/**
 * Returns a pointer to the list used to track temp files.
 *
 * @param self Devhelp plugin.
 *
 * @return Pointer to the internal list used to track temp files.
 */
GList* devhelp_plugin_get_temp_files(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), NULL);
	return self->priv->temp_files;
}


/**
 * Returns the path to the 'man' program on the system or NULL if one was not
 * found.
 *
 * @param self Devhelp plugin.
 *
 * @return The path to the 'man' program or NULL if not found.  Free the
 * returned value when no longer needed.
 */
const gchar* devhelp_plugin_get_man_prog_path(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), NULL);
	return (const gchar *)self->priv->man_prog_path;
}


/**
 * Finds whether or not a 'man' program is available (in the path) on the
 * current system.
 *
 * @param self Devhelp plugin.
 *
 * @return TRUE if a 'man' program was found or FALSE if not.
 */
gboolean devhelp_plugin_get_have_man_prog(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), FALSE);
	return devhelp_plugin_get_man_prog_path(self) != NULL;
}


/* TODO: make properties for the following accessor functions */

gboolean devhelp_plugin_get_devhelp_sidebar_visible(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), FALSE);
#if GTK_CHECK_VERSION(2,18,0)
	return gtk_widget_get_visible(self->priv->sb_notebook);
#else
	return GTK_WIDGET_VISIBLE(self->priv->sb_notebook);
#endif
}


void devhelp_plugin_set_devhelp_sidebar_visible(DevhelpPlugin *self, gboolean visible)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	gtk_widget_set_visible(self->priv->sb_notebook, visible);
}


gboolean devhelp_plugin_get_use_devhelp(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), FALSE);
	return self->priv->use_devhelp;
}


void devhelp_plugin_set_use_devhelp(DevhelpPlugin *self, gboolean use)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	self->priv->use_devhelp = use;
	gtk_widget_set_visible(self->priv->sb_notebook, use);
	/* TODO: hide edit menu items and keybindings */
	/* TODO: if no providers, hide webview */
}


gboolean devhelp_plugin_get_use_man(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), FALSE);
	return self->priv->use_man;
}


void devhelp_plugin_set_use_man(DevhelpPlugin *self, gboolean use)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	self->priv->use_man = use;
	/* TODO: hide edit menu items and keybindings */
	/* TODO: if no providers, hide webview */
}


gboolean devhelp_plugin_get_use_codesearch(DevhelpPlugin *self)
{
	g_return_val_if_fail(DEVHELP_IS_PLUGIN(self), FALSE);
	return self->priv->use_codesearch;
}


void devhelp_plugin_set_use_codesearch(DevhelpPlugin *self, gboolean use)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	self->priv->use_codesearch = use;
	/* TODO: hide edit menu items and keybindings */
	/* TODO: if no providers, hide webview */
}


/* Activates (brings to top/makes visible) the Devhelp plugin's sidebar tab. */
static inline void devhelp_plugin_activate_sidebar_tab(DevhelpPlugin *self)
{
	GtkNotebook *nb;
	gint current_tab_id;

	g_return_if_fail(self != NULL);

	nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);

	current_tab_id = gtk_notebook_get_current_page(nb);
	if (current_tab_id != gtk_notebook_page_num(nb, self->priv->sb_notebook))
		self->priv->last_sb_tab_id = current_tab_id;

	gtk_widget_set_visible(self->priv->sb_notebook, TRUE);
	gtk_notebook_set_current_page(nb, gtk_notebook_page_num(nb, self->priv->sb_notebook));
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
	GtkNotebook *nb;
	gint current_tab_id;

	g_return_if_fail(self != NULL);

	nb = GTK_NOTEBOOK(self->priv->main_notebook);

	current_tab_id = gtk_notebook_get_current_page(nb);
	if (current_tab_id != gtk_notebook_page_num(nb, self->priv->webview_tab))
		self->priv->last_main_tab_id = current_tab_id;

	gtk_notebook_set_current_page(nb, gtk_notebook_page_num(nb, self->priv->webview_tab));
}


/* Toggles the Devhelp tab in the Geany sidebar notebook. */
static inline void devhelp_plugin_toggle_sidebar_tab(DevhelpPlugin *self, gint tab)
{
	GtkNotebook *sbar_nb;
	gint current_tab_id;

	g_return_if_fail(self != NULL);

	sbar_nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
	current_tab_id = gtk_notebook_get_current_page(sbar_nb);

	if (current_tab_id == gtk_notebook_page_num(sbar_nb, self->priv->sb_notebook))
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

	if ((current_tag = devhelp_plugin_get_current_word(self)) == NULL)
		return;

	devhelp_plugin_search_books(self, current_tag);

	g_free(current_tag);
}


/* Called when the editor menu item is selected */
static void on_search_help_man_activate(GtkMenuItem * menuitem, DevhelpPlugin *self)
{
	gchar *current_tag;

	g_return_if_fail(self != NULL);

	if ((current_tag = devhelp_plugin_get_current_word(self)) == NULL)
		return;

	devhelp_plugin_search_manpages(self, current_tag);

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

	curword = devhelp_plugin_get_current_word(self);

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
	gchar *uri;

	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	uri = dh_link_get_uri(dhlink);
	devhelp_plugin_set_webview_uri(self, uri);
	devhelp_plugin_activate_webview_tab(self);
	g_free(uri);
}


static void on_back_button_clicked(GtkToolButton * btn, DevhelpPlugin *self)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	webkit_web_view_go_back(WEBKIT_WEB_VIEW(self->priv->webview));
}


static void on_forward_button_clicked(GtkToolButton * btn, DevhelpPlugin *self)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	webkit_web_view_go_forward(WEBKIT_WEB_VIEW(self->priv->webview));
}


static void on_zoom_in_button_clicked(GtkToolButton * btn, DevhelpPlugin *self)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	//webkit_web_view_zoom_in(devhelp_plugin_get_webview(self));

	///webkit_web_view_set_zoom_level(devhelp_plugin_get_webview(self), 1);

	//dh_web_view_zoom_out(devhelp_plugin_get_webview(self));
	
    gfloat current_zoom = webkit_web_view_get_zoom_level(devhelp_plugin_get_webview(self));

    devhelp_plugin_set_zoom_level(self, current_zoom + 0.1);

}
static void on_zoom_default_button_clicked(GtkToolButton *btn, DevhelpPlugin *self)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	
	devhelp_plugin_set_zoom_level(self, self->priv->zoom_default);
}

static void on_zoom_out_button_clicked(GtkToolButton *btn, DevhelpPlugin *self)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));
	//webkit_web_view_zoom_out(devhelp_plugin_get_webview(self));
	///webkit_web_view_set_zoom_level(devhelp_plugin_get_webview(self), -1);
	//dh_web_view_zoom_in(devhelp_plugin_get_webview(self));
	
	gfloat current_zoom = webkit_web_view_get_zoom_level(devhelp_plugin_get_webview(self));

   devhelp_plugin_set_zoom_level(self, current_zoom - 0.1);
}


/* Controls the sensitivity of the back/forward buttons when webkit page changes. */
static inline void update_history_buttons(DevhelpPlugin *self)
{
	g_return_if_fail(DEVHELP_IS_PLUGIN(self));

	gtk_widget_set_sensitive(GTK_WIDGET(self->priv->btn_back),
		webkit_web_view_can_go_back(devhelp_plugin_get_webview(self)));
	gtk_widget_set_sensitive(GTK_WIDGET(self->priv->btn_forward),
		webkit_web_view_can_go_forward(devhelp_plugin_get_webview(self)));
}


static void on_document_load_finished(WebKitWebView *view, DevhelpPlugin *self)
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

