/*
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <glib.h>

#include "geanyplugin.h"

extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;


/* Normalize a pathname. This collapses redundant separators and up-level references so that A//B, A/./B
 * and A/foo/../B all become A/B. It does not normalize the case. On Windows, it converts forward
 * slashes to backward slashes. It should be understood that this may change the meaning of the
 * path if it contains symbolic links!
 */
gchar *
normpath(const gchar * filename)
{
	gchar **v;
	gchar **p;
	gchar **out;
	gchar **pout;
	gchar *ret;

	if (!filename || strlen(filename) == 0)
		return g_strdup(".");
	v = g_strsplit_set(filename, "/\\", -1);
	if (!g_strv_length(v))
		return g_strdup(".");

	out = g_malloc0(sizeof(gchar *) * (g_strv_length(v) + 2));
	pout = out;

	if (filename[0] == '.' && strcmp(v[0], ".") == 0)
	{
		*pout = strdup(".");
		pout++;
	}
	else if (filename[0] == '/')
	{
		*pout = strdup("/");
		pout++;
	}

	for (p = v; *p; p++)
	{
		if (strcmp(*p, ".") == 0 || strcmp(*p, "") == 0)
		{
			continue;
		}
		else if (strcmp(*p, "..") == 0)
		{
			if (pout != out)
			{
				pout--;
				if (strcmp(*pout, "..") != 0)
				{
					g_free(*pout);
					*pout = NULL;
					continue;
				}
				pout++;
			}
		}
		*pout++ = g_strdup(*p);
	}

	ret = g_build_filenamev(out);

	g_strfreev(out);
	g_strfreev(v);
	return ret;
}

gchar *
get_full_path(const gchar * location, const gchar * path)
{
	gchar *dir;

	dir = g_path_get_dirname(location);
	setptr(dir, g_build_filename(dir, path, NULL));
	setptr(dir, normpath(dir));
	return dir;
}

gchar *
get_relative_path(const gchar * location, const gchar * path)
{
	gchar *dir;
	gchar *pth;
	gchar *ret = NULL;

	gint plen;
	gint dlen;

	if (!g_path_is_absolute(path))
	{
		return g_strdup(path);
	}

	dir = normpath(location);
	pth = normpath(path);

	plen = strlen(pth);
	dlen = strlen(dir);

	if (strstr(pth, dir) == pth)
	{
		if (plen > dlen)
		{
			ret = g_strdup(path + strlen(dir) + 1);
		}
		else if (plen == dlen)
		{
			ret = g_strdup(".");
		}
	}
	g_free(dir);
	g_free(pth);
	return ret;
}


#ifdef UNITTESTS
#include <check.h>

START_TEST(test_get_relative_path)
{
	gchar *path;
	path = get_relative_path("/a/b", "/a/b/c");
	fail_unless(strcmp(path, "c") == 0, "expected: \"c\", get \"%s\"\n", path);
	g_free(path);

	path = get_relative_path("/a/b", "/a/b");
	fail_unless(strcmp(path, ".") == 0, "expected: \".\", get \"%s\"\n", path);
	g_free(path);
}

END_TEST;


TCase *
utils_test_case_create()
{
	TCase *tc_utils = tcase_create("utils");
	tcase_add_test(tc_utils, test_get_relative_path);
	return tc_utils;
}


#endif
