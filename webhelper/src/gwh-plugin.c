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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <geanyplugin.h>
#include <geany.h>
#include <document.h>

#include "gwh-browser.h"
#include "gwh-settings.h"


#define PLUGIN_NAME     "Web Helper"
#define PLUGIN_TARNAME  "web-helper"


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;
GeanyFunctions   *geany_functions;


PLUGIN_VERSION_CHECK(195)

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Web helper"),
  _("Display a preview web page that gets updated upon document saving and "
    "provide web analysis and debugging tools (aka Web Inspector), all using "
    "WebKit."),
  "0.1",
  "Colomban Wendling <ban@herbesfolles.org>"
)


static GtkWidget   *G_browser   = NULL;
static gint         G_page_num  = 0;
static GtkWidget   *G_notebook  = NULL;
static GwhSettings *G_settings  = NULL;


static void
attach_browser (void)
{
  GwhBrowserPosition position;
  
  g_object_get (G_settings, "browser-position", &position, NULL);
  if (position == GWH_BROWSER_POSITION_LEFT) {
    G_notebook = geany_data->main_widgets->sidebar_notebook;
  } else {
    G_notebook = geany_data->main_widgets->message_window_notebook;
  }
  G_page_num = gtk_notebook_append_page (GTK_NOTEBOOK (G_notebook), G_browser,
                                         gtk_label_new (_("Web preview")));
}

static void
detach_browser (void)
{
  /* remove the page we added. we handle the case where the page were
   * reordered */
  if (gtk_notebook_get_nth_page (GTK_NOTEBOOK (G_notebook),
                                 G_page_num) != G_browser) {
    gint i;
    gint n;
    
    G_page_num = -1;
    n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (G_notebook));
    for (i = 0; i < n; i++) {
      if (gtk_notebook_get_nth_page (GTK_NOTEBOOK (G_notebook),
                                     i) == G_browser) {
        G_page_num = i;
        break;
      }
    }
  }
  if (G_page_num >= 0) {
    gtk_notebook_remove_page (GTK_NOTEBOOK (G_notebook), G_page_num);
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
on_item_auto_reload_toggled (GtkCheckMenuItem *item,
                             gpointer          dummy)
{
  g_object_set (G_OBJECT (G_settings), "browser-auto-reload",
                gtk_check_menu_item_get_active (item), NULL);
}

static void
on_browser_populate_popup (GwhBrowser *browser,
                           GtkMenu    *menu,
                           gpointer    dummy)
{
  GtkWidget  *item;
  gboolean    auto_reload = FALSE;
  
  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_append (menu, item);
  
  g_object_get (G_OBJECT (G_settings), "browser-auto-reload", &auto_reload,
                NULL);
  item = gtk_check_menu_item_new_with_mnemonic (_("Reload upon document saving"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), auto_reload);
  gtk_widget_show (item);
  gtk_menu_append (menu, item);
  g_signal_connect (item, "toggled", G_CALLBACK (on_item_auto_reload_toggled),
                    NULL);
}


static gchar *
get_config_filename (void)
{
  return g_build_filename (geany_data->app->configdir, "plugins",
                           PLUGIN_TARNAME, PLUGIN_TARNAME".conf", NULL);
}

static void
load_config (void)
{
  gchar  *path;
  GError *err = NULL;
  
  G_settings = gwh_settings_get_default ();
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
  utils_mkdir (dirname, FALSE);
  g_free (dirname);
  if (! gwh_settings_save_to_file (G_settings, path, &err)) {
    g_warning ("Failed to save configuration: %s", err->message);
    g_error_free (err);
  }
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
  
  G_browser = gwh_browser_new ();
  gwh_browser_set_inspector_transient_for (GWH_BROWSER (G_browser),
                                           GTK_WINDOW (data->main_widgets->window));
  g_signal_connect (G_browser, "populate-popup",
                    G_CALLBACK (on_browser_populate_popup), NULL);
  
  attach_browser ();
  gtk_widget_show_all (G_browser);
  
  g_signal_connect (G_settings, "notify::browser-position",
                    G_CALLBACK (on_settings_browser_position_notify), NULL);
  
  plugin_signal_connect (geany_plugin, NULL, "document-save", TRUE,
                         G_CALLBACK (on_document_save), NULL);
}

void
plugin_cleanup (void)
{
  detach_browser ();
  
  save_config ();
}


typedef struct _GwhConfigDialog GwhConfigDialog;
struct _GwhConfigDialog
{
  GtkWidget *browser_position;
  GtkWidget *browser_auto_reload;
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
      gwh_settings_widget_sync (G_settings, cdialog->browser_position);
      gwh_settings_widget_sync (G_settings, cdialog->browser_auto_reload);
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
  GtkWidget        *box;
  GwhConfigDialog  *cdialog;
  
  cdialog = g_malloc (sizeof *cdialog);
  
  box = gtk_vbox_new (FALSE, 6);
  cdialog->browser_position = gwh_settings_widget_new (G_settings,
                                                       "browser-position");
  gtk_box_pack_start (GTK_BOX (box), cdialog->browser_position, FALSE, TRUE, 0);
  cdialog->browser_auto_reload = gwh_settings_widget_new (G_settings,
                                                          "browser-auto-reload");
  gtk_box_pack_start (GTK_BOX (box), cdialog->browser_auto_reload,
                      FALSE, TRUE, 0);
  
  g_signal_connect (dialog, "response",
                    G_CALLBACK (on_configure_dialog_response), cdialog);
  
  return box;
}
