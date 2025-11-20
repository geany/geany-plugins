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

#include "gwh-browser.h"

#include "config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <document.h>

#include "gwh-utils.h"
#include "gwh-settings.h"
#include "gwh-keybindings.h"
#include "gwh-plugin.h"


struct _GwhBrowserPrivate
{
  GwhSettings        *settings;
  
  GIcon        *default_icon;
  
  GtkWidget          *toolbar;
  GtkWidget          *web_view;
  WebKitWebInspector *inspector;
  
  GtkWidget    *url_entry;
  GtkWidget    *url_combo;
  GtkToolItem  *item_prev;
  GtkToolItem  *item_next;
  GtkToolItem  *item_cancel;
  GtkToolItem  *item_reload;
  GtkToolItem  *item_inspector;
  
  GtkWidget    *statusbar;
  gchar        *hovered_link;
};

enum {
  PROP_0,
  PROP_URI,
  PROP_WEB_VIEW,
  PROP_TOOLBAR
};

enum {
  POPULATE_POPUP,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};
static const gdouble zoom_in_factor = 1.2;
static const gdouble zoom_out_factor = 1.0 / 1.2;


G_DEFINE_TYPE_WITH_PRIVATE (GwhBrowser, gwh_browser, GTK_TYPE_BOX)


static void
set_location_icon (GwhBrowser  *self,
                   cairo_surface_t *icon_surface)
{
  gboolean success = FALSE;

  if (icon_surface) {
    GdkPixbuf *icon;

    icon = gdk_pixbuf_get_from_surface (
      icon_surface, 0, 0,
      cairo_image_surface_get_width (icon_surface),
      cairo_image_surface_get_height (icon_surface)
    );

    gtk_entry_set_icon_from_pixbuf (GTK_ENTRY (self->priv->url_entry),
                                    GTK_ENTRY_ICON_PRIMARY, icon);
    g_object_unref (icon);
    success = TRUE;
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
on_settings_browser_bookmarks_notify (GObject    *object,
                                      GParamSpec *pspec,
                                      GwhBrowser *self)
{
  gchar **bookmarks;
  
  g_return_if_fail (GWH_IS_BROWSER (self));
  
  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (self->priv->url_combo));
  bookmarks = gwh_browser_get_bookmarks (self);
  if (bookmarks) {
    gchar **p;
    
    for (p = bookmarks; *p; p++) {
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->priv->url_combo),
                                      *p);
    }
    g_strfreev (bookmarks);
  }
}

/* web inspector events handling */

#define INSPECTOR_VISIBLE(self) \
  (webkit_web_inspector_get_web_view ((self)->priv->inspector) != NULL)

static void
inspector_set_visible (GwhBrowser *self,
                       gboolean    visible)
{
  if (visible != INSPECTOR_VISIBLE (self)) {
    if (visible) {
      webkit_web_inspector_show (self->priv->inspector);
    } else {
      webkit_web_inspector_close (self->priv->inspector);
    }
  }
}

static gboolean
on_inspector_closed (WebKitWebInspector *inspector,
                     GwhBrowser         *self)
{
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (self->priv->item_inspector),
                                     FALSE);
  return FALSE;
}

static gboolean
on_inspector_opened (WebKitWebInspector *inspector,
                          GwhBrowser         *self)
{
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (self->priv->item_inspector),
                                     TRUE);
  return FALSE;
}

/* web view events hanlding */

static void
on_item_inspector_toggled (GtkToggleToolButton *button,
                           GwhBrowser          *self)
{
  inspector_set_visible (self, gtk_toggle_tool_button_get_active (button));
}

static void
on_url_entry_activate (GtkEntry    *entry,
                       GwhBrowser  *self)
{
  gwh_browser_set_uri (self, gtk_entry_get_text (entry));
}

static void
on_url_combo_active_notify (GtkComboBox  *combo,
                            GParamSpec   *pspec,
                            GwhBrowser   *self)
{
  if (gtk_combo_box_get_active (combo) != -1) {
    const gchar *uri = gtk_entry_get_text (GTK_ENTRY (self->priv->url_entry));
    
    gwh_browser_set_uri (self, uri);
  }
}

