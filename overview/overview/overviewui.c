/*
 * overviewui.c - This file is part of the Geany Overview plugin
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

#include "overviewui.h"
#include "overviewplugin.h"
#include "overviewscintilla.h"

typedef void (*DocForEachFunc) (ScintillaObject   *src_sci,
                                OverviewScintilla *overview);

static void overview_ui_hijack_all_editor_views  (void);
static void overview_ui_restore_all_editor_views (void);
static void overview_ui_update_all_editor_views  (void);

static OverviewPrefs *overview_ui_prefs     = NULL;
static GtkWidget     *overview_ui_menu_sep  = NULL;
static GtkWidget     *overview_ui_menu_item = NULL;

static inline void
overview_ui_scintilla_foreach (DocForEachFunc callback)
{
  guint i;
  foreach_document (i)
    {
      GeanyDocument     *doc = documents[i];
      ScintillaObject   *src_sci;
      OverviewScintilla *overview;
      src_sci  = doc->editor->sci;
      overview = g_object_get_data (G_OBJECT (src_sci), "overview");
      if (IS_SCINTILLA (doc->editor->sci))
        callback (src_sci, overview);
      else
        g_critical ("enumerating invalid scintilla editor widget");
    }
}

static inline gboolean
overview_ui_position_is_left (void)
{
  GtkPositionType position;
  g_object_get (overview_ui_prefs, "position", &position, NULL);
  return (position == GTK_POS_LEFT);
}

static inline void
overview_ui_container_add_expanded (GtkContainer *container,
                                    GtkWidget    *widget)
{
  gtk_container_add (container, widget);
  /* GTK+ 3 doesn't expand by default anymore */
#if GTK_CHECK_VERSION (3, 0, 0)
  g_object_set (widget, "expand", TRUE, NULL);
#endif
}

static void
overview_ui_hijack_editor_view (ScintillaObject   *src_sci,
                                OverviewScintilla *null_and_unused)
{
  GtkWidget     *parent;
  GtkWidget     *container;
  GtkWidget     *overview;
  gboolean       on_left;

  g_assert (g_object_get_data (G_OBJECT (src_sci), "overview") == NULL);

  parent    = gtk_widget_get_parent (GTK_WIDGET (src_sci));
  container = gtk_hbox_new (FALSE, 0);
  overview  = overview_scintilla_new (src_sci);

  overview_prefs_bind_scintilla (overview_ui_prefs, G_OBJECT (overview));
  gtk_widget_set_no_show_all (overview, TRUE);

  g_object_set_data (G_OBJECT (src_sci), "overview", overview);

  on_left = overview_ui_position_is_left ();
#ifndef OVERVIEW_UI_SUPPORTS_LEFT_POSITION
  if (on_left)
    {
      g_critical ("Refusing to add Overview into left position because "
                  "your Geany version isn't new enough to support this "
                  "without crashing hard.");
      on_left = FALSE;
    }
#endif

  g_object_ref (src_sci);
  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (src_sci));

  if (on_left)
    {
      gtk_box_pack_start (GTK_BOX (container), overview, FALSE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (container), GTK_WIDGET (src_sci), TRUE, TRUE, 0);
    }
  else
    {
      gtk_box_pack_start (GTK_BOX (container), GTK_WIDGET (src_sci), TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (container), overview, FALSE, TRUE, 0);
    }

  overview_ui_container_add_expanded (GTK_CONTAINER (parent), container);
  g_object_unref (src_sci);

  gtk_widget_show_all (container);
}

static void
overview_ui_hijack_all_editor_views (void)
{
  overview_ui_scintilla_foreach (overview_ui_hijack_editor_view);
}

