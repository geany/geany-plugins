/*
 *  
 *  Copyright (C) 2010-2011  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#ifndef H_GWH_KEYBINDINGS
#define H_GWH_KEYBINDINGS

#include <glib.h>
#include <gtk/gtk.h>

#include <geanyplugin.h>
#include <geany.h>

G_BEGIN_DECLS


enum {
  GWH_KB_TOGGLE_INSPECTOR,
  GWH_KB_SHOW_HIDE_SEPARATE_WINDOW,
  GWH_KB_COUNT
};


G_GNUC_INTERNAL
void            gwh_keybindings_init          (void);
G_GNUC_INTERNAL
void            gwh_keybindings_cleanup       (void);
G_GNUC_INTERNAL
GeanyKeyGroup  *gwh_keybindings_get_group     (void);
G_GNUC_INTERNAL
gboolean        gwh_keybindings_handle_event  (GtkWidget   *widget G_GNUC_UNUSED,
                                               GdkEventKey *event,
                                               gpointer     data G_GNUC_UNUSED);


G_END_DECLS

#endif /* guard */
