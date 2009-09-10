/*
 *  geanyprj - Alternative project support for geany light IDE.
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
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

#include "geanyprj.h"

gchar *
find_file_path(const gchar * dir, const gchar * filename)
{
	gboolean ret = FALSE;
	gchar *base;
	gchar *gitdir = NULL;
	gchar *base_prev = g_strdup(":");

	base = g_strdup(dir);

	while (strcmp(base, base_prev) != 0)
	{
		gitdir = g_build_filename(base, filename, NULL);
		ret = g_file_test(gitdir, G_FILE_TEST_IS_REGULAR);
		if (ret)
		{
			g_free(base_prev);
			g_free(base);
			return gitdir;
		}
		g_free(gitdir);
		g_free(base_prev);
		base_prev = base;
		base = g_path_get_dirname(base);
	}

	g_free(base_prev);
	g_free(base);
	return NULL;
}

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
	gint plen;
	gint dlen;

	if (!g_path_is_absolute(path))
	{
		return g_strdup(path);
	}

	dir = g_path_get_dirname(location);
	setptr(dir, normpath(dir));

	plen = strlen(path);
	dlen = strlen(dir);

	if (strstr(path, dir) == path)
	{
		if (plen > dlen)
		{
			setptr(dir, g_strdup(path + strlen(dir) + 1));
			return dir;
		}
		else if (plen == dlen)
		{
			return g_strdup("./");
		}
	}
	g_free(dir);
	return NULL;
}

void
save_config(GKeyFile * config, const gchar * path)
{
	gchar *data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(path, data);
	g_free(data);
}

gint
config_length(GKeyFile * config, const gchar * section, const gchar * name)
{
	gchar *key;
	gint i = 0;

	key = g_strdup_printf("%s%d", name, i);
	while (g_key_file_has_key(config, section, key, NULL))
	{
		i++;
		g_free(key);
		key = g_strdup_printf("%s%d", name, i);
	}
	g_free(key);
	return i;
}


/* Gets a list of files from the specified directory.
 * Locale encoding is expected for path and used for the file list.
 * The list and the data in the list should be freed after use.
 * Returns: The list or NULL if no files found.
 * length will point to the number of non-NULL data items in the list, unless NULL.
 * error is the location for storing a possible error, or NULL. */
GSList *
get_file_list(const gchar * path, guint * length, gboolean(*func) (const gchar *), GError ** error)
{
	GSList *list = NULL;
	guint len = 0;
	GDir *dir;
	gchar *filename;
	gchar *abs_path;

	if (error)
		*error = NULL;
	if (length)
		*length = 0;
	g_return_val_if_fail(path != NULL, NULL);

	if (g_path_is_absolute(path))
	{
		abs_path = g_strdup(path);
	}
	else
	{
		abs_path = g_get_current_dir();
		setptr(abs_path, g_build_filename(abs_path, path, NULL));
	}
	if (!g_file_test(abs_path, G_FILE_TEST_IS_DIR))
	{
		g_free(abs_path);
		return NULL;
	}

	dir = g_dir_open(abs_path, 0, error);
	if (dir == NULL)
	{
		g_free(abs_path);
		return NULL;
	}

	while (1)
	{
		const gchar *name = g_dir_read_name(dir);
		if (name == NULL)
			break;

		if (name[0] == '.')
			continue;

		filename = g_build_filename(abs_path, name, NULL);

		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		{
			guint l;
			GSList *lst = get_file_list(filename, &l, func, NULL);
			g_free(filename);
			if (!lst)
				continue;
			list = g_slist_concat(list, lst);
			len += l;
		}
		else if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
		{
			if (!func || func(filename))
			{
				list = g_slist_prepend(list, filename);
				len++;
			}
			else
			{
				g_free(filename);
			}
		}
	}
	g_dir_close(dir);
	g_free(abs_path);

	if (length)
		*length = len;
	return list;
}