static void
on_item_bookmark_toggled (GtkCheckMenuItem *item,
                          GwhBrowser       *self)
{
  if (gtk_check_menu_item_get_active (item)) {
    gwh_browser_add_bookmark (self, gwh_browser_get_uri (self));
  } else {
    gwh_browser_remove_bookmark (self, gwh_browser_get_uri (self));
  }
}

static void
item_show_accelerator (GtkWidget *item,
                       gsize      key_id)
{
  GeanyKeyBinding *binding = keybindings_get_item (gwh_keybindings_get_group (),
                                                   key_id);

  if (binding->key) {
    /* we need an accel group for setting the accelerator, but we can't get
     * Geany's.  It doesn't matter though, as this is only for showing the
     * accelarator, not actually for tiggering the item. */
    GtkAccelGroup *dummy_accel_group = gtk_accel_group_new ();

    gtk_widget_add_accelerator (item, "activate", dummy_accel_group,
                                binding->key, binding->mods,
                                GTK_ACCEL_VISIBLE);
    g_object_set_data_full (G_OBJECT (item), "dummy_accel_group",
                            dummy_accel_group, g_object_unref);
  }
}

static void
on_url_entry_icon_press (GtkEntry            *entry,
                         GtkEntryIconPosition icon_pos,
                         GdkEventButton      *event,
                         GwhBrowser          *self)
{
  if (icon_pos == GTK_ENTRY_ICON_PRIMARY) {
    GtkWidget    *menu = gtk_menu_new ();
    GtkWidget    *item;
    const gchar  *uri = gwh_browser_get_uri (self);
    
    item = gtk_check_menu_item_new_with_mnemonic (_("Bookmark this website"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
                                    gwh_browser_has_bookmark (self, uri));
    g_signal_connect (item, "toggled",
                      G_CALLBACK (on_item_bookmark_toggled), self);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show (item);
    item_show_accelerator (item, GWH_KB_TOGGLE_BOOKMARK);
    
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                    event->button, event->time);
  }
}

static void
on_item_load_current_file_activate (GtkMenuItem  *item,
                                    GwhBrowser   *self)
{
  gwh_browser_set_uri_from_document (self, document_get_current ());
}

static void
on_url_entry_populate_popup (GtkEntry    *entry,
                             GtkWidget   *menu,
                             GwhBrowser  *self)
{
  GtkWidget *item;
  GeanyDocument *doc = document_get_current ();

  /* separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* load current file item */
  item = gtk_menu_item_new_with_mnemonic (_("_Load current file"));
  gtk_widget_set_sensitive (item, doc && doc->real_path);
  g_signal_connect (item, "activate",
                    G_CALLBACK (on_item_load_current_file_activate), self);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);
  item_show_accelerator (item, GWH_KB_LOAD_CURRENT_FILE);
}

static gboolean
on_entry_completion_match_selected (GtkEntryCompletion *comp,
                                    GtkTreeModel       *model,
                                    GtkTreeIter        *iter,
                                    GwhBrowser         *self)
{
  gint    column = gtk_entry_completion_get_text_column (comp);
  gchar  *row;
  
  gtk_tree_model_get (model, iter, column, &row, -1);
  /* set the entry value too in the unlikely case the selected URI is the
   * currently viewed one, in which case set_uri() won't change it */
  gtk_entry_set_text (GTK_ENTRY (self->priv->url_entry), row);
  gwh_browser_set_uri (self, row);
  g_free (row);
  
  return TRUE;
}

static void
update_history (GwhBrowser *self)
{
  WebKitWebView  *web_view = WEBKIT_WEB_VIEW (self->priv->web_view);
  
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_prev),
                            webkit_web_view_can_go_back (web_view));
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_next),
                            webkit_web_view_can_go_forward (web_view));
}

