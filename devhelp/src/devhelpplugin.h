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
#include <webkit/webkitwebview.h>

G_BEGIN_DECLS


#ifndef DHPLUG_DATA_DIR
#define DHPLUG_DATA_DIR "/usr/local/share/geany-devhelp"
#endif

#define DHPLUG_WEBVIEW_HOME_FILE DHPLUG_DATA_DIR "/home.html"
#define DHPLUG_MAX_LABEL_TAG 30 /* never search for more than this many chars */

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


struct _DevhelpPluginClass
{
    GObjectClass parent_class;
};


GType			devhelp_plugin_get_type				(void);
DevhelpPlugin*	devhelp_plugin_new					(void);

gchar*			devhelp_plugin_clean_word			(gchar *str);
gchar*			devhelp_plugin_get_current_tag		(void);

const gchar*	devhelp_plugin_get_webview_uri		(DevhelpPlugin *self);
void			devhelp_plugin_set_webview_uri		(DevhelpPlugin *self, const gchar *uri);

gboolean		devhelp_plugin_get_sidebar_tabs_bottom	(DevhelpPlugin *self);
void			devhelp_plugin_set_sidebar_tabs_bottom	(DevhelpPlugin *self, gboolean bottom);

gboolean		devhelp_plugin_get_active			(DevhelpPlugin *self);
void			devhelp_plugin_set_active			(DevhelpPlugin *self, gboolean active);

gboolean		devhelp_plugin_get_is_in_msgwin		(DevhelpPlugin *self);
void			devhelp_plugin_set_is_in_msgwin		(DevhelpPlugin *self, gboolean in_msgwin);

void			devhelp_plugin_activate_ui			(DevhelpPlugin *self, gboolean show_search_tab);
void 			devhelp_plugin_search				(DevhelpPlugin *self, const gchar *term);
void 			devhelp_plugin_search_books			(DevhelpPlugin *self, const gchar *term);
void 			devhelp_plugin_search_manpages		(DevhelpPlugin *self, const gchar *term);
void			devhelp_plugin_search_code			(DevhelpPlugin *self, const gchar *term, const gchar *lang);

const gchar*	devhelp_plugin_get_last_uri			(DevhelpPlugin *self);
void			devhelp_plugin_set_last_uri			(DevhelpPlugin *self, const gchar *uri);

gfloat			devhelp_plugin_get_zoom_level		(DevhelpPlugin *self);
void			devhelp_plugin_set_zoom_level		(DevhelpPlugin *self, gfloat zoom_level);

void			devhelp_plugin_activate_search_tab		(DevhelpPlugin *self);
void			devhelp_plugin_activate_contents_tab	(DevhelpPlugin *self);
void			devhelp_plugin_activate_webview_tab		(DevhelpPlugin *self);
void			devhelp_plugin_activate_all_tabs		(DevhelpPlugin *self);

void			devhelp_plugin_toggle_search_tab		(DevhelpPlugin *self);
void			devhelp_plugin_toggle_contents_tab		(DevhelpPlugin *self);
void			devhelp_plugin_toggle_webview_tab		(DevhelpPlugin *self);

WebKitWebView*	devhelp_plugin_get_webview				(DevhelpPlugin *self);


gchar *devhelp_plugin_manpages_search(const gchar *term, const gchar *section);
void devhelp_plugin_remove_manpages_temp_files(void);


void devhelp_plugin_search_code(DevhelpPlugin *self, const gchar *term, const gchar *lang);


G_END_DECLS
#endif /* DEVHELP_PLUGIN_H */
