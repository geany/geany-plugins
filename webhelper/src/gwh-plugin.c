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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>
#include <geany.h>
#include <document.h>

#include "gwh-utils.h"
#include "gwh-browser.h"
#include "gwh-settings.h"
#include "gwh-plugin.h"
#include "gwh-keybindings.h"
#include "gwh-enum-types.h"


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;


PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Web helper"),
  _("Display a preview web page that gets updated upon document saving and "
    "provide web analysis and debugging tools (aka Web Inspector), all using "
    "WebKit."),
  GWH_PLUGIN_VERSION,
  "Colomban Wendling <ban@herbesfolles.org>"
)


enum {
  CONTAINER_NOTEBOOK,
  CONTAINER_WINDOW
};

static GtkWidget   *G_browser   = NULL;
static struct {
  guint       type;
  GtkWidget  *widget;
  
  /* only valid if type == CONTAINER_WINDOW */
  gboolean    visible;
} G_container;
static GwhSettings *G_settings  = NULL;


static void
separate_window_set_visible (gboolean visible)
{
  if (visible != G_container.visible) {
    gchar *geometry;
    
    G_container.visible = visible;
    if (visible) {
      gtk_widget_show (G_container.widget);
      g_object_get (G_settings, "browser-separate-window-geometry", &geometry, NULL);
      gwh_set_window_geometry (GTK_WINDOW (G_container.widget), geometry, NULL, NULL);
      g_free (geometry);
    } else {
      geometry = gwh_get_window_geometry (GTK_WINDOW (G_container.widget), 0, 0);
      g_object_set (G_settings, "browser-separate-window-geometry", geometry, NULL);
      g_free (geometry);
      gtk_widget_hide (G_container.widget);
    }
  }
}

static gboolean
on_separate_window_delete_event (GtkWidget  *widget,
                                 GdkEvent   *event,
                                 gpointer    data)
{
  /* never honor delete-event */
  return TRUE;
}

static void
on_separate_window_destroy (GtkWidget  *widget,
                            gpointer    data)
{
  gtk_container_remove (GTK_CONTAINER (G_container.widget), G_browser);
}

static gboolean
on_idle_widget_show (gpointer data)
{
  separate_window_set_visible (TRUE);
  /* present back the Geany's window because it is very unlikely the user
   * expects the focus on our newly created window at this point, since we
   * either just loaded the plugin or activated a element from Geany's UI */
  gtk_window_present (GTK_WINDOW (geany_data->main_widgets->window));
  
  return FALSE;
}

static GtkWidget *
create_separate_window (void)
{
  GtkWidget  *window;
  gboolean    skips_taskbar;
  gboolean    is_transient;
  gint        window_type;
  
  g_object_get (G_settings,
                "wm-secondary-windows-skip-taskbar", &skips_taskbar,
                "wm-secondary-windows-are-transient", &is_transient,
                "wm-secondary-windows-type", &window_type,
                NULL);
  window = g_object_new (GTK_TYPE_WINDOW,
                         "type", GTK_WINDOW_TOPLEVEL,
                         "skip-taskbar-hint", skips_taskbar,
                         "title", _("Web view"),
                         "deletable", FALSE,
                         "type-hint", window_type,
                         NULL);
  g_signal_connect (window, "delete-event",
                    G_CALLBACK (on_separate_window_delete_event), NULL);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (on_separate_window_destroy), NULL);
  gtk_container_add (GTK_CONTAINER (window), G_browser);
  if (is_transient) {
    gtk_window_set_transient_for (GTK_WINDOW (window),
                                  GTK_WINDOW (geany_data->main_widgets->window));
  } else {
    GList *icons;
    
    icons = gtk_window_get_icon_list (GTK_WINDOW (geany_data->main_widgets->window));
    gtk_window_set_icon_list (GTK_WINDOW (window), icons);
    g_list_free (icons);
  }
  
  return window;
}

static void
attach_browser (void)
{
  GwhBrowserPosition position;
  
  g_object_get (G_settings, "browser-position", &position, NULL);
  if (position == GWH_BROWSER_POSITION_SEPARATE_WINDOW) {
    G_container.type = CONTAINER_WINDOW;
    G_container.widget = create_separate_window ();
    /* seems that if a window is shown before it's transient parent, bad stuff
     * happend. so, show our window a little later. */
    g_idle_add (on_idle_widget_show, G_container.widget);
  } else {
    G_container.type = CONTAINER_NOTEBOOK;
    if (position == GWH_BROWSER_POSITION_SIDEBAR) {
      G_container.widget = geany_data->main_widgets->sidebar_notebook;
    } else {
      G_container.widget = geany_data->main_widgets->message_window_notebook;
    }
    gtk_notebook_append_page (GTK_NOTEBOOK (G_container.widget),
                              G_browser, gtk_label_new (_("Web preview")));
  }
}

