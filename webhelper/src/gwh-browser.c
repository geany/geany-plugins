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

#include "gwh-browser.h"

#include "config.h"

#include <stdio.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "gwh-utils.h"
#include "gwh-settings.h"


#if ! GTK_CHECK_VERSION (2, 18, 0)
# ifndef gtk_widget_set_visible
#  define gtk_widget_set_visible(w, v) \
  ((v) ? gtk_widget_show (w) : gtk_widget_hide (w))
# endif /* defined (gtk_widget_set_visible) */
# ifndef gtk_widget_get_visible
#  define gtk_widget_get_visible(w) \
  (GTK_WIDGET_VISIBLE (w))
# endif /* defined (gtk_widget_get_visible) */
#endif


struct _GwhBrowserPrivate
{
  GwhSettings        *settings;
  
  GIcon        *default_icon;
  
  GtkWidget          *toolbar;
  GtkWidget          *paned;
  GtkWidget          *web_view;
  WebKitWebInspector *inspector;
  GtkWidget          *inspector_view; /* the widget that should be shown to
                                       * display the inspector, not necessarily
                                       * a WebKitWebView */
  GtkWidget          *inspector_window;
  gint                inspector_window_x;
  gint                inspector_window_y;
  GtkWidget          *inspector_web_view;
  
  GtkWidget    *url_entry;
  GtkToolItem  *item_prev;
  GtkToolItem  *item_next;
  GtkToolItem  *item_cancel;
  GtkToolItem  *item_reload;
  GtkToolItem  *item_inspector;
};

enum {
  PROP_0,
  PROP_INSPECTOR_TRANSIENT_FOR,
  PROP_ORIENTATION,
  PROP_URI,
  PROP_WEB_VIEW,
  PROP_TOOLBAR
};

enum {
  POPULATE_POPUP,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};


G_DEFINE_TYPE_WITH_CODE (GwhBrowser, gwh_browser, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))



static void
set_location_icon (GwhBrowser  *self,
                   const gchar *icon_uri)
{
  gboolean success = FALSE;
  
  if (icon_uri) {
    GdkPixbuf *icon;
    
    icon = gwh_pixbuf_new_from_uri (icon_uri, NULL);
    if (icon) {
      gtk_entry_set_icon_from_pixbuf (GTK_ENTRY (self->priv->url_entry),
                                      GTK_ENTRY_ICON_PRIMARY, icon);
      g_object_unref (icon);
      success = TRUE;
    }
  }
  if (! success) {
    if (G_UNLIKELY (self->priv->default_icon == NULL)) {
      gchar *ctype;
      
      ctype = g_content_type_from_mime_type ("text/html");
      self->priv->default_icon = g_content_type_get_icon (ctype);
      g_free (ctype);
    }
    gtk_entry_set_icon_from_gicon (GTK_ENTRY (self->priv->url_entry),
                                   GTK_ENTRY_ICON_PRIMARY,
                                   self->priv->default_icon);
  }
}

static gchar *
get_web_inspector_window_geometry (GwhBrowser *self)
{
  gint        width;
  gint        height;
  gint        x;
  gint        y;
  GtkWindow  *window;
  
  window = GTK_WINDOW (self->priv->inspector_window);
  gtk_window_get_size (window, &width, &height);
  if (gtk_widget_get_visible (GTK_WIDGET (window))) {
    gtk_window_get_position (window, &x, &y);
  } else {
    x = self->priv->inspector_window_x;
    y = self->priv->inspector_window_y;
  }
  
  return g_strdup_printf ("%dx%d%+d%+d", width, height, x, y);
}

