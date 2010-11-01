/*
 *  
 *  Copyright (C) 2010  Colomban Wendling <ban@herbesfolles.org>
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
