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

#ifndef H_GWH_UTILS
#define H_GWH_UTILS

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS


/* emulates some values of GdkWindowTypeHint */
typedef enum _GwhWindowType
{
  GWH_WINDOW_TYPE_NORMAL  = GDK_WINDOW_TYPE_HINT_NORMAL,
  GWH_WINDOW_TYPE_UTILITY = GDK_WINDOW_TYPE_HINT_UTILITY
} GwhWindowType;


G_GNUC_INTERNAL
GdkPixbuf      *gwh_pixbuf_new_from_uri       (const gchar *uri,
                                               GError     **error);
G_GNUC_INTERNAL
gchar          *gwh_get_window_geometry       (GtkWindow *window,
                                               gint       default_x,
                                               gint       default_y);
G_GNUC_INTERNAL
void            gwh_set_window_geometry       (GtkWindow   *window,
                                               const gchar *geometry,
                                               gint        *x_,
                                               gint        *y_);


G_END_DECLS

#endif /* guard */
