/*
 * overviewprefspanel.c - This file is part of the Geany Overview plugin
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

#include "overviewprefspanel.h"
#include "overviewcolor.h"
#include "overviewplugin.h"
#include "overviewui.h"

struct OverviewPrefsPanel_
{
  GtkFrame       parent;
  OverviewPrefs *prefs;
  GtkWidget     *prefs_table;
  GtkWidget     *width_spin;
  GtkWidget     *zoom_spin;
  GtkWidget     *scr_lines_spin;
  GtkWidget     *pos_left_check;
  GtkWidget     *hide_tt_check;
  GtkWidget     *hide_sb_check;
  GtkWidget     *ovl_dis_check;
  GtkWidget     *ovl_inv_check;
  GtkWidget     *ovl_clr_btn;
  GtkWidget     *out_clr_btn;
};

struct OverviewPrefsPanelClass_
{
  GtkFrameClass parent_class;
};

static void overview_prefs_panel_finalize (GObject *object);

G_DEFINE_TYPE (OverviewPrefsPanel, overview_prefs_panel, GTK_TYPE_FRAME)

static void
overview_prefs_panel_class_init (OverviewPrefsPanelClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = overview_prefs_panel_finalize;

  g_signal_new ("prefs-stored",
                G_TYPE_FROM_CLASS (g_object_class),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE,
                1, OVERVIEW_TYPE_PREFS);

  g_signal_new ("prefs-loaded",
                G_TYPE_FROM_CLASS (g_object_class),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE,
                1, OVERVIEW_TYPE_PREFS);
}

static void
overview_prefs_panel_finalize (GObject *object)
{
  OverviewPrefsPanel *self;

  g_return_if_fail (OVERVIEW_IS_PREFS_PANEL (object));

  self = OVERVIEW_PREFS_PANEL (object);

  g_object_unref (self->prefs);

  G_OBJECT_CLASS (overview_prefs_panel_parent_class)->finalize (object);
}

static GtkWidget *
builder_get_widget (GtkBuilder  *builder,
                    const gchar *name)
{
  GObject *result = NULL;
  gchar   *real_name;

  real_name = g_strdup_printf ("overview-%s", name);
  result    = gtk_builder_get_object (builder, real_name);

  if (! G_IS_OBJECT (result))
    g_critical ("unable to find widget '%s' in UI file", real_name);
  else if (! GTK_IS_WIDGET (result))
    g_critical ("object '%s' in UI file is not a widget", real_name);

  g_free (real_name);

  return (GtkWidget*) result;
}

static void
overview_prefs_panel_store_prefs (OverviewPrefsPanel *self)
{
  guint         width;
  gint          zoom;
  guint         scr_lines;
  gboolean      pos_left  = FALSE;
  OverviewColor ovl_clr   = {0,0,0,0};
  OverviewColor out_clr   = {0,0,0,0};

  width     = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->width_spin));
  zoom      = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->zoom_spin));
  scr_lines = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->scr_lines_spin));
  pos_left  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->pos_left_check));
  overview_color_from_color_button (&ovl_clr, GTK_COLOR_BUTTON (self->ovl_clr_btn));
  overview_color_from_color_button (&out_clr, GTK_COLOR_BUTTON (self->out_clr_btn));

  g_object_set (self->prefs,
                "width", width,
                "zoom", zoom,
                "scroll-lines", scr_lines,
                "position", pos_left ? GTK_POS_LEFT : GTK_POS_RIGHT,
                "show-tooltip", !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->hide_tt_check)),
                "show-scrollbar", !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->hide_sb_check)),
                "overlay-enabled", !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->ovl_dis_check)),
                "overlay-inverted", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->ovl_inv_check)),
                "overlay-color", &ovl_clr,
                "overlay-outline-color", &out_clr,
                NULL);

  g_signal_emit_by_name (self, "prefs-stored", self->prefs);
}

static void
overview_prefs_panel_load_prefs (OverviewPrefsPanel *self)
{
  gint            zoom      = 0;
  guint           width     = 0;
  guint           scr_lines = 0;
  gboolean        show_tt   = FALSE;
  gboolean        show_sb   = FALSE;
  gboolean        ovl_en    = FALSE;
  gboolean        ovl_inv   = FALSE;
  GtkPositionType pos       = FALSE;
  OverviewColor  *ovl_clr   = NULL;
  OverviewColor  *out_clr   = NULL;

  g_object_get (self->prefs,
                "width", &width,
                "zoom", &zoom,
                "scroll-lines", &scr_lines,
                "position", &pos,
                "show-tooltip", &show_tt,
                "show-scrollbar", &show_sb,
                "overlay-enabled", &ovl_en,
                "overlay-inverted", &ovl_inv,
                "overlay-color", &ovl_clr,
                "overlay-outline-color", &out_clr,
                NULL);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->width_spin), width);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->zoom_spin), zoom);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->scr_lines_spin), scr_lines);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->pos_left_check), (pos == GTK_POS_LEFT));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->hide_tt_check), !show_tt);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->hide_sb_check), !show_sb);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->ovl_inv_check), ovl_inv);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->ovl_dis_check), !ovl_en);
  overview_color_to_color_button (ovl_clr, GTK_COLOR_BUTTON (self->ovl_clr_btn));
  overview_color_to_color_button (out_clr, GTK_COLOR_BUTTON (self->out_clr_btn));

  overview_color_free (ovl_clr);
  overview_color_free (out_clr);

  g_signal_emit_by_name (self, "prefs-loaded", self->prefs);
}

static void
overview_prefs_panel_init (OverviewPrefsPanel *self)
{
  GtkBuilder *builder;
  GError     *error = NULL;
  GtkWidget  *overlay_frame;

  builder = gtk_builder_new ();
  if (! gtk_builder_add_from_file (builder, OVERVIEW_PREFS_UI_FILE, &error))
    {
      g_critical ("failed to open UI file '%s': %s", OVERVIEW_PREFS_UI_FILE, error->message);
      g_error_free (error);
      g_object_unref (builder);
      return;
    }

  self->prefs_table    = builder_get_widget (builder, "prefs-table");
  self->width_spin     = builder_get_widget (builder, "width-spin");
  self->zoom_spin      = builder_get_widget (builder, "zoom-spin");
  self->scr_lines_spin = builder_get_widget (builder, "scroll-lines-spin");
  self->pos_left_check = builder_get_widget (builder, "position-left-check");
  self->hide_tt_check  = builder_get_widget (builder, "hide-tooltip-check");
  self->hide_sb_check  = builder_get_widget (builder, "hide-scrollbar-check");
  self->ovl_inv_check = builder_get_widget (builder, "overlay-inverted-check");
  self->ovl_clr_btn    = builder_get_widget (builder, "overlay-color");
  self->out_clr_btn    = builder_get_widget (builder, "overlay-outline-color");

  // The "Draw over visible area" checkbox hides/shows the "Overlay" frame
  self->ovl_dis_check  = builder_get_widget (builder, "overlay-disable-check");
  overlay_frame = builder_get_widget (builder, "overlay-frame");
  g_object_bind_property (self->ovl_dis_check, "active", overlay_frame, "sensitive", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  gtk_widget_show_all (self->prefs_table);
  gtk_container_add (GTK_CONTAINER (self), self->prefs_table);
  g_object_unref (builder);

#ifndef OVERVIEW_UI_SUPPORTS_LEFT_POSITION
  gtk_widget_set_no_show_all (self->pos_left_check, TRUE);
  gtk_widget_hide (self->pos_left_check);
#endif
}

static void
on_host_dialog_response (GtkDialog          *dialog,
                         gint                response_id,
                         OverviewPrefsPanel *self)
{
  switch (response_id)
    {
    case GTK_RESPONSE_APPLY:
    case GTK_RESPONSE_OK:
      overview_prefs_panel_store_prefs (self);
      break;
    case GTK_RESPONSE_CANCEL:
    default:
      break;
    }
}

GtkWidget *
overview_prefs_panel_new (OverviewPrefs *prefs,
                          GtkDialog     *host_dialog)
{
  OverviewPrefsPanel *self;
  self = g_object_new (OVERVIEW_TYPE_PREFS_PANEL, NULL);
  self->prefs = g_object_ref (prefs);
  g_signal_connect (host_dialog, "response", G_CALLBACK (on_host_dialog_response), self);
  overview_prefs_panel_load_prefs (self);
  return GTK_WIDGET (self);
}
