/*
 * overviewcolor.c - This file is part of the Geany Overview plugin
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

#include "overviewcolor.h"
#include <string.h>

OverviewColor *
overview_color_copy (OverviewColor *color)
{
  OverviewColor *new_color = g_slice_new0 (OverviewColor);
  memcpy (new_color, color, sizeof (OverviewColor));
  return new_color;
}

void
overview_color_free (OverviewColor *color)
{
  if (color != NULL)
    g_slice_free (OverviewColor, color);
}

gboolean
overview_color_equal (const OverviewColor *color1,
                      const OverviewColor *color2)
{
  return (color1->red   == color2->red &&
          color1->green == color2->green &&
          color1->blue  == color2->blue &&
          color1->alpha == color2->alpha);
}

gboolean
overview_color_parse (OverviewColor *color,
                      const gchar   *color_str)
{
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (color_str != NULL, FALSE);

#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA gcolor;
  if (gdk_rgba_parse (&gcolor, color_str))
    {
      overview_color_from_rgba (color, &gcolor);
      return TRUE;
    }
#else
  GdkColor gcolor;
  if (gdk_color_parse (color_str, &gcolor))
    {
      overview_color_from_gdk_color (color, &gcolor, 1.0);
      return TRUE;
    }
#endif

  return FALSE;
}

gchar *
overview_color_to_string (const OverviewColor *color)
{
  g_return_val_if_fail (color != NULL, NULL);

#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA gcolor;
  overview_color_to_rgba (color, &gcolor);
  return gdk_rgba_to_string (&gcolor);
#else
  GdkColor gcolor;
  overview_color_to_gdk_color (color, &gcolor);
  return gdk_color_to_string (&gcolor);
#endif
}

void
overview_color_from_gdk_color (OverviewColor  *color,
                               const GdkColor *gcolor,
                               gdouble         alpha)
{
  g_return_if_fail (color != NULL);
  g_return_if_fail (gcolor != NULL);

  color->red   = (gdouble) gcolor->red / G_MAXUINT16;
  color->green = (gdouble) gcolor->green / G_MAXUINT16;
  color->blue  = (gdouble) gcolor->blue / G_MAXUINT16;
  color->alpha = alpha;
}

void
overview_color_to_gdk_color (const OverviewColor *color,
                             GdkColor            *gcolor)
{
  g_return_if_fail (color != NULL);
  g_return_if_fail (gcolor != NULL);

  gcolor->red   = (guint16)(color->red * G_MAXUINT16);
  gcolor->green = (guint16)(color->green * G_MAXUINT16);
  gcolor->blue  = (guint16)(color->blue * G_MAXUINT16);
}

#if GTK_CHECK_VERSION (3, 0, 0)
void
overview_color_from_rgba (OverviewColor *color,
                          const GdkRGBA *rgba)
{
  g_return_if_fail (color != NULL);
  g_return_if_fail (rgba != NULL);

  color->red   = rgba->red;
  color->green = rgba->green;
  color->blue  = rgba->blue;
  color->alpha = rgba->alpha;
}

void
overview_color_to_rgba (const OverviewColor *color,
                        GdkRGBA             *rgba)
{
  g_return_if_fail (color != NULL);
  g_return_if_fail (rgba != NULL);

  rgba->red   = color->red;
  rgba->green = color->green;
  rgba->blue  = color->blue;
  rgba->alpha = color->alpha;
}
#endif

void
overview_color_from_int (OverviewColor *color,
                         guint32        abgr,
                         gboolean       with_alpha)
{
  g_return_if_fail (color != NULL);
  guint8 r = abgr & 0xFF;
  guint8 g = (abgr>>8) & 0xFF;
  guint8 b = (abgr>>16) & 0xFF;
  guint8 a = 255;

  if (with_alpha)
    a = (abgr>>24) & 0xFF;

  color->red   = (gdouble)r / G_MAXUINT8;
  color->green = (gdouble)g / G_MAXUINT8;
  color->blue  = (gdouble)b / G_MAXUINT8;
  color->alpha = (gdouble)a / G_MAXUINT8;
}

guint32
overview_color_to_int (const OverviewColor *color,
                       gboolean             with_alpha)
{
  g_return_val_if_fail (color != NULL, 0);

  guint32 r = (guint8)(color->red * 255.0);
  guint32 g = (guint8)(color->green * 255.0);
  guint32 b = (guint8)(color->blue * 255.0);
  guint32 a = 0;

  if (with_alpha)
    a = (guint8)(color->alpha * 255.0);

  return (a<<24) | (b<<16) | (g<<8) | r;
}

gboolean
overview_color_from_keyfile   (OverviewColor *color,
                               GKeyFile      *keyfile,
                               const gchar   *section,
                               const gchar   *option,
                               GError       **error)
{
  gchar  *color_key;
  gchar  *alpha_key;
  gchar  *clr_str;
  gdouble alpha;

  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (keyfile != NULL, FALSE);
  g_return_val_if_fail (section != NULL, FALSE);
  g_return_val_if_fail (option != NULL, FALSE);

  color_key = g_strdup_printf ("%s-color", option);
  alpha_key = g_strdup_printf ("%s-alpha", option);

  clr_str = g_key_file_get_string (keyfile, section, color_key, error);
  if (*error != NULL)
    {
      g_free (color_key);
      g_free (alpha_key);
      return FALSE;
    }

  g_free (color_key);

  alpha = g_key_file_get_double (keyfile, section, alpha_key, error);
  if (*error != NULL)
    {
      g_free (alpha_key);
      g_free (clr_str);
      return FALSE;
    }

  g_free (alpha_key);

  if (alpha < 0.0 || alpha > 1.0)
    g_warning ("alpha value '%g' from keyfile out of 0-1 range", alpha);

  overview_color_parse (color, clr_str);
  color->alpha = alpha;

  g_free (clr_str);

  return TRUE;
}

gboolean
overview_color_to_keyfile (const OverviewColor *color,
                           GKeyFile            *keyfile,
                           const gchar         *section,
                           const gchar         *option)
{
  gchar *color_key;
  gchar *alpha_key;
  gchar *clr_str;

  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (keyfile != NULL, FALSE);
  g_return_val_if_fail (section != NULL, FALSE);
  g_return_val_if_fail (option != NULL, FALSE);

  color_key = g_strdup_printf ("%s-color", option);
  alpha_key = g_strdup_printf ("%s-alpha", option);

  clr_str = overview_color_to_string (color);
  g_key_file_set_string (keyfile, section, color_key, clr_str);
  g_free (color_key);
  g_free (clr_str);

  g_key_file_set_double (keyfile, section, alpha_key, color->alpha);
  g_free (alpha_key);

  return TRUE;
}

void
overview_color_from_color_button (OverviewColor  *color,
                                  GtkColorButton *button)
{
  GdkColor gcolor;
  guint16  alpha;
  gdouble  falpha;
  gtk_color_button_get_color (button, &gcolor);
  alpha = gtk_color_button_get_alpha (button);
  falpha = (gdouble) alpha / (gdouble) G_MAXUINT16;
  overview_color_from_gdk_color (color, &gcolor, falpha);
}

void
overview_color_to_color_button (const OverviewColor *color,
                                GtkColorButton      *button)
{
  GdkColor gcolor;
  guint16  alpha;
  overview_color_to_gdk_color (color, &gcolor);
  gtk_color_button_set_color (button, &gcolor);
  alpha = (guint16)(color->alpha * (gdouble) G_MAXUINT16);
  gtk_color_button_set_alpha (button, alpha);
}

GType
overview_color_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      type =
        g_boxed_type_register_static ("OverviewColor",
                                      (GBoxedCopyFunc) overview_color_copy,
                                      (GBoxedFreeFunc) overview_color_free);
    }
  return type;
}