static void
detach_browser (void)
{
  if (G_container.type == CONTAINER_WINDOW) {
    separate_window_set_visible (FALSE); /* saves the geometry */
    gtk_widget_destroy (G_container.widget);
  } else {
    gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (G_browser)),
                          G_browser);
  }
}

static void
on_settings_browser_position_notify (GObject     *object,
                                     GParamSpec  *pspec,
                                     gpointer     data)
{
  g_object_ref (G_browser);
  detach_browser ();
  attach_browser ();
  g_object_unref (G_browser);
}

static void
on_settings_windows_attrs_notify (GObject    *object,
                                  GParamSpec *pspec,
                                  gpointer    data)
{
  /* recreate the window to apply the new attributes */
  if (G_container.type == CONTAINER_WINDOW) {
    g_object_ref (G_browser);
    detach_browser ();
    attach_browser ();
    g_object_unref (G_browser);
  }
}

static void
on_document_save (GObject        *obj,
                  GeanyDocument  *doc,
                  gpointer        user_data)
{
  gboolean auto_reload = FALSE;
  
  g_object_get (G_OBJECT (G_settings), "browser-auto-reload", &auto_reload,
                NULL);
  if (auto_reload) {
    gwh_browser_reload (GWH_BROWSER (G_browser));
  }
}

static void
on_item_auto_reload_toggled (GAction  *action,
                             GVariant *parameter,
                             gpointer  dummy)
{
  gboolean browser_auto_reload;

  g_object_get (G_OBJECT (G_settings),
                "browser-auto-reload", &browser_auto_reload, NULL);
  g_object_set (G_OBJECT (G_settings), "browser-auto-reload",
                !browser_auto_reload, NULL);
  g_simple_action_set_state (G_SIMPLE_ACTION (action),
                             g_variant_new_boolean (!browser_auto_reload));
}

static void
on_browser_populate_popup (GwhBrowser        *browser,
                           WebKitContextMenu *menu,
                           gpointer           dummy)
{
  GSimpleAction         *action;
  gboolean               auto_reload = FALSE;
  WebKitContextMenuItem *item;

  webkit_context_menu_append (menu,
                              webkit_context_menu_item_new_separator ());

  g_object_get (G_OBJECT (G_settings), "browser-auto-reload", &auto_reload,
                NULL);
  action = g_simple_action_new_stateful ("browser-auto-reload", NULL,
                                         g_variant_new_boolean (auto_reload));
  g_signal_connect (action, "activate",
                    G_CALLBACK (on_item_auto_reload_toggled), NULL);
  item = webkit_context_menu_item_new_from_gaction (G_ACTION (action),
                                                    _("Reload upon document saving"),
                                                    NULL);
  webkit_context_menu_append (menu, item);
  g_object_unref (action);
}

static void
on_kb_toggle_inspector (guint key_id)
{
  gwh_browser_toggle_inspector (GWH_BROWSER (G_browser));
}

static void
on_kb_show_hide_separate_window (guint key_id)
{
  if (G_container.type == CONTAINER_WINDOW) {
    separate_window_set_visible (! G_container.visible);
  }
}

static void
on_kb_toggle_bookmark (guint key_id)
{
  const gchar *uri = gwh_browser_get_uri (GWH_BROWSER (G_browser));
  
  if (gwh_browser_has_bookmark (GWH_BROWSER (G_browser), uri)) {
    gwh_browser_remove_bookmark (GWH_BROWSER (G_browser), uri);
  } else {
    gwh_browser_add_bookmark (GWH_BROWSER (G_browser), uri);
  }
}

static void
on_kb_load_current_file (guint key_id)
{
  gwh_browser_set_uri_from_document (GWH_BROWSER (G_browser),
                                     document_get_current ());
}


static gchar *
get_config_filename (void)
{
  return g_build_filename (geany_data->app->configdir, "plugins",
                           GWH_PLUGIN_TARNAME, GWH_PLUGIN_TARNAME".conf", NULL);
}

