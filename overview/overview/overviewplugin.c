/*
 * overviewplugin.c - This file is part of the Geany Overview plugin
 *
 * Copyright (c) 2015 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "overviewplugin.h"
#include "overviewscintilla.h"
#include "overviewprefs.h"
#include "overviewprefspanel.h"
#include "overviewui.h"
#include <errno.h>

GeanyPlugin    *geany_plugin;
GeanyData      *geany_data;
GeanyFunctions *geany_functions;

/* scintilla_get_type() is needed but was only added to the API in 224, but
 * previous versions will still work on Linux since it's before the symbol
 * linkage was fixed up. TODO: remove this after next Geany release. */
#if GEANY_API_VERSION >= 224
PLUGIN_VERSION_CHECK (224)
#else
PLUGIN_VERSION_CHECK (211)
#endif

PLUGIN_SET_INFO (
  "Overview",
  _("Provides an overview of the active document"),
  "0.01",
  "Matthew Brush <matt@geany.org>")

static OverviewPrefs   *overview_prefs     = NULL;

static void write_config (void);

static void
on_visible_pref_notify (OverviewPrefs *prefs,
                        GParamSpec    *pspec,
                        gpointer       user_data)
{
  write_config ();
}

static void
toggle_visibility (void)
{
  gboolean visible = TRUE;
  g_object_get (overview_prefs, "visible", &visible, NULL);
  g_object_set (overview_prefs, "visible", !visible, NULL);
}

enum
{
  KB_TOGGLE_VISIBLE,
  KB_TOGGLE_POSITION,
  KB_TOGGLE_INVERTED,
  NUM_KB
};

static gboolean
on_kb_activate (guint keybinding_id)
{
  switch (keybinding_id)
    {
    case KB_TOGGLE_VISIBLE:
      toggle_visibility ();
      break;
    case KB_TOGGLE_POSITION:
      {
        GtkPositionType pos;
        g_object_get (overview_prefs, "position", &pos, NULL);
        pos = (pos == GTK_POS_LEFT) ? GTK_POS_RIGHT : GTK_POS_LEFT;
        g_object_set (overview_prefs, "position", pos, NULL);
        break;
      }
    case KB_TOGGLE_INVERTED:
      {
        gboolean inv = FALSE;
        g_object_get (overview_prefs, "overlay-inverted", &inv, NULL);
        g_object_set (overview_prefs, "overlay-inverted", !inv, NULL);
        break;
      }
    default:
      break;
    }
  return TRUE;
}

static gchar *
get_config_file (void)
{
  gchar              *dir;
  gchar              *fn;
  static const gchar *def_config = OVERVIEW_PREFS_DEFAULT_CONFIG;

  dir = g_build_filename (geany_data->app->configdir, "plugins", "overview", NULL);
  fn = g_build_filename (dir, "prefs.conf", NULL);

  if (! g_file_test (fn, G_FILE_TEST_IS_DIR))
    {
      if (g_mkdir_with_parents (dir, 0755) != 0)
        {
          g_critical ("failed to create config dir '%s': %s", dir, g_strerror (errno));
          g_free (dir);
          g_free (fn);
          return NULL;
        }
    }

  g_free (dir);

  if (! g_file_test (fn, G_FILE_TEST_EXISTS))
    {
      GError *error = NULL;
      if (!g_file_set_contents (fn, def_config, -1, &error))
        {
          g_critical ("failed to save default config to file '%s': %s",
                      fn, error->message);
          g_error_free (error);
          g_free (fn);
          return NULL;
        }
    }

  return fn;
}

static void
write_config (void)
{
  gchar  *conf_file;
  GError *error = NULL;
  conf_file = get_config_file ();
  if (! overview_prefs_save (overview_prefs, conf_file, &error))
    {
      g_critical ("failed to save preferences to file '%s': %s", conf_file, error->message);
      g_error_free (error);
    }
  g_free (conf_file);
}

void
plugin_init (G_GNUC_UNUSED GeanyData *data)
{
  gchar          *conf_file;
  GError         *error = NULL;
  GeanyKeyGroup  *key_group;

  plugin_module_make_resident (geany_plugin);

  overview_prefs = overview_prefs_new ();
  conf_file = get_config_file ();
  if (! overview_prefs_load (overview_prefs, conf_file, &error))
    {
      g_critical ("failed to load preferences file '%s': %s", conf_file, error->message);
      g_error_free (error);
    }
  g_free (conf_file);

  overview_ui_init (overview_prefs);

  key_group = plugin_set_key_group (geany_plugin,
                                    "overview",
                                    NUM_KB,
                                    on_kb_activate);

  keybindings_set_item (key_group,
                        KB_TOGGLE_VISIBLE,
                        NULL, 0, 0,
                        "toggle-visibility",
                        _("Toggle Visibility"),
                        overview_ui_get_menu_item ());

  keybindings_set_item (key_group,
                        KB_TOGGLE_POSITION,
                        NULL, 0, 0,
                        "toggle-position",
                        _("Toggle Left/Right Position"),
                        NULL);

  keybindings_set_item (key_group,
                        KB_TOGGLE_INVERTED,
                        NULL, 0, 0,
                        "toggle-inverted",
                        _("Toggle Overlay Inversion"),
                        NULL);

  g_signal_connect (overview_prefs, "notify::visible", G_CALLBACK (on_visible_pref_notify), NULL);
}

void
plugin_cleanup (void)
{
  write_config ();
  overview_ui_deinit ();

  if (OVERVIEW_IS_PREFS (overview_prefs))
    g_object_unref (overview_prefs);
  overview_prefs = NULL;
}

static void
on_prefs_stored (OverviewPrefsPanel *panel,
                 OverviewPrefs      *prefs,
                 gpointer            user_data)
{
  write_config ();
  overview_ui_queue_update ();
}

GtkWidget *
plugin_configure (GtkDialog *dialog)
{
  GtkWidget *panel;
  panel = overview_prefs_panel_new (overview_prefs, dialog);
  g_signal_connect (panel, "prefs-stored", G_CALLBACK (on_prefs_stored), NULL);
  return panel;
}
