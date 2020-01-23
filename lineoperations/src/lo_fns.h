/*
 *      lo_fns.h - Line operations, remove duplicate lines, empty lines,
 *                 lines with only whitespace, sort lines.
 *
 *      Copyright 2015 Sylvan Mostert <smostert.dev@gmail.com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/



#ifndef LO_FNS_H
#define LO_FNS_H

#include <geanyplugin.h>
#include "Scintilla.h"
#include <stdlib.h>      /* qsort */
#include <string.h>


typedef gint (*lo_strcmpfns)(const gchar *str1, const gchar *str2);

/* Get sort function based on user preferences */
lo_strcmpfns
getcmpfns(void);


/* Remove Duplicate Lines, sorted */
gint
rmdupst(gchar **lines, gint num_lines, gchar *new_file);


/* Remove Duplicate Lines, ordered */
gint
rmdupln(gchar **lines, gint num_lines, gchar *new_file);


/* Remove Unique Lines */
gint
rmunqln(gchar **lines, gint num_lines, gchar *new_file);


/* Keep Unique Lines */
gint
kpunqln(gchar **lines, gint num_lines, gchar *new_file);


/* Remove Empty Lines */
gint
rmemtyln(ScintillaObject *sci, gint line_num, gint end_line_num);


/* Remove Whitespace Lines */
gint
rmwhspln(ScintillaObject *sci, gint line_num, gint end_line_num);


/* Sort Lines Ascending */
gint
sortlnsasc(gchar **lines, gint num_lines, gchar *new_file);


/* Sort Lines Descending */
gint
sortlndesc(gchar **lines, gint num_lines, gchar *new_file);


/* Remove Every Nth Line */
gint
rmnthln(ScintillaObject *sci, gint line_num, gint end_line_num);

#endif
