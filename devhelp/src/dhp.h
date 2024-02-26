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

#ifndef DEVHELP_PLUGIN_COMMON_H
#define DEVHELP_PLUGIN_COMMON_H

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

G_BEGIN_DECLS


#ifndef DHPLUG_DATA_DIR
#define DHPLUG_DATA_DIR "/usr/local/share/geany-devhelp"
#endif

#define DHPLUG_WEBVIEW_HOME_FILE DHPLUG_DATA_DIR "/home.html"
#define DHPLUG_MAX_LABEL_TAG 30 /* never search for more than this many chars */

#define DEVHELP_PLUGIN_WEBVIEW_TAB_LABEL	_("Documentation")
#define DEVHELP_PLUGIN_DOCUMENTS_TAB_LABEL	_("Code")

#define DEVHELP_TYPE_PLUGIN				(devhelp_plugin_get_type())
#define DEVHELP_PLUGIN(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), DEVHELP_TYPE_PLUGIN, DevhelpPlugin))
#define DEVHELP_PLUGIN_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), DEVHELP_TYPE_PLUGIN, DevhelpPluginClass))
#define DEVHELP_IS_PLUGIN(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), DEVHELP_TYPE_PLUGIN))
#define DEVHELP_IS_PLUGIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), DEVHELP_TYPE_PLUGIN))
#define DEVHELP_PLUGIN_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), DEVHELP_TYPE_PLUGIN, DevhelpPluginClass))


typedef struct	_DevhelpPlugin			DevhelpPlugin;
typedef struct	_DevhelpPluginClass		DevhelpPluginClass;
typedef struct	_DevhelpPluginPrivate	DevhelpPluginPrivate;




struct _DevhelpPlugin
{
    GObject parent;
    DevhelpPluginPrivate *priv;
};

//G_DEFINE_TYPE_WITH_CODE(DevhelpPlugin, devhelpplugin, G_TYPE_OBJECT, G_ADD_PRIVATE(DevhelpPlugin))


struct _DevhelpPluginClass
{
    GObjectClass parent_class;
};


typedef enum
{
	DEVHELP_PLUGIN_WEBVIEW_LOCATION_NONE,
	DEVHELP_PLUGIN_WEBVIEW_LOCATION_SIDEBAR,
	DEVHELP_PLUGIN_WEBVIEW_LOCATION_MESSAGE_WINDOW,
	DEVHELP_PLUGIN_WEBVIEW_LOCATION_MAIN_NOTEBOOK
} DevhelpPluginWebViewLocation;


GType			devhelp_plugin_get_type	(void);
DevhelpPlugin*	devhelp_plugin_new		(void);


void			devhelp_plugin_load_settings			(DevhelpPlugin *self, const gchar *filename);
void			devhelp_plugin_store_settings			(DevhelpPlugin *self, const gchar *filename);


/* Property getters */
gchar*			devhelp_plugin_get_current_word			(DevhelpPlugin *self);
const gchar*	devhelp_plugin_get_webview_uri			(DevhelpPlugin *self);
gboolean		devhelp_plugin_get_sidebar_tabs_bottom	(DevhelpPlugin *self);
gboolean		devhelp_plugin_get_ui_active			(DevhelpPlugin *self);
gboolean		devhelp_plugin_get_in_message_window	(DevhelpPlugin *self);
gfloat			devhelp_plugin_get_zoom_level			(DevhelpPlugin *self);
WebKitWebView*	devhelp_plugin_get_webview				(DevhelpPlugin *self);
GList*			devhelp_plugin_get_temp_files			(DevhelpPlugin *self);
const gchar*	devhelp_plugin_get_man_prog_path		(DevhelpPlugin *self);
gboolean		devhelp_plugin_get_have_man_prog		(DevhelpPlugin *self);


/* Property setters */
void			devhelp_plugin_set_webview_uri			(DevhelpPlugin *self, const gchar *uri);
void			devhelp_plugin_set_sidebar_tabs_bottom	(DevhelpPlugin *self, gboolean bottom);
void			devhelp_plugin_set_ui_active			(DevhelpPlugin *self, gboolean active);
void			devhelp_plugin_set_in_message_window	(DevhelpPlugin *self, gboolean in_msgwin);
void			devhelp_plugin_set_zoom_level			(DevhelpPlugin *self, gfloat zoom_level);


/* Methods */
void			devhelp_plugin_activate_ui				(DevhelpPlugin *self, gboolean show_search_tab);
void			devhelp_plugin_activate_search_tab		(DevhelpPlugin *self);
void			devhelp_plugin_activate_contents_tab	(DevhelpPlugin *self);
void			devhelp_plugin_activate_webview_tab		(DevhelpPlugin *self);
void			devhelp_plugin_activate_all_tabs		(DevhelpPlugin *self);

void			devhelp_plugin_toggle_search_tab		(DevhelpPlugin *self);
void			devhelp_plugin_toggle_contents_tab		(DevhelpPlugin *self);
void			devhelp_plugin_toggle_webview_tab		(DevhelpPlugin *self);

void 			devhelp_plugin_search					(DevhelpPlugin *self, const gchar *term);
void 			devhelp_plugin_search_books				(DevhelpPlugin *self, const gchar *term);
void			devhelp_plugin_search_code				(DevhelpPlugin *self, const gchar *term, const gchar *lang);


/* Manual pages (see manpages.c) */
void 			devhelp_plugin_search_manpages				(DevhelpPlugin *self, const gchar *term);
gchar*			devhelp_plugin_manpages_search				(DevhelpPlugin *self, const gchar *term, const gchar *section);
void			devhelp_plugin_add_temp_file				(DevhelpPlugin *self, const gchar *filename);
void			devhelp_plugin_remove_manpages_temp_files	(DevhelpPlugin *self);


/* TODO: make properties for these */
gboolean devhelp_plugin_get_devhelp_sidebar_visible(DevhelpPlugin *self);
void devhelp_plugin_set_devhelp_sidebar_visible(DevhelpPlugin *self, gboolean visible);
gboolean devhelp_plugin_get_use_devhelp(DevhelpPlugin *self);
void devhelp_plugin_set_use_devhelp(DevhelpPlugin *self, gboolean use);
gboolean devhelp_plugin_get_use_man(DevhelpPlugin *self);
void devhelp_plugin_set_use_man(DevhelpPlugin *self, gboolean use);
gboolean devhelp_plugin_get_use_codesearch(DevhelpPlugin *self);
void devhelp_plugin_set_use_codesearch(DevhelpPlugin *self, gboolean use);


G_END_DECLS
#endif /* DEVHELP_PLUGIN_H */