static void
set_web_inspector_window_geometry (GwhBrowser  *self,
                                   const gchar *geometry)
{
  gint        width;
  gint        height;
  gint        x;
  gint        y;
  gchar       dummy;
  GtkWindow  *window;
  
  g_return_if_fail (geometry != NULL);
  
  window = GTK_WINDOW (self->priv->inspector_window);
  gtk_window_get_size (window, &width, &height);
  gtk_window_get_position (window, &x, &y);
  switch (sscanf (geometry, "%dx%d%d%d%c", &width, &height, &x, &y, &dummy)) {
    case 4:
    case 3:
      self->priv->inspector_window_x = x;
      self->priv->inspector_window_y = y;
      gtk_window_move (window, x, y);
      /* fallthrough */
    case 2:
    case 1:
      gtk_window_resize (window, width, height);
      break;
    
    default:
      g_warning ("Invalid window geometry \"%s\"", geometry);
  }
}

/* settings change notifications */

static void
on_settings_browser_last_uri_notify (GObject    *object,
                                     GParamSpec *pspec,
                                     GwhBrowser *self)
{
  gchar *uri;
  
  g_object_get (object, pspec->name, &uri, NULL);
  gwh_browser_set_uri (self, uri);
  g_free (uri);
}

static void
on_settings_browser_orientation_notify (GObject    *object,
                                        GParamSpec *pspec,
                                        GwhBrowser *self)
{
  GtkOrientation orientation;
  
  g_object_get (object, pspec->name, &orientation, NULL);
  if (orientation != gtk_orientable_get_orientation (GTK_ORIENTABLE (self))) {
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), orientation);
  }
}

static void
on_settings_inspector_window_geometry_notify (GObject    *object,
                                              GParamSpec *pspec,
                                              GwhBrowser *self)
{
  gchar *geometry;
  
  g_object_get (object, pspec->name, &geometry, NULL);
  set_web_inspector_window_geometry (self, geometry);
  g_free (geometry);
}

/* web inspector events handling */

#define INSPECTOR_DETACHED(self) \
  (gtk_bin_get_child (GTK_BIN ((self)->priv->inspector_window)) != NULL)

#define INSPECTOR_VISIBLE(self) \
  (gtk_widget_get_visible ((self)->priv->inspector_view))

static void
inspector_hide_window (GwhBrowser *self)
{
  if (gtk_widget_get_visible (self->priv->inspector_window)) {
    gtk_window_get_position (GTK_WINDOW (self->priv->inspector_window),
                             &self->priv->inspector_window_x,
                             &self->priv->inspector_window_y);
    gtk_widget_hide (self->priv->inspector_window);
  }
}

static void
inspector_show_window (GwhBrowser *self)
{
  if (! gtk_widget_get_visible (self->priv->inspector_window)) {
    gtk_widget_show (self->priv->inspector_window);
    gtk_window_move (GTK_WINDOW (self->priv->inspector_window),
                     self->priv->inspector_window_x,
                     self->priv->inspector_window_y);
  }
}

static WebKitWebView *
on_inspector_inspect_web_view (WebKitWebInspector *inspector,
                               WebKitWebView      *view,
                               GwhBrowser         *self)
{
  return WEBKIT_WEB_VIEW (self->priv->inspector_web_view);
}

static gboolean
on_inspector_show_window (WebKitWebInspector *inspector,
                          GwhBrowser         *self)
{
  gtk_widget_show (self->priv->inspector_view);
  if (INSPECTOR_DETACHED (self)) {
    inspector_show_window (self);
  }
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (self->priv->item_inspector),
                                     TRUE);
  
  return TRUE;
}

static gboolean
on_inspector_close_window (WebKitWebInspector *inspector,
                           GwhBrowser         *self)
{
  gtk_widget_hide (self->priv->inspector_view);
  inspector_hide_window (self);
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (self->priv->item_inspector),
                                     FALSE);
  
  return TRUE;
}

static gboolean
on_inspector_detach_window (WebKitWebInspector *inspector,
                            GwhBrowser         *self)
{
  gtk_widget_reparent (self->priv->inspector_view,
                       self->priv->inspector_window);
  inspector_show_window (self);
  
  return TRUE;
}

