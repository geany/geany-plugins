/*
 *      utils.c - this file is part of "codenavigation", which is
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

#include "utils.h"

/* Function which returns a newly-allocated string containing the
 * extension of the file path which is given, or NULL if it did not found any extension.
 */
gchar*
get_extension(gchar* path)
{
	gchar* extension = NULL;
	gchar* pc = path;
	while(*pc != '\0')
	{
		if(*pc == '.')
			extension = pc+1;
		pc++;
	}

	if(extension == NULL || (*extension) == '\0')
		return NULL;
	else
		return g_strdup(extension);
}

/* Copy a path and remove the extension
 */
gchar*
copy_and_remove_extension(gchar* path)
{
	gchar* str = NULL;
	gchar* pc = NULL;
	gchar* dot_pos = NULL;

	if(path == NULL || path[0] == '\0')
		return NULL;

	str = g_strdup(path);
	pc = str;
	while(*pc != '\0')
	{
		if(*pc == '.')
		{
			dot_pos = pc;
			break;
		}
		pc++;
	}

	if(dot_pos != NULL)
		*dot_pos = '\0';

	return str;
}

/* Comparison of strings, for use with g_slist_find_custom */
gint
compare_strings(const gchar* a, const gchar* b)
{
	return (gint)(!utils_str_equal(a, b));
}
