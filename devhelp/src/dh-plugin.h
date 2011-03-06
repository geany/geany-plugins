/*
 * dhplug.h - Part of the Geany Devhelp Plugin
 * 
 * Copyright 2010 Matthew Brush <mbrush@leftclick.ca>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 
#ifndef DH_PLUGIN_H
#define DH_PLUGIN_H

#include <gtk/gtk.h>

#ifndef DHPLUG_DATA_DIR
#define DHPLUG_DATA_DIR "/usr/local/share/geany-devhelp"
#endif

#define DHPLUG_WEBVIEW_HOME_FILE DHPLUG_DATA_DIR"/home.html"

/* main plugin struct */
typedef struct
{
	GtkWidget *book_tree;			/// "Contents" in the sidebar
	GtkWidget *search;				/// "Search" in the sidebar
	GtkWidget *sb_notebook;			/// Notebook that holds contents/search
	gint sb_notebook_tab;			/// Index of tab where devhelp sidebar is
	GtkWidget *webview;				/// Webkit that shows documentation
	gint webview_tab;				/// Index of tab that contains the webview
	GtkWidget *main_notebook;		/// Notebook that holds Geany doc notebook and
									/// and webkit view
	GtkWidget *doc_notebook;		/// Geany's document notebook  
	GtkWidget *editor_menu_item;	/// Item in the editor's context menu 
	GtkWidget *editor_menu_sep;		/// Separator item above menu item
	gboolean *webview_active;		/// Tracks whether webview stuff is shown
	
	gboolean last_main_tab_id;		/// These track the last id of the tabs
	gboolean last_sb_tab_id;		///   before toggling
	gboolean tabs_toggled;			/// Tracks state of whether to toggle to
									/// Devhelp or back to code
	gboolean created_main_nb;		/// Track whether we created the main notebook
	
	GtkPositionType orig_sb_tab_pos;
	gboolean sidebar_tab_bottom;
	gboolean in_message_window;

} DevhelpPlugin;

gchar *devhelp_clean_word(gchar *str);
gchar *devhelp_get_current_tag(void);
void devhelp_activate_tabs(DevhelpPlugin *dhplug, gboolean contents);
DevhelpPlugin *devhelp_plugin_new(gboolean sb_tabs_bottom, gboolean show_in_msgwin);
void devhelp_plugin_destroy(DevhelpPlugin *dhplug);
void devhelp_plugin_sidebar_tabs_bottom(DevhelpPlugin *dhplug, gboolean bottom);

#endif
