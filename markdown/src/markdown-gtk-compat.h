/*
 * markdown-gtk-compat.h - Part of the Geany Markdown plugin
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

#ifndef MARKDOWN_GTK_COMPAT_H_
#define MARKDOWN_GTK_COMPAT_H_

#if !GTK_CHECK_VERSION(3, 4, 0)
# define MarkdownGtkTable GtkTable
# define MARKDOWN_GTK_TABLE GTK_TABLE
# define markdown_gtk_table_set_row_spacing(table, spacing) \
    gtk_table_set_row_spacings(table, spacing)
# define markdown_gtk_table_set_col_spacing(table, spacing) \
    gtk_table_set_col_spacings(table, spacing)
#else
# define MarkdownGtkTable GtkGrid
# define MARKDOWN_GTK_TABLE GTK_GRID
# define markdown_gtk_table_set_row_spacing(table, spacing) \
    gtk_grid_set_row_spacing(table, spacing)
# define markdown_gtk_table_set_col_spacing(table, spacing) \
    gtk_grid_set_column_spacing(table, spacing)
#endif

typedef struct {
  guint8 red;
  guint8 green;
  guint8 blue;
} MarkdownColor;

GtkWidget *markdown_gtk_table_new(guint rows, guint columns, gboolean homogeneous);

void markdown_gtk_table_attach(MarkdownGtkTable *table, GtkWidget *child,
  guint left_attach, guint right_attach, guint top_attach, guint bottom_attach,
  GtkAttachOptions xoptions, GtkAttachOptions yoptions);

GtkWidget *markdown_gtk_color_button_new_with_color(MarkdownColor *color);

gboolean markdown_color_parse(const gchar *spec, MarkdownColor *color);

void markdown_gtk_color_button_get_color(GtkColorButton *button, MarkdownColor *color);

#endif /* MARKDOWN_GTK_COMPAT_H_ */
