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

#include "gwh-utils.h"

#include <glib.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>



GdkPixbuf *
gwh_pixbuf_new_from_uri (const gchar *uri,
                         GError     **error)
{
  GdkPixbuf        *pixbuf = NULL;
  GFile            *file;
  GInputStream     *stream;
  
  file = g_file_new_for_uri (uri);
  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
  if (stream) {
    GdkPixbufLoader  *loader;
    guchar            buf[BUFSIZ];
    gboolean          success = TRUE;
    
    loader = gdk_pixbuf_loader_new ();
    while (success) {
      gssize n_read;
      
      n_read = g_input_stream_read (stream, buf, sizeof (buf), NULL, error);
      if (n_read < 0) {
        success = FALSE;
      } else {
        if (n_read > 0) {
          success = gdk_pixbuf_loader_write (loader, buf, (gsize)n_read, error);
        }
        if (success && (gsize)n_read < sizeof (buf)) {
          success = gdk_pixbuf_loader_close (loader, error);
          break;
        }
      }
    }
    if (success) {
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
      if (pixbuf) {
        g_object_ref (pixbuf);
      } else {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Unknown error");
      }
    }
    g_object_unref (loader);
    g_object_unref (stream);
  }
  g_object_unref (file);
  
  return pixbuf;
}

/**
 * gwh_get_window_geometry:
 * @window: A #GtkWindow
 * @default_x: Default position on X if not computable
 * @default_y: Default position on Y if not computable
 * 
 * Gets the XWindow-style geometry of a #GtkWindow
 * 
 * Returns: A newly-allocated string representing the window geometry.
 */
gchar *
gwh_get_window_geometry (GtkWindow *window,
                         gint       default_x,
                         gint       default_y)
{
  gint width;
  gint height;
  gint x;
  gint y;
  
  gtk_window_get_size (window, &width, &height);
  if (gtk_widget_get_visible (GTK_WIDGET (window))) {
    gtk_window_get_position (window, &x, &y);
  } else {
    x = default_x;
    y = default_y;
  }
  
  return g_strdup_printf ("%dx%d%+d%+d", width, height, x, y);
}

/**
 * gwh_set_window_geometry:
 * @window: A #GtkWindow
 * @geometry: a XWindow-style geometry
 * @x_: (out): return location for the window's X coordinate
 * @y_: (out): return location for the window's Y coordinate
 * 
 * Sets the geometry of a window from a XWindow-style geometry.
 */
void
gwh_set_window_geometry (GtkWindow   *window,
                         const gchar *geometry,
                         gint        *x_,
                         gint        *y_)
{
  gint            width;
  gint            height;
  gint            x;
  gint            y;
  gchar           dummy;
  GdkWindowHints  hints_mask = 0;
  
  g_return_if_fail (geometry != NULL);
  
  gtk_window_get_size (window, &width, &height);
  gtk_window_get_position (window, &x, &y);
  switch (sscanf (geometry, "%dx%d%d%d%c", &width, &height, &x, &y, &dummy)) {
    case 4:
    case 3:
      if (x_) *x_ = x;
      if (y_) *y_ = y;
      gtk_window_move (window, x, y);
      hints_mask |= GDK_HINT_USER_POS;
      /* fallthrough */
    case 2:
    case 1:
      gtk_window_resize (window, width, height);
      hints_mask |= GDK_HINT_USER_SIZE;
      break;
    
    default:
      g_warning ("Invalid window geometry \"%s\"", geometry);
  }
  gtk_window_set_geometry_hints (window, NULL, NULL, hints_mask);
}
