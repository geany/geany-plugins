/*
 *      latexutils.h
 *
 *      Copyright 2009-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */
#ifndef LATEXUTILS_H
#define LATEXUTILS_H

#include "geanylatex.h"

gchar **glatex_read_file_in_array(const gchar *filename);
void glatex_usepackage(const gchar *pkg, const gchar *options);
void glatex_enter_key_pressed_in_entry(G_GNUC_UNUSED GtkWidget *widget, gpointer dialog);
void glatex_insert_string(const gchar *string, gboolean reset_position);
void glatex_replace_special_character(void);

#endif
