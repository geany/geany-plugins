/*
 *      utils.c
 *
 *      Copyright 2009 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include "latexutils.h"
#include "document.h"

gchar **geanylatex_read_file_in_array(const gchar *filename)
{
	gchar **result = NULL;
	gchar *data;

	if (filename == NULL) return NULL;

	g_file_get_contents(filename, &data, NULL, NULL);

	if (data != NULL)
	{
		result = g_strsplit_set(data, "\r\n", -1);
	}

	g_free(data);

	return result;
}

const gchar *glatex_get_aux_file()
{
	GeanyDocument *doc = NULL;
	gchar *locale_filename = NULL;
	GString *tmp = NULL;

	doc = document_get_current();

	if (doc != NULL)
	{
		if (doc->file_name != NULL)
		{
			locale_filename = utils_get_locale_from_utf8(doc->file_name);
			tmp = g_string_new(locale_filename);
			utils_string_replace_all(tmp, ".tex", ".aux");

			return g_string_free(tmp, FALSE);
		}
	}

	return NULL;
}

