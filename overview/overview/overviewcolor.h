/*
 * overviewcolor.h - This file is part of the Geany Overview plugin
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

#ifndef OVERVIEW_COLOR_H_
#define OVERVIEW_COLOR_H_

#include <gtk/gtk.h>

#define OVERVIEW_TYPE_COLOR (overview_color_get_type ())
#define OVERVIEW_COLOR_INIT { 0.0, 0.0, 0.0, 1.0 }

typedef struct
{
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble alpha;
}
OverviewColor;

GType          overview_color_get_type       (void);
OverviewColor *overview_color_copy           (OverviewColor       *color);
void           overview_color_free           (OverviewColor       *color);
gboolean       overview_color_equal          (const OverviewColor *color1,
                                              const OverviewColor *color2);
gboolean       overview_color_parse          (OverviewColor       *color,
                                              const gchar         *color_str);
gchar         *overview_color_to_string      (const OverviewColor *color);
void           overview_color_from_gdk_color (OverviewColor       *color,
                                              const GdkColor      *gcolor,
                                              gdouble              alpha);
void           overview_color_to_gdk_color   (const OverviewColor *color,
                                              GdkColor            *gcolor);
#if GTK_CHECK_VERSION (3, 0, 0)
void           overview_color_from_rgba      (OverviewColor       *color,
                                              const GdkRGBA       *rgba);
void           overview_color_to_rgba        (const OverviewColor *color,
                                              GdkRGBA             *rgba);
#endif
void           overview_color_from_int       (OverviewColor       *color,
                                              guint32              abgr,
                                              gboolean             with_alpha);
guint32        overview_color_to_int         (const OverviewColor *color,
                                              gboolean             with_alpha);
gboolean       overview_color_from_keyfile   (OverviewColor       *color,
                                              GKeyFile            *keyfile,
                                              const gchar         *section,
                                              const gchar         *option,
                                              GError             **error);
gboolean       overview_color_to_keyfile     (const OverviewColor *color,
                                              GKeyFile            *keyfile,
                                              const gchar         *section,
                                              const gchar         *option);

void           overview_color_from_color_button (OverviewColor       *color,
                                                 GtkColorButton      *button);
void           overview_color_to_color_button   (const OverviewColor *color,
                                                 GtkColorButton      *button);

#endif /* OVERVIEW_COLOR_H_ */