static void
update_load_status (GwhBrowser *self)
{
  gboolean        loading = FALSE;
  WebKitWebView  *web_view = WEBKIT_WEB_VIEW (self->priv->web_view);

  loading = webkit_web_view_is_loading (web_view);

  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_reload), ! loading);
  gtk_widget_set_visible   (GTK_WIDGET (self->priv->item_reload), ! loading);
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_cancel), loading);
  gtk_widget_set_visible   (GTK_WIDGET (self->priv->item_cancel), loading);
  
  update_history (self);
}

static void
on_web_view_load_changed (WebKitWebView   *object,
                          WebKitLoadEvent  load_event,
                          GwhBrowser      *self)
{
  update_load_status (self);
}

static gboolean
on_web_view_load_failed (WebKitWebView   *web_view,
                         WebKitLoadEvent  load_event,
                         gchar           *failing_uri,
                         GError          *error,
                         GwhBrowser      *self)
{
  update_load_status (self);
  
  return FALSE; /* we didn't really handle the error, so return %FALSE */
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
  update_history (self);
}

static void
on_web_view_favicon_notify (GObject    *object,
                            GParamSpec *pspec,
                            GwhBrowser *self)
{
  cairo_surface_t *icon_surface;
  
  icon_surface = webkit_web_view_get_favicon (WEBKIT_WEB_VIEW (self->priv->web_view));
  set_location_icon (self, icon_surface);
}

static void
on_web_view_estimated_load_progress_notify (GObject    *object,
                                            GParamSpec *pspec,
                                            GwhBrowser *self)
{
  gdouble value;
  
  value = webkit_web_view_get_estimated_load_progress (WEBKIT_WEB_VIEW (self->priv->web_view));
  if (value >= 1.0) {
    value = 0.0;
  }
  gtk_entry_set_progress_fraction (GTK_ENTRY (self->priv->url_entry), value);
}

static void
on_item_zoom_100_activate (WebKitWebView *view)
{
  webkit_web_view_set_zoom_level (view, 1.0);
}

static void
on_item_full_content_zoom_activate (GSimpleAction *action,
                                    GVariant      *dummy_parameter,
                                    GwhBrowser    *self)
{
  WebKitSettings *settings;
  gboolean        zoom_text_only;

  settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (self->priv->web_view));

  zoom_text_only = !webkit_settings_get_zoom_text_only (settings);
  webkit_settings_set_zoom_text_only (settings, zoom_text_only);
  g_simple_action_set_state (action, g_variant_new_boolean (!zoom_text_only));
}

static void web_view_zoom (WebKitWebView *view, gdouble factor)
{
  gdouble zoom_level = webkit_web_view_get_zoom_level (view);
  webkit_web_view_set_zoom_level (view, zoom_level * factor);
}

static void web_view_zoom_in (WebKitWebView *view)
{
  web_view_zoom (view, zoom_in_factor);
}

static void web_view_zoom_out (WebKitWebView *view)
{
  web_view_zoom (view, zoom_out_factor);
}