static void
overview_ui_restore_editor_view (ScintillaObject   *src_sci,
                                 OverviewScintilla *overview)
{
  GtkWidget *old_parent = gtk_widget_get_parent (GTK_WIDGET (src_sci));
  GtkWidget *new_parent = gtk_widget_get_parent (old_parent);
  g_object_ref (src_sci);
  g_object_set_data (G_OBJECT (src_sci), "overview", NULL);
  gtk_container_remove (GTK_CONTAINER (old_parent), GTK_WIDGET (src_sci));
  gtk_container_remove (GTK_CONTAINER (new_parent), old_parent);
  overview_ui_container_add_expanded (GTK_CONTAINER (new_parent), GTK_WIDGET (src_sci));
  g_object_unref (src_sci);
}

static void
overview_ui_restore_all_editor_views (void)
{
  overview_ui_scintilla_foreach (overview_ui_restore_editor_view);
}

static void
overview_ui_update_editor_view (ScintillaObject   *src_sci,
                                OverviewScintilla *overview)
{
  GtkWidget         *parent;
  gboolean           on_left;

  on_left = overview_ui_position_is_left ();
#ifndef OVERVIEW_UI_SUPPORTS_LEFT_POSITION
  if (on_left)
    {
      g_critical ("Refusing to swap Overview into left position because "
                  "your Geany version isn't new enough to support this "
                  "without crashing hard.");
      on_left = FALSE;
    }
#endif

  parent = gtk_widget_get_parent (GTK_WIDGET (src_sci));

  g_object_ref (src_sci);
  g_object_ref (overview);

  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (src_sci));
  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (overview));

  if (on_left)
    {
      gtk_box_pack_start (GTK_BOX (parent), GTK_WIDGET (overview), FALSE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (parent), GTK_WIDGET (src_sci), TRUE, TRUE, 0);
    }
  else
    {
      gtk_box_pack_start (GTK_BOX (parent), GTK_WIDGET (src_sci), TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (parent), GTK_WIDGET (overview), FALSE, TRUE, 0);
    }

  gtk_widget_show_all (parent);

  g_object_unref (overview);
  g_object_unref (src_sci);

  overview_scintilla_sync (overview);
}

void
overview_ui_update_all_editor_views (void)
{
  overview_ui_scintilla_foreach (overview_ui_update_editor_view);
}

GtkWidget *
overview_ui_get_menu_item (void)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (overview_ui_menu_item), NULL);
  return overview_ui_menu_item;
}

static inline OverviewScintilla *
overview_scintilla_from_document (GeanyDocument *doc)
{
  if (DOC_VALID (doc))
    {
      ScintillaObject *src_sci = doc->editor->sci;
      if (IS_SCINTILLA (src_sci))
        return g_object_get_data (G_OBJECT (src_sci), "overview");
    }
  return NULL;
}

static void
on_document_open_new (G_GNUC_UNUSED GObject *unused,
                      GeanyDocument         *doc,
                      G_GNUC_UNUSED gpointer user_data)
{
  g_object_set_data (G_OBJECT (doc->editor->sci), "overview-document", doc);
  overview_ui_hijack_editor_view (doc->editor->sci, NULL);
  overview_ui_queue_update ();
}

static void
on_document_activate_reload (G_GNUC_UNUSED GObject       *unused,
                             G_GNUC_UNUSED GeanyDocument *doc,
                             G_GNUC_UNUSED gpointer       user_data)
{
  overview_ui_queue_update ();
}

static void
on_document_close (G_GNUC_UNUSED GObject *unused,
                   GeanyDocument         *doc,
                   G_GNUC_UNUSED gpointer user_data)
{
  OverviewScintilla *overview;
  overview = overview_scintilla_from_document (doc);
  overview_ui_restore_editor_view (doc->editor->sci, overview);
}

static gint
overview_ui_get_menu_item_pos (GtkWidget *menu,
                               GtkWidget *item_before)
{
  GList *children;
  gint   pos = 0;
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (GList *iter = children; iter != NULL; iter = g_list_next (iter), pos++)
    {
      if (iter->data == item_before)
        break;
    }
  g_list_free (children);
  return pos + 1;
}

