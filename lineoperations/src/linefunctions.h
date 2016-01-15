/*
 *      linefunctions.h - Line operations, remove duplicate lines, empty lines, lines with only whitespace, sort lines.
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



#ifndef LINEFUNCTIONS_H
#define LINEFUNCTIONS_H

#include "geanyplugin.h"
#include "Scintilla.h"
#include <stdlib.h>      /* qsort */
#include <string.h>


/* Remove Duplicate Lines, sorted */
void rmdupst(GeanyDocument *doc);


/* Remove Duplicate Lines, ordered */
void rmdupln(GeanyDocument *doc);


/* Remove Unique Lines */
void rmunqln(GeanyDocument *doc);


/* Remove Empty Lines */
void rmemtyln(GeanyDocument *doc);


/* Remove Whitespace Lines */
void rmwhspln(GeanyDocument *doc);


/* Sort Lines Ascending and Descending */
void sortlines(GeanyDocument *doc, gboolean asc);

#endif