static gboolean
on_inspector_attach_window (WebKitWebInspector *inspector,
                            GwhBrowser         *self)
{
  gtk_widget_reparent (self->priv->inspector_view, self->priv->paned);
  inspector_hide_window (self);
  
  return TRUE;
}

static gboolean
on_inspector_window_delete_event (GtkWidget  *window,
                                  GdkEvent   *event,
                                  GwhBrowser *self)
{
  webkit_web_inspector_close (self->priv->inspector);
  
  return TRUE;
}

/* web view events hanlding */

static void
on_item_inspector_toggled (GtkToggleToolButton *button,
                           GwhBrowser          *self)
{
  if (gtk_toggle_tool_button_get_active (button)) {
    webkit_web_inspector_show (self->priv->inspector);
  } else {
    webkit_web_inspector_close (self->priv->inspector);
  }
}

static void
on_url_entry_activate (GtkEntry    *entry,
                       GwhBrowser  *self)
{
  gwh_browser_set_uri (self, gtk_entry_get_text (entry));
}

static void
update_load_status (GwhBrowser *self)
{
  gboolean        loading = FALSE;
  WebKitWebView  *web_view = WEBKIT_WEB_VIEW (self->priv->web_view);
  
  switch (webkit_web_view_get_load_status (web_view)) {
    case WEBKIT_LOAD_PROVISIONAL:
    case WEBKIT_LOAD_COMMITTED:
    case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
      loading = TRUE;
      break;
    
    case WEBKIT_LOAD_FINISHED:
    case WEBKIT_LOAD_FAILED:
      loading = FALSE;
      break;
  }
  
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_reload), ! loading);
  gtk_widget_set_visible   (GTK_WIDGET (self->priv->item_reload), ! loading);
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_cancel), loading);
  gtk_widget_set_visible   (GTK_WIDGET (self->priv->item_cancel), loading);
  
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_prev),
                            webkit_web_view_can_go_back (web_view));
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_next),
                            webkit_web_view_can_go_forward (web_view));
}

static void
on_web_view_load_status_notify (GObject    *object,
                                GParamSpec *pspec,
                                GwhBrowser *self)
{
  update_load_status (self);
}

static gboolean
on_web_view_load_error (WebKitWebView  *web_view,
                        WebKitWebFrame *web_frame,
                        gchar          *uri,
                        gpointer        web_error,
                        GwhBrowser     *self)
{
  update_load_status (self);
  
  return FALSE; /* we didn't really handled the error, so return %FALSE */
}

static void
on_web_view_uri_notify (GObject    *object,
                        GParamSpec *pspec,
                        GwhBrowser *self)
{
  const gchar *uri;
  
  uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (self->priv->web_view));
  gtk_entry_set_text (GTK_ENTRY (self->priv->url_entry), uri);
  g_object_set (self->priv->settings, "browser-last-uri", uri, NULL);
}

static void
on_web_view_icon_uri_notify (GObject    *object,
                             GParamSpec *pspec,
                             GwhBrowser *self)
{
  const gchar *icon_uri;
  
  icon_uri = webkit_web_view_get_icon_uri (WEBKIT_WEB_VIEW (self->priv->web_view));
  set_location_icon (self, icon_uri);
}

static void
on_web_view_progress_notify (GObject    *object,
                             GParamSpec *pspec,
                             GwhBrowser *self)
{
  gdouble value;
  
  value = webkit_web_view_get_progress (WEBKIT_WEB_VIEW (self->priv->web_view));
  if (value >= 1.0) {
    value = 0.0;
  }
  gtk_entry_set_progress_fraction (GTK_ENTRY (self->priv->url_entry), value);
}