static void
overview_ui_add_menu_item (void)
{
  static const gchar *view_menu_name = "menu_view1_menu";
  static const gchar *prev_item_name = "menu_show_sidebar1";
  GtkWidget          *main_window = geany_data->main_widgets->window;
  GtkWidget          *view_menu;
  GtkWidget          *prev_item;
  gint                item_pos;
  gboolean            visible = FALSE;

  view_menu = ui_lookup_widget (main_window, view_menu_name);
  if (! GTK_IS_MENU (view_menu))
    {
      g_critical ("failed to locate the View menu (%s) in Geany's main menu",
                  view_menu_name);
      return;
    }

  overview_ui_menu_item = gtk_check_menu_item_new_with_label (_("Show Overview"));
  prev_item = ui_lookup_widget (main_window, prev_item_name);
  if (! GTK_IS_MENU_ITEM (prev_item))
    {
      g_critical ("failed to locate the Show Sidebar menu item (%s) in Geany's UI",
                  prev_item_name);
      overview_ui_menu_sep = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (view_menu), overview_ui_menu_sep);
      gtk_menu_shell_append (GTK_MENU_SHELL (view_menu), overview_ui_menu_item);
      gtk_widget_show (overview_ui_menu_sep);
    }
  else
    {
      item_pos = overview_ui_get_menu_item_pos (view_menu, prev_item);
      overview_ui_menu_sep = NULL;
      gtk_menu_shell_insert (GTK_MENU_SHELL (view_menu), overview_ui_menu_item, item_pos);
    }

  g_object_get (overview_ui_prefs, "visible", &visible, NULL);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (overview_ui_menu_item), visible);
  g_object_bind_property (overview_ui_menu_item, "active",
                          overview_ui_prefs, "visible",
                          G_BINDING_DEFAULT);

  gtk_widget_show (overview_ui_menu_item);
}

static void
on_position_pref_notify (OverviewPrefs *prefs,
                         GParamSpec    *pspec,
                         gpointer       user_data)
{
  overview_ui_update_all_editor_views ();
}

void
overview_ui_init (OverviewPrefs *prefs)
{
  overview_ui_prefs = g_object_ref (prefs);

  overview_ui_add_menu_item ();
  overview_ui_hijack_all_editor_views ();

  g_signal_connect (prefs, "notify::position",
                    G_CALLBACK (on_position_pref_notify), NULL);

  plugin_signal_connect (geany_plugin, NULL, "document-new", TRUE, G_CALLBACK (on_document_open_new), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-open", TRUE, G_CALLBACK (on_document_open_new), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-activate", TRUE, G_CALLBACK (on_document_activate_reload), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-reload", TRUE, G_CALLBACK (on_document_activate_reload), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-close", TRUE, G_CALLBACK (on_document_close), NULL);

}

void
overview_ui_deinit (void)
{
  overview_ui_restore_all_editor_views ();

  if (GTK_IS_WIDGET (overview_ui_menu_sep))
    gtk_widget_destroy (overview_ui_menu_sep);
  gtk_widget_destroy (overview_ui_menu_item);

  if (OVERVIEW_IS_PREFS (overview_ui_prefs))
    g_object_unref (overview_ui_prefs);
  overview_ui_prefs = NULL;
}

static gboolean
on_update_overview_later (gpointer user_data)
{
  GeanyDocument *doc;
  doc = document_get_current ();
  if (DOC_VALID (doc))
    {
      OverviewScintilla *overview;
      overview = g_object_get_data (G_OBJECT (doc->editor->sci), "overview");
      if (OVERVIEW_IS_SCINTILLA (overview))
        overview_scintilla_sync (overview);
    }
  return FALSE;
}

void
overview_ui_queue_update (void)
{
  g_idle_add_full (G_PRIORITY_LOW, on_update_overview_later, NULL, NULL);
}
