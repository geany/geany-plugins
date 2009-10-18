/*
 *      utils.h - this file is part of "codenavigation", which is
 *      part of the "geany-plugins" project.
 *
 *      Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
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

#ifndef UTILS_H
#define UTILS_H

#include "codenavigation.h"

/* Function which returns a newly-allocated string containing the
 * extension of the file path which is given, or NULL if it did not found any extension.
 */
gchar*
get_extension(gchar* path);

/* Copy a path and remove the extension
 */
gchar*
copy_and_remove_extension(gchar* path);

/* Comparison of strings, for use with g_slist_find_custom */
gint
compare_strings(const gchar* a, const gchar* b);

#endif /* UTILS_H */