static void
on_item_flip_orientation_activate (GtkMenuItem *item,
                                   GwhBrowser  *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  gtk_orientable_get_orientation (GTK_ORIENTABLE (self)) == GTK_ORIENTATION_VERTICAL
                                  ? GTK_ORIENTATION_HORIZONTAL
                                  : GTK_ORIENTATION_VERTICAL);
}

static void
on_item_zoom_100_activate (GtkMenuItem *item,
                           GwhBrowser  *self)
{
  webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (self->priv->web_view), 1.0);
}

static void
on_item_full_content_zoom_activate (GtkCheckMenuItem *item,
                                    GwhBrowser       *self)
{
  webkit_web_view_set_full_content_zoom (WEBKIT_WEB_VIEW (self->priv->web_view),
                                         gtk_check_menu_item_get_active (item));
}

static void
on_web_view_populate_popup (WebKitWebView *view,
                            GtkMenu       *menu,
                            GwhBrowser    *self)
{
  GtkWidget *item;
  GtkWidget *submenu;
  
  #define ADD_SEPARATOR(menu) \
    item = gtk_separator_menu_item_new (); \
    gtk_widget_show (item); \
    gtk_menu_append (menu, item)
  
  ADD_SEPARATOR (menu);
  
  /* Zoom menu */
  submenu = gtk_menu_new ();
  item = gtk_menu_item_new_with_mnemonic (_("_Zoom"));
  gtk_widget_show (item);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
  gtk_menu_append (menu, item);
  /* zoom in */
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ZOOM_IN, NULL);
  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (webkit_web_view_zoom_in), view);
  gtk_menu_append (GTK_MENU (submenu), item);
  /* zoom out */
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ZOOM_OUT, NULL);
  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (webkit_web_view_zoom_out), view);
  gtk_menu_append (GTK_MENU (submenu), item);
  /* zoom 1:1 */
  ADD_SEPARATOR (submenu);
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ZOOM_100, NULL);
  g_signal_connect (item, "activate",
                    G_CALLBACK (on_item_zoom_100_activate), self);
  gtk_menu_append (GTK_MENU (submenu), item);
  /* full content zoom */
  ADD_SEPARATOR (submenu);
  item = gtk_check_menu_item_new_with_mnemonic (_("Full-_content zoom"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
                                  webkit_web_view_get_full_content_zoom (view));
  g_signal_connect (item, "activate",
                    G_CALLBACK (on_item_full_content_zoom_activate), self);
  gtk_menu_append (GTK_MENU (submenu), item);
  /* show zoom sumbenu */
  gtk_widget_show_all (submenu);
  
  ADD_SEPARATOR (menu);
  
  item = gtk_menu_item_new_with_label (_("Flip panes orientation"));
  g_signal_connect (item, "activate",
                    G_CALLBACK (on_item_flip_orientation_activate), self);
  gtk_widget_show (item);
  gtk_menu_append (menu, item);
  if (! INSPECTOR_VISIBLE (self) || INSPECTOR_DETACHED (self)) {
    gtk_widget_set_sensitive (item, FALSE);
  }
  
  #undef ADD_SEPARATOR
  
  g_signal_emit (self, signals[POPULATE_POPUP], 0, menu);
}

static gboolean
on_web_view_scroll_event (GtkWidget      *widget,
                          GdkEventScroll *event,
                          GwhBrowser     *self)
{
  guint     mods = event->state & gtk_accelerator_get_default_mod_mask ();
  gboolean  handled = FALSE;
  
  if (mods == GDK_CONTROL_MASK) {
    handled = TRUE;
    switch (event->direction) {
      case GDK_SCROLL_DOWN:
        webkit_web_view_zoom_out (WEBKIT_WEB_VIEW (self->priv->web_view));
        break;
      
      case GDK_SCROLL_UP:
        webkit_web_view_zoom_in (WEBKIT_WEB_VIEW (self->priv->web_view));
        break;
      
      default:
        handled = FALSE;
    }
  }
  
  return handled;
}

