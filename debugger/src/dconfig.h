/*
 *		dconfig.h
 *      
 *      Copyright 2011 Alexander Petukhov <devel(at)apetukhov.ru>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <stdarg.h>

/* panel config parts */
#define CP_TABBED_MODE 1
#define CP_OT_TABS 2
#define CP_OT_SELECTED 3
#define CP_TT_LTABS 4
#define CP_TT_LSELECTED 5
#define CP_TT_RTABS 6
#define CP_TT_RSELECTED 7


typedef enum _debug_store {
	DEBUG_STORE_PLUGIN,
	DEBUG_STORE_PROJECT
} debug_store;


void		config_init(void);
void		config_destroy(void);

void		config_set_panel(int config_part, gpointer config_value, ...);

gboolean	config_get_save_to_project(void);

gboolean	config_get_tabbed(void);

int*		config_get_tabs(gsize *length);
int			config_get_selected_tab_index(void);

int*		config_get_left_tabs(gsize *length);
int			config_get_left_selected_tab_index(void);

int*		config_get_right_tabs(gsize *length);
int			config_get_right_selected_tab_index(void);

void		config_set_debug_changed(void);
void		config_set_debug_store(debug_store store);

void		config_on_project_open(GObject *obj, GKeyFile *config, gpointer user_data);
void		config_on_project_close(GObject *obj, gpointer user_data);
void		config_on_project_save(GObject *obj, GKeyFile *config, gpointer user_data);

void		config_update_project_keyfile(void);

GtkWidget	*config_plugin_configure(GtkDialog *dialog);
