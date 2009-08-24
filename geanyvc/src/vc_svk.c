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

extern GeanyData *geany_data;


static const gchar *SVK_CMD_DIFF_FILE[] = { "svk", "diff", ABS_FILENAME, NULL };
static const gchar *SVK_CMD_DIFF_DIR[] = { "svk", "diff", ABS_DIRNAME, NULL };
static const gchar *SVK_CMD_REVERT_FILE[] = { "svk", "revert", BASENAME, NULL };
static const gchar *SVK_CMD_REVERT_DIR[] = { "svk", "revert", "--recursive", BASE_DIRNAME, NULL };
static const gchar *SVK_CMD_STATUS[] = { "svk", "status", NULL };
static const gchar *SVK_CMD_ADD[] = { "svk", "add", BASENAME, NULL };
static const gchar *SVK_CMD_REMOVE[] = { "svk", "remove", BASENAME, NULL };
static const gchar *SVK_CMD_LOG_FILE[] = { "svk", "log", BASENAME, NULL };
static const gchar *SVK_CMD_LOG_DIR[] = { "svk", "log", ABS_DIRNAME, NULL };
static const gchar *SVK_CMD_COMMIT[] = { "svk", "commit", "-m", MESSAGE, FILE_LIST, NULL };
static const gchar *SVK_CMD_BLAME[] = { "svk", "blame", BASENAME, NULL };
static const gchar *SVK_CMD_SHOW[] = { "svk", "cat", BASENAME, NULL };
static const gchar *SVK_CMD_UPDATE[] = { "svk", "up", NULL };

static const VC_COMMAND commands[] = {
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_DIFF_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_DIFF_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_REVERT_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_BASE,
	 .command = SVK_CMD_REVERT_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_STATUS,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_ADD,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_REMOVE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_LOG_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_LOG_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_COMMIT,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_BLAME,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = SVK_CMD_SHOW,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_BASE,
	 .command = SVK_CMD_UPDATE,
	 .env = NULL,
	 .function = NULL}
};


static gchar *
get_base_dir(const gchar * path)
{
	// NOT IMPLEMENTED
	return g_path_get_dirname(path);
}

static gboolean
in_vc_svk(const gchar * filename)
{
	gint exit_code;
	gchar *argv[] = { "svk", "info", NULL, NULL };
	gchar *dir;
	gchar *base_name = NULL;
	gboolean ret = FALSE;


	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
	{
		exit_code =
			execute_custom_command(filename, (const gchar **) argv, NULL, NULL, NULL,
					       filename, NULL, NULL);

	}
	else
	{
		base_name = g_path_get_basename(filename);
		dir = g_path_get_dirname(filename);

		argv[2] = base_name;

		exit_code = execute_custom_command(dir, (const gchar **) argv, NULL, NULL, NULL,
						   dir, NULL, NULL);

		g_free(dir);
		g_free(base_name);
	}

	if (exit_code == 0)
	{
		ret = TRUE;
	}

	return ret;
}

static GSList *
get_commit_files_svk(const gchar * dir)
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
	const char *argv[] = { "svk", "status", NULL };

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

VC_RECORD VC_SVK = {
	.commands = commands,
	.program = "svk",
	.get_base_dir = get_base_dir,
	.in_vc = in_vc_svk,
	.get_commit_files = get_commit_files_svk,
};