static gboolean
on_web_view_context_menu (WebKitWebView       *view,
                          WebKitContextMenu   *context_menu,
                          GdkEvent            *event,
                          WebKitHitTestResult *hit_test_result,
                          GwhBrowser          *self)
{
  WebKitContextMenuItem    *item;
  WebKitContextMenu        *submenu;
  GSimpleAction            *action;
  gboolean                  zoom_text_only;

  webkit_context_menu_append (context_menu,
                              webkit_context_menu_item_new_separator ());

  /* Zoom menu */
  submenu = webkit_context_menu_new ();
  item = webkit_context_menu_item_new_with_submenu (_("_Zoom"), submenu);
  webkit_context_menu_append (context_menu, item);

  /* zoom in */
  action = g_simple_action_new ("zoom-in", NULL);
  g_signal_connect_swapped (action, "activate",
                            G_CALLBACK (web_view_zoom_in), view);
  item = webkit_context_menu_item_new_from_gaction (G_ACTION (action),
                                                    _("Zoom _In"), NULL);
  webkit_context_menu_append (submenu, item);
  g_object_unref (action);

  /* zoom out */
  action = g_simple_action_new ("zoom-out", NULL);
  g_signal_connect_swapped (action, "activate",
                            G_CALLBACK (web_view_zoom_out), view);
  item = webkit_context_menu_item_new_from_gaction (G_ACTION (action),
                                                    _("Zoom _Out"), NULL);
  webkit_context_menu_append (submenu, item);
  g_object_unref (action);

  /* zoom 1:1 */
  webkit_context_menu_append (submenu,
                              webkit_context_menu_item_new_separator ());
  action = g_simple_action_new ("zoom-reset", NULL);
  g_signal_connect_swapped (action, "activate",
                            G_CALLBACK (on_item_zoom_100_activate), view);
  item = webkit_context_menu_item_new_from_gaction (G_ACTION (action),
                                                    _("_Reset Zoom"), NULL);
  webkit_context_menu_append (submenu, item);
  g_object_unref (action);

  /* full content zoom */
  webkit_context_menu_append (submenu,
                              webkit_context_menu_item_new_separator ());
  zoom_text_only = webkit_settings_get_zoom_text_only (webkit_web_view_get_settings (view));
  action = g_simple_action_new_stateful ("full-content-zoom", NULL,
                                         g_variant_new_boolean (!zoom_text_only));
  g_signal_connect (action, "activate",
                    G_CALLBACK (on_item_full_content_zoom_activate), self);
  item = webkit_context_menu_item_new_from_gaction (G_ACTION (action),
                                                    _("Full-_content zoom"),
                                                    NULL);
  webkit_context_menu_append (submenu, item);
  g_object_unref (action);

  g_signal_emit (self, signals[POPULATE_POPUP], 0, context_menu);

  return FALSE;
}

static gboolean
on_web_view_scroll_event (GtkWidget      *widget,
                          GdkEventScroll *event,
                          GwhBrowser     *self)
{
  guint     mods = event->state & gtk_accelerator_get_default_mod_mask ();
  gboolean  handled = FALSE;
  gdouble   delta;
  gdouble   factor;
  
  if (mods == GDK_CONTROL_MASK) {
    handled = TRUE;
    switch (event->direction) {
      case GDK_SCROLL_DOWN:
        web_view_zoom_out (WEBKIT_WEB_VIEW (self->priv->web_view));
        break;
      
      case GDK_SCROLL_UP:
        web_view_zoom_in (WEBKIT_WEB_VIEW (self->priv->web_view));
        break;

      case GDK_SCROLL_SMOOTH:
        delta = event->delta_x + event->delta_y;
        factor = pow (delta < 0 ? zoom_in_factor : zoom_out_factor,
                      fabs (delta));
        web_view_zoom (WEBKIT_WEB_VIEW (self->priv->web_view), factor);
        break;

      default:
        handled = FALSE;
    }
  }
  
  return handled;
}

static void
gwh_browser_destroy (GtkWidget *widget)
{
  GwhBrowser *self = GWH_BROWSER (widget);
  
  /* remove signal handlers that might get called during the destruction phase
   * but that rely on stuff that might already heave been destroyed */
  g_signal_handlers_disconnect_matched (self->priv->inspector,
                                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                        self);
  g_signal_handlers_disconnect_matched (self->priv->web_view,
                                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                        self);
  g_signal_handlers_disconnect_matched (self->priv->settings,
                                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                        self);
  
  GTK_WIDGET_CLASS (gwh_browser_parent_class)->destroy (widget);
}

static void
gwh_browser_finalize (GObject *object)
{
  GwhBrowser *self = GWH_BROWSER (object);
  
  if (self->priv->default_icon) {
    g_object_unref (self->priv->default_icon);
  }
  g_object_unref (self->priv->settings);
  g_object_unref (self->priv->statusbar);
  g_free (self->priv->hovered_link);
  
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
  g_object_notify (G_OBJECT (self->priv->settings), "browser-bookmarks");
}

