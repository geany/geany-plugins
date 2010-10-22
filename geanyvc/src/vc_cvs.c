/*
 *      Copyright 2007-2009 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *      Copyright 2007-2009 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
 *      Copyright 2007-2009 Yura Siamashka <yurand2@gmail.com>
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

static const gchar *CVS_CMD_DIFF_FILE[] = { "cvs", "diff", "-u", BASE_FILENAME, NULL };
static const gchar *CVS_CMD_DIFF_DIR[] = { "cvs", "diff", "-u", NULL };
static const gchar *CVS_CMD_REVERT_FILE[] = { "cvs", "update", "-C", BASE_FILENAME, NULL };
static const gchar *CVS_CMD_REVERT_DIR[] = { "cvs", "update", "-C", BASE_DIRNAME, NULL };
static const gchar *CVS_CMD_STATUS[] = { "cvs", "status", NULL };
static const gchar *CVS_CMD_ADD[] = { "cvs", "add", BASE_FILENAME, NULL };
static const gchar *CVS_CMD_REMOVE[] = { "cvs", "remove", BASE_FILENAME, NULL };
static const gchar *CVS_CMD_LOG_FILE[] = { "cvs", "log", BASE_FILENAME, NULL };
static const gchar *CVS_CMD_LOG_DIR[] = { "cvs", "log", NULL };
static const gchar *CVS_CMD_COMMIT[] = { "cvs", "commit", "-m", MESSAGE, FILE_LIST, NULL };
static const gchar *CVS_CMD_BLAME[] = { "cvs", "annotate", BASE_FILENAME, NULL };
static const gchar *CVS_CMD_SHOW[] = { "cvs", NULL };
static const gchar *CVS_CMD_UPDATE[] = { "cvs", "up", "-d", NULL };

static const VC_COMMAND commands[] = {
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_DIFF_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_DIFF_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_REVERT_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_BASE,
	 .command = CVS_CMD_REVERT_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_STATUS,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_ADD,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_REMOVE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_LOG_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_LOG_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_COMMIT,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_BLAME,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = CVS_CMD_SHOW,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_BASE,
	 .command = CVS_CMD_UPDATE,
	 .env = NULL,
	 .function = NULL}
};

static gchar *
get_base_dir(const gchar * path)
{
	gchar *test_dir;
	gchar *base;
	gchar *base_prev = NULL;

	if (g_file_test(path, G_FILE_TEST_IS_DIR))
		base = g_strdup(path);
	else
		base = g_path_get_dirname(path);

	do
	{
		test_dir = g_build_filename(base, "CVS", NULL);
		if (!g_file_test(test_dir, G_FILE_TEST_IS_DIR))
		{
			g_free(test_dir);
			break;
		}
		g_free(test_dir);
		g_free(base_prev);
		base_prev = base;
		base = g_path_get_dirname(base);
	}
	while (strcmp(base, base_prev) != 0);

	g_free(base);
	return base_prev;
}

static gboolean
in_vc_cvs(const gchar * filename)
{
	return find_dir(filename, "CVS", FALSE);
}

static GSList *
get_commit_files_cvs(const gchar * dir)
{
	enum
	{
		FIRST_CHAR,
		SKIP_SPACE,
		FILE_NAME,
	};

	gchar *txt;
	GSList *ret = NULL;
	gint pstatus = FIRST_CHAR;
	const gchar *p;
	gchar *base_name;
	const gchar *start = NULL;
	CommitItem *item;

	const gchar *status;
	gchar *filename;
	const char *argv[] = { "cvs", "-nq", "update", NULL };

	execute_custom_command(dir, argv, NULL, &txt, NULL, dir, NULL, NULL);
	if (!NZV(txt))
		return NULL;
	p = txt;

	while (*p)
	{
		if (*p == '\r')
		{
		}
		else if (pstatus == FIRST_CHAR)
		{
			status = NULL;
			if (*p == '?')
				status = FILE_STATUS_UNKNOWN;
			else if (*p == 'M')
				status = FILE_STATUS_MODIFIED;
			else if (*p == 'D')
				status = FILE_STATUS_DELETED;
			else if (*p == 'A')
				status = FILE_STATUS_ADDED;

			if (!status || *(p + 1) != ' ')
			{
				// skip unknown status line
				while (*p)
				{
					p++;
					if (*p == '\n')
					{
						p++;
						break;
					}
				}
				pstatus = FIRST_CHAR;
				continue;
			}
			pstatus = SKIP_SPACE;
		}
		else if (pstatus == SKIP_SPACE)
		{
			if (*p == ' ' || *p == '\t')
			{
			}
			else
			{
				start = p;
				pstatus = FILE_NAME;
			}
		}
		else if (pstatus == FILE_NAME)
		{
			if (*p == '\n')
			{
				if (status != FILE_STATUS_UNKNOWN)
				{
					base_name = g_malloc0(p - start + 1);
					memcpy(base_name, start, p - start);
					filename = g_build_filename(dir, base_name, NULL);
					g_free(base_name);
					item = g_new(CommitItem, 1);
					item->status = status;
					item->path = filename;
					ret = g_slist_append(ret, item);
				}
				pstatus = FIRST_CHAR;
			}
		}
		p++;
	}
	g_free(txt);
	return ret;
}

VC_RECORD VC_CVS = {
	.commands = commands,
	.program = "cvs",
	.get_base_dir = get_base_dir,
	.in_vc = in_vc_cvs,
	.get_commit_files = get_commit_files_cvs,
};