static void
load_config (void)
{
  gchar  *path;
  GError *err = NULL;
  
  G_settings = gwh_settings_get_default ();
  
  gwh_settings_install_property (G_settings, g_param_spec_boolean (
    "browser-auto-reload",
    _("Browser auto reload"),
    _("Whether the browser reloads itself upon document saving"),
    TRUE,
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_string (
    "browser-last-uri",
    _("Browser last URI"),
    _("Last URI visited by the browser"),
    "about:blank",
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_boxed (
    "browser-bookmarks",
    _("Bookmarks"),
    _("List of bookmarks"),
    G_TYPE_STRV,
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_enum (
    "browser-orientation",
    _("Browser orientation"),
    _("Orientation of the browser widget"),
    GTK_TYPE_ORIENTATION,
    GTK_ORIENTATION_VERTICAL,
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_enum (
    "browser-position",
    _("Browser position"),
    _("Position of the browser widget in Geany's UI"),
    GWH_TYPE_BROWSER_POSITION,
    GWH_BROWSER_POSITION_MESSAGE_WINDOW,
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_string (
    "browser-separate-window-geometry",
    _("Browser separate window geometry"),
    _("Last geometry of the separated browser's window"),
    "400x300",
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_boolean (
    "inspector-detached",
    _("Inspector detached"),
    _("Whether the inspector is in a separate window or docked in the browser"),
    FALSE,
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_boolean (
    "wm-secondary-windows-skip-taskbar",
    _("Secondary windows skip task bar"),
    _("Whether to tell the window manager not to show the secondary windows in the task bar"),
    TRUE,
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_boolean (
    "wm-secondary-windows-are-transient",
    _("Secondary windows are transient"),
    _("Whether secondary windows are transient children of their parent"),
    TRUE,
    G_PARAM_READWRITE));
  gwh_settings_install_property (G_settings, g_param_spec_enum (
    "wm-secondary-windows-type",
    _("Secondary windows type"),
    _("The type of the secondary windows"),
    GWH_TYPE_WINDOW_TYPE,
    GWH_WINDOW_TYPE_NORMAL,
    G_PARAM_READWRITE));
  
  path = get_config_filename ();
  if (! gwh_settings_load_from_file (G_settings, path, &err)) {
    g_warning ("Failed to load configuration: %s", err->message);
    g_error_free (err);
  }
  g_free (path);
}

static void
save_config (void)
{
  gchar  *path;
  gchar  *dirname;
  GError *err = NULL;
  
  path = get_config_filename ();
  dirname = g_path_get_dirname (path);
  utils_mkdir (dirname, TRUE);
  g_free (dirname);
  if (! gwh_settings_save_to_file (G_settings, path, &err)) {
    g_warning ("Failed to save configuration: %s", err->message);
    g_error_free (err);
  }
  g_free (path);
  g_object_unref (G_settings);
  G_settings = NULL;
}

void
plugin_init (GeanyData *data)
{
  /* even though it's not really a good idea to keep all the library we load
   * into memory, this is needed for webkit. first, without this we creash after
   * module unloading, and webkitgtk inserts static data into the GLib
   * (g_quark_from_static_string() for example) so it's not safe to remove it */
  plugin_module_make_resident (geany_plugin);
  
  load_config ();
  gwh_keybindings_init ();
  
  G_browser = gwh_browser_new ();
  g_signal_connect (G_browser, "populate-popup",
                    G_CALLBACK (on_browser_populate_popup), NULL);
  
  attach_browser ();
  gtk_widget_show_all (G_browser);
  
  plugin_signal_connect (geany_plugin, G_OBJECT (G_settings),
                         "notify::browser-position", FALSE,
                         G_CALLBACK (on_settings_browser_position_notify), NULL);
  plugin_signal_connect (geany_plugin, G_OBJECT (G_settings),
                         "notify::wm-secondary-windows-skip-taskbar", FALSE,
                         G_CALLBACK (on_settings_windows_attrs_notify), NULL);
  plugin_signal_connect (geany_plugin, G_OBJECT (G_settings),
                         "notify::wm-secondary-windows-are-transient", FALSE,
                         G_CALLBACK (on_settings_windows_attrs_notify), NULL);
  plugin_signal_connect (geany_plugin, G_OBJECT (G_settings),
                         "notify::wm-secondary-windows-type", FALSE,
                         G_CALLBACK (on_settings_windows_attrs_notify), NULL);
  
  plugin_signal_connect (geany_plugin, NULL, "document-save", TRUE,
                         G_CALLBACK (on_document_save), NULL);
  
  /* add keybindings */
  keybindings_set_item (gwh_keybindings_get_group (), GWH_KB_TOGGLE_INSPECTOR,
                        on_kb_toggle_inspector, GDK_F12, 0, "toggle_inspector",
                        _("Toggle Web Inspector"), NULL);
  keybindings_set_item (gwh_keybindings_get_group (),
                        GWH_KB_SHOW_HIDE_SEPARATE_WINDOW,
                        on_kb_show_hide_separate_window, 0, 0,
                        "show_hide_separate_window",
                        _("Show/Hide Web View's Window"), NULL);
  keybindings_set_item (gwh_keybindings_get_group (), GWH_KB_TOGGLE_BOOKMARK,
                        on_kb_toggle_bookmark, 0, 0, "toggle_bookmark",
                        _("Toggle bookmark for the current website"), NULL);
  keybindings_set_item (gwh_keybindings_get_group (), GWH_KB_LOAD_CURRENT_FILE,
                        on_kb_load_current_file, 0, 0, "load_current_file",
                        _("Load the current file in the web view"), NULL);
}

void
plugin_cleanup (void)
{
  detach_browser ();
  
  gwh_keybindings_cleanup ();
  save_config ();
}


typedef struct _GwhConfigDialog GwhConfigDialog;
struct _GwhConfigDialog
{
  GtkWidget *browser_position;
  GtkWidget *browser_auto_reload;
  
  GtkWidget *secondary_windows_skip_taskbar;
  GtkWidget *secondary_windows_are_transient;
  GtkWidget *secondary_windows_type;
};

static void
on_configure_dialog_response (GtkDialog        *dialog,
                              gint              response_id,
                              GwhConfigDialog  *cdialog)
{
  switch (response_id) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_APPLY:
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_YES: {
      gwh_settings_widget_sync_v (G_settings,
                                  cdialog->browser_position,
                                  cdialog->browser_auto_reload,
                                  cdialog->secondary_windows_skip_taskbar,
                                  cdialog->secondary_windows_are_transient,
                                  cdialog->secondary_windows_type,
                                  NULL);
      break;
    }
    
    default: break;
  }
  
  if (response_id != GTK_RESPONSE_APPLY) {
    g_free (cdialog);
  }
}

GtkWidget *
plugin_configure (GtkDialog *dialog)
{
  GtkWidget        *box1;
  GtkWidget        *box;
  GtkWidget        *alignment;
  GwhConfigDialog  *cdialog;
  
  cdialog = g_malloc (sizeof *cdialog);
  
  /* Top-level box, containing the different frames */
  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  
  /* Browser */
  gtk_box_pack_start (GTK_BOX (box1), ui_frame_new_with_alignment (_("Browser"), &alignment), FALSE, FALSE, 0);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (alignment), box);
  /* browser position */
  cdialog->browser_position = gwh_settings_widget_new (G_settings, "browser-position");
  gtk_box_pack_start (GTK_BOX (box), cdialog->browser_position, FALSE, TRUE, 0);
  /* auto-reload */
  cdialog->browser_auto_reload = gwh_settings_widget_new (G_settings,
                                                          "browser-auto-reload");
  gtk_box_pack_start (GTK_BOX (box), cdialog->browser_auto_reload, FALSE, TRUE, 0);
  
  /* Windows */
  gtk_box_pack_start (GTK_BOX (box1), ui_frame_new_with_alignment (_("Windows"), &alignment), FALSE, FALSE, 0);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (alignment), box);
  /* skip taskbar */
  cdialog->secondary_windows_skip_taskbar = gwh_settings_widget_new (G_settings,
                                                                     "wm-secondary-windows-skip-taskbar");
  gtk_box_pack_start (GTK_BOX (box), cdialog->secondary_windows_skip_taskbar, FALSE, TRUE, 0);
  /* tranisent */
  cdialog->secondary_windows_are_transient = gwh_settings_widget_new (G_settings,
                                                                      "wm-secondary-windows-are-transient");
  gtk_box_pack_start (GTK_BOX (box), cdialog->secondary_windows_are_transient, FALSE, TRUE, 0);
  /* type */
  cdialog->secondary_windows_type = gwh_settings_widget_new (G_settings,
                                                             "wm-secondary-windows-type");
  gtk_box_pack_start (GTK_BOX (box), cdialog->secondary_windows_type, FALSE, TRUE, 0);
  
  g_signal_connect (dialog, "response",
                    G_CALLBACK (on_configure_dialog_response), cdialog);
  
  return box1;
}