static void
gwh_browser_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  switch (prop_id) {
    case PROP_URI:
      g_value_set_string (value, gwh_browser_get_uri (GWH_BROWSER (object)));
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
    case PROP_URI:
      gwh_browser_set_uri (GWH_BROWSER (object), g_value_get_string (value));
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
  GtkWidgetClass     *widget_class    = GTK_WIDGET_CLASS (klass);
  
  object_class->finalize      = gwh_browser_finalize;
  object_class->constructed   = gwh_browser_constructed;
  object_class->get_property  = gwh_browser_get_property;
  object_class->set_property  = gwh_browser_set_property;
  
  widget_class->destroy       = gwh_browser_destroy;
  widget_class->show_all      = gwh_browser_show_all;
  
  signals[POPULATE_POPUP] = g_signal_new ("populate-popup",
                                          G_OBJECT_CLASS_TYPE (object_class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (GwhBrowserClass, populate_popup),
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE, 1, WEBKIT_TYPE_CONTEXT_MENU);
  
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
}

/* a GtkEntryCompletionMatchFunc matching anywhere in the haystack */
static gboolean
url_completion_match_func (GtkEntryCompletion  *comp,
                           const gchar         *key,
                           GtkTreeIter         *iter,
                           gpointer             dummy)
{
  GtkTreeModel *model   = gtk_entry_completion_get_model (comp);
  gint          column  = gtk_entry_completion_get_text_column (comp);
  gchar        *row     = NULL;
  gboolean      match   = FALSE;
  
  gtk_tree_model_get (model, iter, column, &row, -1);
  if (row) {
    SETPTR (row, g_utf8_normalize (row, -1, G_NORMALIZE_DEFAULT));
    SETPTR (row, g_utf8_casefold (row, -1));
    match = strstr (row, key) != NULL;
    g_free (row);
  }
  
  return match;
}

static GtkToolItem *
tool_button_new (const gchar *mnemonic,
                 const gchar *icon_name,
                 const gchar *tooltip)
{
  return g_object_new (GTK_TYPE_TOOL_BUTTON,
                       "use-underline", TRUE,
                       "label", mnemonic,
                       "icon-name", icon_name,
                       "tooltip-text", tooltip,
                       NULL);
}

static GtkWidget *
create_toolbar (GwhBrowser *self)
{
  GtkWidget          *toolbar;
  GtkToolItem        *item;
  GtkEntryCompletion *comp;
  
  toolbar = g_object_new (GTK_TYPE_TOOLBAR,
                          "icon-size", GTK_ICON_SIZE_MENU,
                          "toolbar-style", GTK_TOOLBAR_ICONS,
                          NULL);
  
  self->priv->item_prev = tool_button_new (_("_Back"), "go-previous",
                                           _("Go back"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_prev, -1);
  gtk_widget_show (GTK_WIDGET (self->priv->item_prev));
  self->priv->item_next = tool_button_new (_("_Forward"), "go-next",
                                           _("Go forward"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_next, -1);
  gtk_widget_show (GTK_WIDGET (self->priv->item_next));
  self->priv->item_cancel = tool_button_new (_("_Cancel"), "process-stop",
                                             _("Cancel loading"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_cancel, -1);
  /* don't show cancel */
  self->priv->item_reload = tool_button_new (_("_Refresh"), "view-refresh",
                                             _("Reload current page"));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_reload, -1);
  gtk_widget_show (GTK_WIDGET (self->priv->item_reload));
  
  self->priv->url_combo = gtk_combo_box_text_new_with_entry ();
  item = gtk_tool_item_new ();
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_container_add (GTK_CONTAINER (item), self->priv->url_combo);
  gtk_tool_item_set_expand (item, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  gtk_widget_show_all (GTK_WIDGET (item));
  
  self->priv->url_entry = gtk_bin_get_child (GTK_BIN (self->priv->url_combo));
  set_location_icon (self, NULL);
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (self->priv->url_entry),
                                   GTK_ENTRY_ICON_PRIMARY,
                                   _("Website information and settings"));
  
  comp = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (comp, gtk_combo_box_get_model (GTK_COMBO_BOX (self->priv->url_combo)));
  gtk_entry_completion_set_text_column (comp, gtk_combo_box_get_entry_text_column (GTK_COMBO_BOX (self->priv->url_combo)));
  gtk_entry_completion_set_match_func (comp, url_completion_match_func, NULL, NULL);
  gtk_entry_set_completion (GTK_ENTRY (self->priv->url_entry), comp);
  
  self->priv->item_inspector = g_object_new (GTK_TYPE_TOGGLE_TOOL_BUTTON,
                                             "use-underline", TRUE,
                                             "label", _("Web _Inspector"),
                                             "tooltip-text", _("Toggle web inspector"),
                                             "icon-name", "dialog-information",
                                             NULL);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), self->priv->item_inspector, -1);
  gtk_widget_show (GTK_WIDGET (self->priv->item_inspector));
  
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_prev), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_next), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->item_cancel), FALSE);
  
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
  g_signal_connect (G_OBJECT (self->priv->url_entry), "icon-press",
                    G_CALLBACK (on_url_entry_icon_press), self);
  g_signal_connect (G_OBJECT (self->priv->url_entry), "populate-popup",
                    G_CALLBACK (on_url_entry_populate_popup), self);
  g_signal_connect (G_OBJECT (self->priv->url_combo), "notify::active",
                    G_CALLBACK (on_url_combo_active_notify), self);
  g_signal_connect (G_OBJECT (comp), "match-selected",
                    G_CALLBACK (on_entry_completion_match_selected), self);
  
  return toolbar;
}

