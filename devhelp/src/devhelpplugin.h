/*
 * devhelpplugin.h
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

#ifndef __DEVHELPPLUGIN_H__
#define __DEVHELPPLUGIN_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#ifndef DHPLUG_DATA_DIR
#define DHPLUG_DATA_DIR "/usr/local/share/geany-devhelp"
#endif

#define DHPLUG_WEBVIEW_HOME_FILE DHPLUG_DATA_DIR"/home.html"
#define DHPLUG_MAX_LABEL_TAG 30 /* never search for more than this many chars */

#define DEVHELP_TYPE_PLUGIN				(devhelp_plugin_get_type())
#define DEVHELP_PLUGIN(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), DEVHELP_TYPE_PLUGIN, DevhelpPlugin))
#define DEVHELP_PLUGIN_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), DEVHELP_TYPE_PLUGIN, DevhelpPluginClass))
#define DEVHELP_IS_PLUGIN(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), DEVHELP_TYPE_PLUGIN))
#define DEVHELP_IS_PLUGIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), DEVHELP_TYPE_PLUGIN))
#define DEVHELP_PLUGIN_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), DEVHELP_TYPE_PLUGIN, DevhelpPluginClass))
			
typedef struct _DevhelpPlugin DevhelpPlugin;
typedef struct _DevhelpPluginClass DevhelpPluginClass;
typedef struct _DevhelpPluginPrivate DevhelpPluginPrivate;

struct _DevhelpPlugin
{
    GObject parent;

    GtkWidget *book_tree;       /* "Contents" in the sidebar */
    GtkWidget *search;          /* "Search" in the sidebar */
    GtkWidget *sb_notebook;     /* Notebook that holds contents/search */
    gint sb_notebook_tab;       /* Index of tab where devhelp sidebar is */
    GtkWidget *webview;         /* Webkit that shows documentation */
    gint webview_tab;           /* Index of tab that contains the webview */
    GtkWidget *main_notebook;   /* Notebook that holds Geany doc notebook and
								 * and webkit view */
    GtkWidget *doc_notebook;    /* Geany's document notebook   */
    GtkWidget *editor_menu_item;        /* Item in the editor's context menu */
    GtkWidget *editor_menu_sep; /* Separator item above menu item */
    gboolean *webview_active;   /* Tracks whether webview stuff is shown */

    gboolean last_main_tab_id;  /* These track the last id of the tabs */
    gboolean last_sb_tab_id;    /*   before toggling */
    gboolean tabs_toggled;      /* Tracks state of whether to toggle to
								 * Devhelp or back to code */
    gboolean created_main_nb;   /* Track whether we created the main notebook */

    GtkPositionType orig_sb_tab_pos;
    gboolean sidebar_tab_bottom;
    gboolean in_message_window;

    gchar *last_uri;
    gfloat zoom_level;

    GtkToolItem *btn_back;
    GtkToolItem *btn_forward;

    DevhelpPluginPrivate *priv;
};

struct _DevhelpPluginClass
{
    GObjectClass parent_class;
};


GType devhelp_plugin_get_type(void);
DevhelpPlugin *devhelp_plugin_new(gboolean sb_tabs_bottom, gboolean show_in_msgwin, gchar * last_uri);
gchar *devhelp_plugin_clean_word(gchar * str);
gchar *devhelp_plugin_get_current_tag(void);
void devhelp_plugin_activate_tabs(DevhelpPlugin * dhplug, gboolean contents);
void devhelp_plugin_sidebar_tabs_bottom(DevhelpPlugin * dhplug, gboolean bottom);

G_END_DECLS
#endif /* __DEVHELPPLUGIN_H__ */