static void
on_orientation_notify (GObject    *object,
                       GParamSpec *pspec,
                       GwhBrowser *self)
{
  g_object_set (G_OBJECT (self->priv->settings), "browser-orientation",
                gtk_orientable_get_orientation (GTK_ORIENTABLE (self)), NULL);
}

static void
gwh_browser_destroy (GtkObject *object)
{
  GwhBrowser *self = GWH_BROWSER (object);
  
  /* save the setting now because we can't really set it at the time it changed,
   * but it's not a problem, anyway probably nobody but us is interested by the
   * geometry of our inspector window. */
  g_object_set (self->priv->settings, "inspector-window-geometry",
                get_web_inspector_window_geometry (self), NULL);
  
  /* remove signal handlers that might get called during the destruction phase
   * but that rely on stuff that might already heave been destroyed */
  g_signal_handlers_disconnect_matched (self->priv->inspector,
                                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                        self);
  g_signal_handlers_disconnect_matched (self->priv->web_view,
                                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                        self);
  /* also destroy the window, since it has no parent that will tell it to die */
  gtk_widget_destroy (self->priv->inspector_window);
  
  GTK_OBJECT_CLASS (gwh_browser_parent_class)->destroy (object);
}

static void
gwh_browser_finalize (GObject *object)
{
  GwhBrowser *self = GWH_BROWSER (object);
  
  if (self->priv->default_icon) {
    g_object_unref (self->priv->default_icon);
  }
  g_object_unref (self->priv->settings);
  
  G_OBJECT_CLASS (gwh_browser_parent_class)->finalize (object);
}

static void
gwh_browser_constructed (GObject *object)
{
  GwhBrowser *self = GWH_BROWSER (object);
  
  if (G_OBJECT_CLASS (gwh_browser_parent_class)->constructed) {
    G_OBJECT_CLASS (gwh_browser_parent_class)->constructed (object);
  }
  
  /* a bit ugly, fake notifications */
  g_object_notify (G_OBJECT (self->priv->settings), "browser-last-uri");
  g_object_notify (G_OBJECT (self->priv->settings), "browser-orientation");
  g_object_notify (G_OBJECT (self->priv->settings), "inspector-window-geometry");
}

