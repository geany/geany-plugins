/*
 *      Copyright 2007-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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


static const gchar *HG_CMD_DIFF_FILE[] = { "hg", "diff", ABS_FILENAME, NULL };
static const gchar *HG_CMD_DIFF_DIR[] = { "hg", "diff", ABS_DIRNAME, NULL };
static const gchar *HG_CMD_REVERT_FILE[] = { "hg", "revert", BASENAME, NULL };
static const gchar *HG_CMD_REVERT_DIR[] = { "hg", "revert", BASE_DIRNAME, NULL };
static const gchar *HG_CMD_STATUS[] = { "hg", "status", NULL };
static const gchar *HG_CMD_ADD[] = { "hg", "add", BASENAME, NULL };
static const gchar *HG_CMD_REMOVE[] = { "hg", "remove", BASENAME, NULL };
static const gchar *HG_CMD_LOG_FILE[] = { "hg", "log", BASENAME, NULL };
static const gchar *HG_CMD_LOG_DIR[] = { "hg", "log", ABS_DIRNAME, NULL };
static const gchar *HG_CMD_COMMIT[] = { "hg", "commit", "-m", MESSAGE, FILE_LIST, NULL };
static const gchar *HG_CMD_BLAME[] = { "hg", "annotate", BASENAME, NULL };
static const gchar *HG_CMD_SHOW[] = { "hg", "cat", BASENAME, NULL };
static const gchar *HG_CMD_UPDATE[] = { "hg", "pull", CMD_SEPARATOR, "hg", "update", NULL };

static const VC_COMMAND commands[] = {
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_DIFF_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_DIFF_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_REVERT_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_BASE,
	 .command = HG_CMD_REVERT_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_STATUS,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_ADD,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_REMOVE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_LOG_FILE,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_LOG_DIR,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_COMMIT,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_BLAME,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_FILE,
	 .command = HG_CMD_SHOW,
	 .env = NULL,
	 .function = NULL},
	{
	 .startdir = VC_COMMAND_STARTDIR_BASE,
	 .command = HG_CMD_UPDATE,
	 .env = NULL,
	 .function = NULL}
};

static gchar *
get_base_dir(const gchar * path)
{
	return find_subdir_path(path, ".hg");
}

static gboolean
in_vc_hg(const gchar * filename)
{
	gint exit_code;
	gchar *argv[] = { "hg", "status", "-mac", NULL, NULL };
	gchar *dir;
	gchar *base_name;
	gboolean ret = FALSE;
	gchar *std_output;

	if (!find_dir(filename, ".hg", TRUE))
		return FALSE;

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		return TRUE;

	dir = g_path_get_dirname(filename);
	base_name = g_path_get_basename(filename);
	argv[3] = base_name;

	exit_code = execute_custom_command(dir, (const gchar **) argv, NULL, &std_output, NULL,
					   dir, NULL, NULL);
	if (NZV(std_output))
	{
		ret = TRUE;
		g_free(std_output);
	}

	g_free(base_name);
	g_free(dir);

	return ret;
}

static GSList *
get_commit_files_hg(const gchar * dir)
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
	gchar *base_dir = find_subdir_path(dir, ".hg");
	const gchar *start = NULL;
	CommitItem *item;

	const gchar *status;
	gchar *filename;
	const char *argv[] = { "hg", "status", NULL };

	g_return_val_if_fail(base_dir, NULL);

	execute_custom_command(base_dir, argv, NULL, &txt, NULL, base_dir, NULL, NULL);
	if (!NZV(txt))
	{
		g_free(base_dir);
		g_free(txt);
		return NULL;
	}
	p = txt;

	while (*p)
	{
		if (*p == '\r')
		{
		}
		else if (pstatus == FIRST_CHAR)
		{
			if (*p == 'A')
				status = FILE_STATUS_ADDED;
			else if (*p == 'R')
				status = FILE_STATUS_DELETED;
			else if (*p == 'M')
				status = FILE_STATUS_MODIFIED;
			else if (*p == '?')
				status = FILE_STATUS_UNKNOWN;
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
					filename = g_build_filename(base_dir, base_name, NULL);
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
	g_free(base_dir);
	return ret;
}

VC_RECORD VC_HG = {
	.commands = commands,
	.program = "hg",
	.get_base_dir = get_base_dir,
	.in_vc = in_vc_hg,
	.get_commit_files = get_commit_files_hg,
};