static guint
get_statusbar_context_id (GtkStatusbar *statusbar)
{
  static guint id = 0;
  
  if (id == 0) {
    id = gtk_statusbar_get_context_id (statusbar, "gwh-browser-hovered-link");
  }
  
  return id;
}

static void
on_web_view_mouse_target_changed (WebKitWebView       *view,
                                  WebKitHitTestResult *hit_test_result,
                                  guint                modifiers,
                                  GwhBrowser          *self)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR (self->priv->statusbar);
  const gchar *uri;

  if (self->priv->hovered_link) {
    gtk_statusbar_pop (statusbar, get_statusbar_context_id (statusbar));
    g_free (self->priv->hovered_link);
    self->priv->hovered_link = NULL;
  }

  if (!webkit_hit_test_result_context_is_link (hit_test_result))
    return;

  uri = webkit_hit_test_result_get_link_uri (hit_test_result);
  if (uri && *uri) {
    self->priv->hovered_link = g_strdup (uri);
    gtk_statusbar_push (statusbar, get_statusbar_context_id (statusbar),
                        self->priv->hovered_link);
  }
}

static void
on_web_view_leave_notify_event (GtkWidget        *widget,
                                GdkEventCrossing *event,
                                GwhBrowser       *self)
{
  if (self->priv->hovered_link) {
    GtkStatusbar *statusbar = GTK_STATUSBAR (self->priv->statusbar);
    
    gtk_statusbar_pop (statusbar, get_statusbar_context_id (statusbar));
  }
}

static void
on_web_view_enter_notify_event (GtkWidget        *widget,
                                GdkEventCrossing *event,
                                GwhBrowser       *self)
{
  if (self->priv->hovered_link) {
    GtkStatusbar *statusbar = GTK_STATUSBAR (self->priv->statusbar);
    
    gtk_statusbar_push (statusbar, get_statusbar_context_id (statusbar),
                        self->priv->hovered_link);
  }
}

static void
on_web_view_realize (GtkWidget  *widget,
                     GwhBrowser *self)
{
  gtk_widget_add_events (widget, GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK);
}

