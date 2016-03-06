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
#include <geanyplugin.h>
#include "geanyvc.h"

#ifdef G_OS_WIN32
#include <shlobj.h>
#endif


enum
{
	EXTERNAL_DIFF_MELD,
	EXTERNAL_DIFF_KOMPARE,
	EXTERNAL_DIFF_KDIFF3,
	EXTERNAL_DIFF_DIFFUSE,
	EXTERNAL_DIFF_TKDIFF,
	EXTERNAL_DIFF_WINMERGE,
	EXTERNAL_DIFF_COUNT
};


static const gchar *viewers[EXTERNAL_DIFF_COUNT] = { "Meld/meld", "kompare", "kdiff3", "diffuse", "tkdiff", "WinMerge/WinMergeU" };

static gchar *extern_diff_viewer = NULL;

void external_diff_viewer_init(void)
{
	gint i;

	for (i = 0; i < EXTERNAL_DIFF_COUNT; i++)
	{
		gchar *filename = g_path_get_basename(viewers[i]);
		gchar *path = g_find_program_in_path(filename);
		g_free(filename);
		if (path)
		{
			extern_diff_viewer = path;
			return;
		}
	}
#ifdef G_OS_WIN32
	TCHAR szPathProgramFiles[MAX_PATH];
	TCHAR szPathProgramFiles86[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, 0, szPathProgramFiles);
	SHGetFolderPath(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, szPathProgramFiles86);

	for (i = 0; i < EXTERNAL_DIFF_COUNT; i++)
	{
		gchar *filename = g_build_filename(szPathProgramFiles, viewers[i], NULL);
		gchar *path = g_find_program_in_path(filename);
		g_free(filename);
		if (path)
		{
			extern_diff_viewer = path;
			return;
		}
	}
	for (i = 0; i < EXTERNAL_DIFF_COUNT; i++)
	{
		gchar *filename = g_build_filename(szPathProgramFiles86, viewers[i], NULL);
		gchar *path = g_find_program_in_path(filename);
		g_free(filename);
		if (path)
		{
			extern_diff_viewer = path;
			return;
		}
	}
#endif
}

void external_diff_viewer_deinit(void)
{
	if (extern_diff_viewer)
	{
		g_free(extern_diff_viewer);
		extern_diff_viewer = NULL;
	}
}

const gchar *
get_external_diff_viewer(void)
{
	if (extern_diff_viewer)
		return extern_diff_viewer;
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

	g_spawn_sync(NULL, argv, NULL,
			 G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
			 G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}