static void
gwh_browser_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  switch (prop_id) {
    case PROP_INSPECTOR_TRANSIENT_FOR:
      g_value_set_object (value,
                          gwh_browser_get_inspector_transient_for (GWH_BROWSER (object)));
      break;
    
    case PROP_URI:
      g_value_set_string (value, gwh_browser_get_uri (GWH_BROWSER (object)));
      break;
    
    case PROP_ORIENTATION:
      g_value_set_enum (value,
                        gtk_orientable_get_orientation (GTK_ORIENTABLE (GWH_BROWSER (object)->priv->paned)));
      break;
    
    case PROP_TOOLBAR:
      g_value_set_object (value, GWH_BROWSER (object)->priv->toolbar);
      break;
    
    case PROP_WEB_VIEW:
      g_value_set_object (value, GWH_BROWSER (object)->priv->web_view);
      break;
    
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gwh_browser_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (prop_id) {
    case PROP_INSPECTOR_TRANSIENT_FOR:
      gwh_browser_set_inspector_transient_for (GWH_BROWSER (object),
                                               g_value_get_object (value));
      break;
    
    case PROP_URI:
      gwh_browser_set_uri (GWH_BROWSER (object), g_value_get_string (value));
      break;
    
    case PROP_ORIENTATION:
      gtk_orientable_set_orientation (GTK_ORIENTABLE (GWH_BROWSER (object)->priv->paned),
                                      g_value_get_enum (value));
      break;
    
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gwh_browser_show_all (GtkWidget *widget)
{
  /* don't show_all() on this widget, it would show the cancel or reload
   * button */
  gtk_widget_show (widget);
}

static void
gwh_browser_class_init (GwhBrowserClass *klass)
{
  GObjectClass       *object_class    = G_OBJECT_CLASS (klass);
  GtkObjectClass     *gtkobject_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass     *widget_class    = GTK_WIDGET_CLASS (klass);
  
  object_class->finalize      = gwh_browser_finalize;
  object_class->constructed   = gwh_browser_constructed;
  object_class->get_property  = gwh_browser_get_property;
  object_class->set_property  = gwh_browser_set_property;
  
  gtkobject_class->destroy    = gwh_browser_destroy;
  
  widget_class->show_all      = gwh_browser_show_all;
  
  signals[POPULATE_POPUP] = g_signal_new ("populate-popup",
                                          G_OBJECT_CLASS_TYPE (object_class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (GwhBrowserClass, populate_popup),
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE, 1, GTK_TYPE_MENU);
  
  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");
  
  g_object_class_install_property (object_class, PROP_INSPECTOR_TRANSIENT_FOR,
                                   g_param_spec_object ("inspector-transient-for",
                                                        "Inspector transient for",
                                                        "The parent window of the inspector when detached.",
                                                        GTK_TYPE_WINDOW,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_URI,
                                   g_param_spec_string ("uri",
                                                        "URI",
                                                        "The browser's URI",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_WEB_VIEW,
                                   g_param_spec_object ("web-view",
                                                        "Web view",
                                                        "The browser's web view",
                                                        WEBKIT_TYPE_WEB_VIEW,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_TOOLBAR,
                                   g_param_spec_object ("toolbar",
                                                        "Toolbar",
                                                        "The browser's toolbar",
                                                        GTK_TYPE_TOOLBAR,
                                                        G_PARAM_READABLE));
  
  g_type_class_add_private (klass, sizeof (GwhBrowserPrivate));
}

static GtkWidget *
create_toolbar (GwhBrowser *self)
{
  GtkWidget   *toolbar;
  GtkToolItem *item;
  
  toolbar = g_object_new (GTK_TYPE_TOOLBAR,
                          "icon-size", GTK_ICON_SIZE_MENU,
                          "toolbar-style", GTK_TOOLBAR_ICONS,
                          NULL);
  
  self->priv->item_prev = gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
  gtk_tool_item_set_tooltip_text (self->priv->item_prev, _("Back"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_prev, -1);
  gtk_widget_show (GTK_WIDGET (self->priv->item_prev));
  self->priv->item_next = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
  gtk_tool_item_set_tooltip_text (self->priv->item_next, _("Forward"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_next, -1);
  gtk_widget_show (GTK_WIDGET (self->priv->item_next));
  self->priv->item_cancel = gtk_tool_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_tool_item_set_tooltip_text (self->priv->item_cancel, _("Cancel loading"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_cancel, -1);
  /* don't show cancel */
  self->priv->item_reload = gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH);
  gtk_tool_item_set_tooltip_text (self->priv->item_reload, _("Reload current page"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_reload, -1);
  gtk_widget_show (GTK_WIDGET (self->priv->item_reload));
  
  self->priv->url_entry = gtk_entry_new ();
  item = gtk_tool_item_new ();
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_container_add (GTK_CONTAINER (item), self->priv->url_entry);
  gtk_tool_item_set_expand (item, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  gtk_widget_show_all (GTK_WIDGET (item));
  
  self->priv->item_inspector = gtk_toggle_tool_button_new_from_stock (GTK_STOCK_INFO);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (self->priv->item_inspector), _("Web inspector"));
  gtk_tool_item_set_tooltip_text (self->priv->item_inspector, _("Toggle web inspector"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_inspector, -1);
  gtk_widget_show (GTK_WIDGET (self->priv->item_inspector));
  
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_prev), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_next), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_cancel), FALSE);
  
  set_location_icon (self, NULL);
  
  g_signal_connect_swapped (G_OBJECT (self->priv->item_prev), "clicked",
                            G_CALLBACK (webkit_web_view_go_back),
                            self->priv->web_view);
  g_signal_connect_swapped (G_OBJECT (self->priv->item_next), "clicked",
                            G_CALLBACK (webkit_web_view_go_forward),
                            self->priv->web_view);
  g_signal_connect_swapped (G_OBJECT (self->priv->item_cancel), "clicked",
                            G_CALLBACK (webkit_web_view_stop_loading),
                            self->priv->web_view);
  g_signal_connect_swapped (G_OBJECT (self->priv->item_reload), "clicked",
                            G_CALLBACK (webkit_web_view_reload),
                            self->priv->web_view);
  g_signal_connect (G_OBJECT (self->priv->item_inspector), "toggled",
                    G_CALLBACK (on_item_inspector_toggled), self);
  g_signal_connect (G_OBJECT (self->priv->url_entry), "activate",
                    G_CALLBACK (on_url_entry_activate), self);
  
  return toolbar;
}

static void
gwh_browser_init (GwhBrowser *self)
{
  GtkWidget *scrolled;
  WebKitWebSettings *wkws;
  
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GWH_TYPE_BROWSER,
                                            GwhBrowserPrivate);
  
  self->priv->default_icon = NULL;
  /* web view need to be created first because we use it in create_toolbar() */
  self->priv->web_view = webkit_web_view_new ();
  wkws = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (self->priv->web_view));
  g_object_set (wkws, "enable-developer-extras", TRUE, NULL);
  
  self->priv->settings = gwh_settings_get_default ();
  
  self->priv->toolbar = create_toolbar (self);
  self->priv->paned = gtk_vpaned_new ();
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolled), self->priv->web_view);
  
  gtk_box_pack_start (GTK_BOX (self), self->priv->toolbar, FALSE, TRUE, 0);
  gtk_widget_show (self->priv->toolbar);
  gtk_box_pack_start (GTK_BOX (self), self->priv->paned, TRUE, TRUE, 0);
  gtk_paned_pack1 (GTK_PANED (self->priv->paned), scrolled, TRUE, TRUE);
  gtk_widget_show_all (self->priv->paned);
  
  self->priv->inspector_view = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->priv->inspector_view),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  self->priv->inspector_web_view = webkit_web_view_new ();
  gtk_widget_show (self->priv->inspector_web_view);
  gtk_container_add (GTK_CONTAINER (self->priv->inspector_view),
                     self->priv->inspector_web_view);
  
  self->priv->inspector_window_x = self->priv->inspector_window_y = 0;
  self->priv->inspector_window = g_object_new (GTK_TYPE_WINDOW,
                                               "type", GTK_WINDOW_TOPLEVEL,
                                               "skip-taskbar-hint", TRUE,
                                               "title", _("Web inspector"),
                                               NULL);
  g_signal_connect (self->priv->inspector_window, "delete-event",
                    G_CALLBACK (on_inspector_window_delete_event), self);
  gtk_container_add (GTK_CONTAINER (self->priv->inspector_window),
                     self->priv->inspector_view);
  
  g_signal_connect (self, "notify::orientation",
                    G_CALLBACK (on_orientation_notify), self);
  
  self->priv->inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (self->priv->web_view));
  g_signal_connect (self->priv->inspector, "inspect-web-view",
                    G_CALLBACK (on_inspector_inspect_web_view), self);
  g_signal_connect (self->priv->inspector, "show-window",
                    G_CALLBACK (on_inspector_show_window), self);
  g_signal_connect (self->priv->inspector, "close-window",
                    G_CALLBACK (on_inspector_close_window), self);
  g_signal_connect (self->priv->inspector, "detach-window",
                    G_CALLBACK (on_inspector_detach_window), self);
  g_signal_connect (self->priv->inspector, "attach-window",
                    G_CALLBACK (on_inspector_attach_window), self);
  
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::load-status",
                    G_CALLBACK (on_web_view_load_status_notify), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::progress",
                    G_CALLBACK (on_web_view_progress_notify), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::uri",
                    G_CALLBACK (on_web_view_uri_notify), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::load-status",
                    G_CALLBACK (on_web_view_load_status_notify), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::load-error",
                    G_CALLBACK (on_web_view_load_error), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::icon-uri",
                    G_CALLBACK (on_web_view_icon_uri_notify), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "populate-popup",
                    G_CALLBACK (on_web_view_populate_popup), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "scroll-event",
                    G_CALLBACK (on_web_view_scroll_event), self);
  
  gtk_widget_grab_focus (self->priv->url_entry);
  
  g_signal_connect (self->priv->settings, "notify::browser-last-uri",
                    G_CALLBACK (on_settings_browser_last_uri_notify), self);
  g_signal_connect (self->priv->settings, "notify::browser-orientation",
                    G_CALLBACK (on_settings_browser_orientation_notify), self);
  g_signal_connect (self->priv->settings, "notify::inspector-window-geometry",
                    G_CALLBACK (on_settings_inspector_window_geometry_notify), self);
}