static void
gwh_browser_init (GwhBrowser *self)
{
  GtkWidget          *scrolled;
  WebKitSettings     *wkws;
  WebKitWebContext   *wkcontext;
  gboolean            inspector_detached;
  
  self->priv = gwh_browser_get_instance_private (self);
  
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  
  self->priv->default_icon = NULL;
  /* web view need to be created first because we use it in create_toolbar() */
  self->priv->web_view = webkit_web_view_new ();
  wkws = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (self->priv->web_view));
  g_object_set (wkws, "enable-developer-extras", TRUE, NULL);

  wkcontext = webkit_web_view_get_context (WEBKIT_WEB_VIEW (self->priv->web_view));
  webkit_web_context_set_favicon_database_directory (wkcontext, NULL);
  
  self->priv->settings = gwh_settings_get_default ();
  g_object_get (self->priv->settings,
                "inspector-detached", &inspector_detached,
                NULL);
  
  self->priv->toolbar = create_toolbar (self);
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolled), self->priv->web_view);
  
  gtk_box_pack_start (GTK_BOX (self), self->priv->toolbar, FALSE, TRUE, 0);
  gtk_widget_show (self->priv->toolbar);
  gtk_box_pack_start (GTK_BOX (self), scrolled, TRUE, TRUE, 0);
  gtk_widget_show_all (scrolled);
  
  self->priv->statusbar = ui_lookup_widget (geany_plugin->geany_data->main_widgets->window, "statusbar");
  if (self->priv->statusbar) {
    g_object_ref (self->priv->statusbar);
  } else {
    /* in the unlikely case we can't get the Geany statusbar, fake one */
    self->priv->statusbar = gtk_statusbar_new ();
  }
  self->priv->hovered_link = NULL;
  
  self->priv->inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (self->priv->web_view));
  g_signal_connect (self->priv->inspector, "bring-to-front",
                    G_CALLBACK (on_inspector_opened), self);
  g_signal_connect (self->priv->inspector, "closed",
                    G_CALLBACK (on_inspector_closed), self);
  
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::estimated-load-progress",
                    G_CALLBACK (on_web_view_estimated_load_progress_notify), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::uri",
                    G_CALLBACK (on_web_view_uri_notify), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "load-changed",
                    G_CALLBACK (on_web_view_load_changed), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "load-failed",
                    G_CALLBACK (on_web_view_load_failed), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "notify::favicon",
                    G_CALLBACK (on_web_view_favicon_notify), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "context-menu",
                    G_CALLBACK (on_web_view_context_menu), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "scroll-event",
                    G_CALLBACK (on_web_view_scroll_event), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "mouse-target-changed",
                    G_CALLBACK (on_web_view_mouse_target_changed), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "leave-notify-event",
                    G_CALLBACK (on_web_view_leave_notify_event), self);
  g_signal_connect (G_OBJECT (self->priv->web_view), "enter-notify-event",
                    G_CALLBACK (on_web_view_enter_notify_event), self);
  g_signal_connect_after (self->priv->web_view, "realize",
                          G_CALLBACK (on_web_view_realize), self);
  
  g_signal_connect (self->priv->web_view, "key-press-event",
                    G_CALLBACK (gwh_keybindings_handle_event), self);
  
  gtk_widget_grab_focus (self->priv->url_entry);
  
  g_signal_connect (self->priv->settings, "notify::browser-last-uri",
                    G_CALLBACK (on_settings_browser_last_uri_notify), self);
  g_signal_connect (self->priv->settings, "notify::browser-bookmarks",
                    G_CALLBACK (on_settings_browser_bookmarks_notify), self);
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
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (self->priv->web_view), real_uri);
    g_object_notify (G_OBJECT (self), "uri");
  }
  g_free (real_uri);
}

