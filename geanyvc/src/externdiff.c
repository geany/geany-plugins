/*
 *      externdiff.h - Plugin to geany light IDE to work with vc
 *
 *      Copyright 2008 Yura Siamashka <yurand2@gmail.com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <string.h>

#include "geanyplugin.h"
#include "geanyvc.h"

extern GeanyFunctions *geany_functions;

const gchar *get_external_diff_viewer(void);


enum
{
	EXTERNAL_DIFF_MELD,
	EXTERNAL_DIFF_KOMPARE,
	EXTERNAL_DIFF_KDIFF3,
	EXTERNAL_DIFF_DIFFUSE,
	EXTERNAL_DIFF_TKDIFF,
	EXTERNAL_DIFF_COUNT
};

static gchar *viewers[EXTERNAL_DIFF_COUNT] = { "meld", "kompare", "kdiff3", "diffuse", "tkdiff" };

static gchar *extern_diff_viewer = NULL;
const gchar *
get_external_diff_viewer()
{
	gint i;

	if (extern_diff_viewer)
		return extern_diff_viewer;

	for (i = 0; i < EXTERNAL_DIFF_COUNT; i++)
	{
		if (g_find_program_in_path(viewers[i]))
		{
			extern_diff_viewer = viewers[i];
			return extern_diff_viewer;
		}
	}
	return NULL;
}

void
vc_external_diff(const gchar * src, const gchar * dest)
{
	gchar *argv[4] = { NULL, NULL, NULL, NULL };

	const gchar *diff = get_external_diff_viewer();
	if (!diff)
		return;

	argv[0] = (gchar *) diff;
	argv[1] = (gchar *) src;
	argv[2] = (gchar *) dest;

	utils_spawn_sync(NULL, argv, NULL,
			 G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
			 G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}
