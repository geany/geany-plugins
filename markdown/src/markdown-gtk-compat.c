/*
 * markdown-gtk-compat.c - Part of the Geany Markdown plugin
 *
 * Copyright 2012 Matthew Brush <mbrush@codebrainz.ca>
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
 *
 */

#include <gtk/gtk.h>
#include "markdown-gtk-compat.h"

GtkWidget *markdown_gtk_table_new(guint rows, guint columns, gboolean homogeneous)
{
  GtkWidget *table;

#if !GTK_CHECK_VERSION(3, 4, 0)
  table = gtk_table_new(rows, columns, homogeneous);
#else
  guint row_cnt, col_cnt;

  table = gtk_grid_new();

  gtk_grid_set_row_homogeneous(MARKDOWN_GTK_TABLE(table), homogeneous);
  gtk_grid_set_column_homogeneous(MARKDOWN_GTK_TABLE(table), homogeneous);

  for (row_cnt=0; row_cnt < rows; row_cnt++)
    gtk_grid_insert_row(MARKDOWN_GTK_TABLE(table), row_cnt);

  for (col_cnt=0; col_cnt < columns; col_cnt++)
    gtk_grid_insert_column(MARKDOWN_GTK_TABLE(table), col_cnt);
#endif

  return table;
}

void markdown_gtk_table_attach(MarkdownGtkTable *table, GtkWidget *child,
  guint left_attach, guint right_attach, guint top_attach, guint bottom_attach,
  GtkAttachOptions xoptions, GtkAttachOptions yoptions)
{
#if !GTK_CHECK_VERSION(3, 4, 0)
  gtk_table_attach(table, child, left_attach, right_attach, top_attach,
    bottom_attach, xoptions, yoptions, 0, 0);
#else
  gtk_grid_attach(table, child, left_attach, top_attach,
    right_attach - left_attach, bottom_attach - top_attach);
#endif
}

GtkWidget *markdown_gtk_color_button_new_with_color(MarkdownColor *color)
{
  GtkWidget *btn;

  btn = gtk_color_button_new();

#if !GTK_CHECK_VERSION(3, 0, 0)
{
  GdkColor clr;
  clr.red = color->red * 256;
  clr.green = color->green * 256;
  clr.blue = color->blue * 256;
  gtk_color_button_set_color(GTK_COLOR_BUTTON(btn), &clr);
}
#else
{
  GdkRGBA clr;
  clr.red = color->red / 256.0;
  clr.green = color->green / 256.0;
  clr.blue = color->blue / 256.0;
# if !GTK_CHECK_VERSION(3, 4, 0)
  gtk_color_button_set_rgba(GTK_COLOR_BUTTON(btn), &clr);
# else
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(btn), &clr);
# endif
}
#endif

  return btn;
}

gboolean markdown_color_parse(const gchar *spec, MarkdownColor *color)
{
  gboolean result;
  g_return_val_if_fail(spec && color, FALSE);

#if !GTK_CHECK_VERSION(3, 0, 0)
{
  GdkColor clr;
  result = gdk_color_parse(spec, &clr);
  if (result) {
    color->red = clr.red / 256;
    color->green = clr.green / 256;
    color->blue = clr.blue / 256;
  }
}
#else
{
  GdkRGBA clr;
  result = gdk_rgba_parse(&clr, spec);
  if (result) {
    color->red = (guint8) (clr.red * 256.0);
    color->green = (guint8) (clr.green * 256.0);
    color->blue = (guint8) (clr.blue * 256.0);
  }
}
#endif

  return result;
}

void markdown_gtk_color_button_get_color(GtkColorButton *button, MarkdownColor *color)
{
  g_return_if_fail(button);
  g_return_if_fail(color);

#if !GTK_CHECK_VERSION(3, 0, 0)
  GdkColor clr;
  gtk_color_button_get_color(button, &clr);
  color->red = clr.red / 256;
  color->green = clr.green / 256;
  color->blue = clr.blue / 256;
#else
  GdkRGBA clr;
# if !GTK_CHECK_VERSION(3, 4, 0)
  gtk_color_button_get_rgba(button, &clr);
# else
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &clr);
# endif
  color->red = (guint8) (clr.red * 256.0);
  color->green = (guint8) (clr.green * 256.0);
  color->blue = (guint8) (clr.blue * 256.0);
#endif
}