/*----------------------------- Begin public API -----------------------------*/

GtkWidget *
gwh_browser_new (void)
{
  return g_object_new (GWH_TYPE_BROWSER, NULL);
}

void
gwh_browser_set_uri (GwhBrowser  *self,
                     const gchar *uri)
{
  gchar *real_uri;
  gchar *scheme;
  
  g_return_if_fail (GWH_IS_BROWSER (self));
  g_return_if_fail (uri != NULL);
  
  real_uri = g_strdup (uri);
  scheme = g_uri_parse_scheme (real_uri);
  if (! scheme) {
    gchar *tmp;
    
    tmp = g_strconcat ("http://", uri, NULL);
    g_free (real_uri);
    real_uri = tmp;
  }
  g_free (scheme);
  if (g_strcmp0 (real_uri, gwh_browser_get_uri (self)) != 0) {
    webkit_web_view_open (WEBKIT_WEB_VIEW (self->priv->web_view), real_uri);
    g_object_notify (G_OBJECT (self), "uri");
  }
  g_free (real_uri);
}

const gchar *
gwh_browser_get_uri (GwhBrowser *self)
{
  g_return_val_if_fail (GWH_IS_BROWSER (self), NULL);
  
  return webkit_web_view_get_uri (WEBKIT_WEB_VIEW (self->priv->web_view));
}

