/*
 *  utils.c - this file is part of "codenavigation", which is
 *  part of the "geany-plugins" project.
 *
 *  Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
 *  Copyright 2014 Federico Reghenzani <federico(dot)dev(at)reghe(dot)net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"

#include <geanyplugin.h>

 /**
 * @brief	Function which returns the extension of the file path which 
 * 			is given, or NULL if it did not found any extension.
 * @param 	gchar*	the file path 
 * @return	gchar*	newly-allocated string containing the extension
 * 
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

/**
 * @brief	Copy a path and remove the extension 
 * @param 	gchar*	the file path 
 * @return	gchar*	newly-allocated string containing the filename without ext
 * 
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
	if(!str)
		return NULL;

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

/**
 * @brief	Comparison of strings, for use with g_slist_find_custom 
 * @param 	const gchar*, const gchar* 
 * @return	gint
 * 
 */
gint
compare_strings(const gchar* a, const gchar* b)
{
	return (gint)(!utils_str_equal(a, b));
}

/**
 * @brief	A PHP-like reverse strpos implementation 
 * @param 	const gchar*	haystack
 * @param   const gchar*	needle 
 * @return	gint	position or -1 if not found
 * 
 */
gint
strrpos(const gchar *haystack, const gchar *needle)
{
   char *p = g_strrstr_len(haystack, -1, needle);
   if (p)
	  return p - haystack;
   return -1;   // not found
}