gboolean
gwh_browser_set_uri_from_document (GwhBrowser    *self,
                                   GeanyDocument *doc)
{
  gchar *uri;

  /* document must exist on disk */
  if (! doc || ! doc->real_path)
    return FALSE;

  uri = g_strconcat ("file://", doc->file_name, NULL);
  gwh_browser_set_uri (self, uri);
  g_free (uri);

  return TRUE;
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
gwh_browser_toggle_inspector (GwhBrowser *self)
{
  g_return_if_fail (GWH_IS_BROWSER (self));
  
  inspector_set_visible (self, ! INSPECTOR_VISIBLE (self));
}

gchar **
gwh_browser_get_bookmarks (GwhBrowser *self)
{
  gchar **bookmarks = NULL;
  
  g_return_val_if_fail (GWH_IS_BROWSER (self), NULL);
  
  g_object_get (self->priv->settings, "browser-bookmarks", &bookmarks, NULL);
  
  return bookmarks;
}

static void
gwh_browser_set_bookmarks (GwhBrowser  *self,
                           gchar      **bookmarks)
{
  g_object_set (self->priv->settings, "browser-bookmarks", bookmarks, NULL);
}

static gint
strv_index (gchar       **strv,
            const gchar  *str)
{
  g_return_val_if_fail (str != NULL, -1);
  
  if (strv) {
    gint idx;
    
    for (idx = 0; *strv; strv++, idx++) {
      if (strcmp (str, *strv) == 0) {
        return idx;
      }
    }
  }
  
  return -1;
}

gboolean
gwh_browser_has_bookmark (GwhBrowser   *self,
                          const gchar  *uri)
{
  gchar   **bookmarks = NULL;
  gboolean  exists    = FALSE;
  
  g_return_val_if_fail (GWH_IS_BROWSER (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  
  bookmarks = gwh_browser_get_bookmarks (self);
  exists = strv_index (bookmarks, uri) >= 0;
  g_strfreev (bookmarks);
  
  return exists;
}

static const gchar *
uri_skip_scheme (const gchar *uri)
{
  if (g_ascii_isalpha (*uri)) {
    do {
      uri++;
    } while (*uri == '+' || *uri == '-' || *uri == '.' ||
             g_ascii_isalnum (*uri));
    /* this is not strictly correct but good enough for what we do */
    while (*uri == ':' || *uri == '/')
      uri++;
  }
  
  return uri;
}

static int
sort_uris (gconstpointer a,
           gconstpointer b)
{
  const gchar *uri1 = uri_skip_scheme (*(const gchar *const *) a);
  const gchar *uri2 = uri_skip_scheme (*(const gchar *const *) b);
  
  return g_ascii_strcasecmp (uri1, uri2);
}

void
gwh_browser_add_bookmark (GwhBrowser   *self,
                          const gchar  *uri)
{
  gchar **bookmarks = NULL;
  
  g_return_if_fail (GWH_IS_BROWSER (self));
  g_return_if_fail (uri != NULL);
  
  bookmarks = gwh_browser_get_bookmarks (self);
  if (strv_index (bookmarks, uri) < 0) {
    gsize length = bookmarks ? g_strv_length (bookmarks) : 0;
    
    bookmarks = g_realloc (bookmarks, (length + 2) * sizeof *bookmarks);
    bookmarks[length] = g_strdup (uri);
    bookmarks[length + 1] = NULL;
    /* it would be faster to insert directly at the right place but who cares */
    qsort (bookmarks, length + 1, sizeof *bookmarks, sort_uris);
    gwh_browser_set_bookmarks (self, bookmarks);
  }
  g_strfreev (bookmarks);
}

void
gwh_browser_remove_bookmark (GwhBrowser  *self,
                             const gchar *uri)
{
  gchar **bookmarks = NULL;
  gint    idx;
  
  g_return_if_fail (GWH_IS_BROWSER (self));
  g_return_if_fail (uri != NULL);
  
  bookmarks = gwh_browser_get_bookmarks (self);
  idx = strv_index (bookmarks, uri);
  if (idx >= 0) {
    gsize length = g_strv_length (bookmarks);
    
    memmove (&bookmarks[idx], &bookmarks[idx + 1],
             (length - (gsize) idx) * sizeof *bookmarks);
    gwh_browser_set_bookmarks (self, bookmarks);
  }
  g_strfreev (bookmarks);
}