GtkToolbar *
gwh_browser_get_toolbar (GwhBrowser *self)
{
  g_return_val_if_fail (GWH_IS_BROWSER (self), NULL);
  
  return GTK_TOOLBAR (self->priv->toolbar);
}

WebKitWebView *
gwh_browser_get_web_view (GwhBrowser *self)
{
  g_return_val_if_fail (GWH_IS_BROWSER (self), NULL);
  
  return WEBKIT_WEB_VIEW (self->priv->web_view);
}

void
gwh_browser_reload (GwhBrowser *self)
{
  g_return_if_fail (GWH_IS_BROWSER (self));
  
  webkit_web_view_reload (WEBKIT_WEB_VIEW (self->priv->web_view));
}

void
gwh_browser_set_inspector_transient_for (GwhBrowser *self,
                                            GtkWindow  *window)
{
  g_return_if_fail (GWH_IS_BROWSER (self));
  g_return_if_fail (window == NULL || GTK_IS_WINDOW (window));
  
  gtk_window_set_transient_for (GTK_WINDOW (self->priv->inspector_window),
                                window);
}

GtkWindow *
gwh_browser_get_inspector_transient_for (GwhBrowser *self)
{
  g_return_val_if_fail (GWH_IS_BROWSER (self), NULL);
  
  return gtk_window_get_transient_for (GTK_WINDOW (self->priv->inspector_window));
}
