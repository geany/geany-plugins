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

#ifndef H_GWH_BROWSER
#define H_GWH_BROWSER

#include <glib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS


#define GWH_TYPE_BROWSER        (gwh_browser_get_type ())
#define GWH_BROWSER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GWH_TYPE_BROWSER, GwhBrowser))
#define GWH_BROWSER_CLASS(ko)   (G_TYPE_CHECK_CLASS_CAST ((k), GWH_TYPE_BROWSER, GwhBrowserClass))
#define GWH_IS_BROWSER(b)       (G_TYPE_CHECK_INSTANCE_TYPE ((b), GWH_TYPE_BROWSER))
#define GWH_IS_BROWSER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GWH_TYPE_BROWSER))


typedef enum {
  GWH_BROWSER_POSITION_MESSAGE_WINDOW,
  GWH_BROWSER_POSITION_SIDEBAR
} GwhBrowserPosition;


typedef struct _GwhBrowser        GwhBrowser;
typedef struct _GwhBrowserClass   GwhBrowserClass;
typedef struct _GwhBrowserPrivate GwhBrowserPrivate;

struct _GwhBrowser
{
  GtkVBox parent;
  
  GwhBrowserPrivate *priv;
};

struct _GwhBrowserClass
{
  GtkVBoxClass parent_class;
  
  void        (*populate_popup)       (GwhBrowser *browser,
                                       GtkMenu    *menu);
};


G_GNUC_INTERNAL
GType           gwh_browser_get_type                      (void) G_GNUC_CONST;
G_GNUC_INTERNAL
GtkWidget      *gwh_browser_new                           (void);
G_GNUC_INTERNAL
void            gwh_browser_set_uri                       (GwhBrowser  *self,
                                                           const gchar *uri);
G_GNUC_INTERNAL
const gchar    *gwh_browser_get_uri                       (GwhBrowser *self);
G_GNUC_INTERNAL
GtkToolbar     *gwh_browser_get_toolbar                   (GwhBrowser *self);
G_GNUC_INTERNAL
WebKitWebView  *gwh_browser_get_web_view                  (GwhBrowser *self);
G_GNUC_INTERNAL
void            gwh_browser_reload                        (GwhBrowser *self);
G_GNUC_INTERNAL
void            gwh_browser_set_inspector_transient_for   (GwhBrowser *self,
                                                           GtkWindow  *window);
G_GNUC_INTERNAL
GtkWindow      *gwh_browser_get_inspector_transient_for   (GwhBrowser *self);


G_END_DECLS

#endif /* guard */
