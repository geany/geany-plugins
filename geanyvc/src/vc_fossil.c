/*
 *      Copyright 2020 Artur Shepilko <nomadbyte@gmail.com>
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
#include <geanyplugin.h>
#include "geanyvc.h"

extern GeanyData *geany_data;

#define FOSSIL_CLIENT "fossil"
#define FOSSIL_CMD_LOG_WIDTH "100"  /* 0:single-line */
#define FOSSIL_CMD_LOG_NUM   "300"

static const gchar *FOSSIL_CMD_DIFF_FILE[] = { FOSSIL_CLIENT, "diff", BASENAME, NULL };
static const gchar *FOSSIL_CMD_DIFF_DIR[] = { FOSSIL_CLIENT, "diff", BASE_DIRNAME, NULL };
static const gchar *FOSSIL_CMD_REVERT_FILE[] = { FOSSIL_CLIENT, "revert", BASENAME, NULL };
static const gchar *FOSSIL_CMD_REVERT_DIR[] = { FOSSIL_CLIENT, "revert", BASE_DIRNAME, NULL };
static const gchar *FOSSIL_CMD_STATUS[] = { FOSSIL_CLIENT, "status", NULL };
static const gchar *FOSSIL_CMD_ADD[] = { FOSSIL_CLIENT, "add", BASENAME, NULL };
static const gchar *FOSSIL_CMD_REMOVE[] = { FOSSIL_CLIENT, "delete", BASENAME, NULL };
static const gchar *FOSSIL_CMD_LOG_FILE[] =
	{ FOSSIL_CLIENT, "timeline", "ancestors", "current", "--path", BASENAME,
		"-W", FOSSIL_CMD_LOG_WIDTH, "-n", FOSSIL_CMD_LOG_NUM, NULL };
static const gchar *FOSSIL_CMD_LOG_DIR[] =
	{ FOSSIL_CLIENT, "timeline", "ancestors", "current", "--path", BASE_DIRNAME,
		"-W", FOSSIL_CMD_LOG_WIDTH, "-n", FOSSIL_CMD_LOG_NUM, NULL };
static const gchar *FOSSIL_CMD_COMMIT[] =
	{ FOSSIL_CLIENT, "commit", "-m", MESSAGE, FILE_LIST, NULL };
static const gchar *FOSSIL_CMD_BLAME[] = { FOSSIL_CLIENT, "blame", BASENAME, NULL };
static const gchar *FOSSIL_CMD_UPDATE[] ={ FOSSIL_CLIENT, "update", NULL };
static const gchar *FOSSIL_CMD_SHOW[] = { FOSSIL_CLIENT, "cat", BASENAME, NULL };

static gchar *
parse_fossil_info(const gchar * txt, const gchar * fld)
{
	gchar *val = NULL;
	gchar *start, *end;
	gint len;

	start = strstr(txt, fld);
	if (!start) return NULL;

	start += strlen(fld);
	while (*start == ' ' || *start == '\t') ++start;

	end = strchr(start, '\n');
	len = ( end ? end - start : strlen(start) );

	if (strcmp(fld, "local-root:") == 0)
	{
		if (!len) return NULL;
	}
	else if (strcmp(fld, "checkout:") == 0
		     || strcmp(fld, "parent:") == 0)
	{
		/* extract id from string:
		 * "id date"
		 */
		end = strstr(start, " ");
		if (!end) return NULL;

		for (--end; *end == ' ' || *end == '\t'; --end) {}
		len = end - start + 1;
	}
	else if (strcmp(fld, "comment:") == 0)
	{
		/* extract comment text from string:
		 * "comment text (user: username)"
		 */
		end = g_strrstr(start, "(user:");
		if (!end) return NULL;

		for (--end; *end == ' ' || *end == '\t'; --end) {}
		len = end - start + 1;
	}
	else if (strcmp(fld, "user:") == 0)
	{
		/* extract username from string:
		 * "username)"
		 */
		end = g_strrstr(start, ")");
		if (!end) return NULL;

		--end;
		len = end - start + 1;
	}

	val = g_malloc0(len + 1);
	memcpy(val, start, len);

	return val;
}

static gchar *
get_base_dir(const gchar * path)
{
	gchar *base_dir = NULL;
	const gchar *argv[] = { FOSSIL_CLIENT, "info", NULL };
	gchar *dir = NULL;
	gchar *filename = NULL;
	gchar *std_out = NULL;
	gchar *std_err = NULL;

	if (g_file_test(path, G_FILE_TEST_IS_DIR))
		dir = g_strdup(path);
	else
		dir = g_path_get_dirname(path);

	execute_custom_command(dir, (const gchar **) argv, NULL, &std_out, &std_err,
			       dir, NULL, NULL);
    g_free(dir);
    if (!std_out) return NULL;

    dir = parse_fossil_info(std_out, "local-root:");
	g_free(std_out);
	if (!dir) return NULL;

	filename = g_build_filename(dir, ".", NULL); /* in case of a trailing slash */
	base_dir = g_path_get_dirname(filename);

	g_free(filename);
	g_free(dir);

	return base_dir;
}

static gboolean
in_vc_fossil(const gchar * filename)
{
	const gchar *argv[] = { FOSSIL_CLIENT, "ls", NULL, NULL };
	gchar *dir;
	gchar *base_name;
	gboolean ret = FALSE;
	gchar *std_out;
	gchar *std_err;

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
	{
		gchar *base_dir = get_base_dir(filename);

		if (!base_dir) return FALSE;

		g_free(base_dir);
		return TRUE;
	}

	dir = g_path_get_dirname(filename);
	base_name = g_path_get_basename(filename);
	argv[2] = base_name;

	execute_custom_command(dir, (const gchar **) argv, NULL, &std_out, &std_err,
			       dir, NULL, NULL);

	if (!EMPTY(std_out))
	{
		ret = TRUE;
		g_free(std_out);
	}

	g_free(base_name);
	g_free(dir);

	return ret;
}

static GSList *
parse_fossil_status(GSList * lst, const gchar * base_dir, const gchar * txt, const gchar * s_out,
		 const gchar * status)
{
	gchar *start, *end;
	gchar *base_name;
	gchar *filename;
	gboolean got_error = FALSE;

	for (start = strstr(txt, s_out); start; start = strstr(start, s_out))
	{
		gint len = 0;
		CommitItem *item;

		start += strlen(s_out);
		end = strchr(start, '\n');
		got_error = (!end);
		if (got_error) break;

		start = strchr(start,' '); /* allow partial match; jump to separator  */
		got_error = (!start || start > end);
		if (got_error) break;

		while (*start == ' ' || *start == '\t') ++start;
		got_error = (!*start);
		if (got_error) break;

		len = end - start;
		base_name = g_malloc0(len + 1);
		memcpy(base_name, start, len);
		filename = g_build_filename(base_dir, base_name, NULL);
		g_free(base_name);

		item = g_new(CommitItem, 1);
		item->status = status;
		item->path = filename;

		lst = g_slist_append(lst, item);
	}

	if (got_error)
	{
		GSList *e;
		for (e = lst; e ; e = g_slist_next(e))
		{
			CommitItem *item = (CommitItem *) (e->data);
			g_free(item->path);
			g_free(item);
		}
		g_slist_free(lst);
		lst = NULL;
	}

	return lst;
}

static GSList *
get_commit_files_fossil(const gchar * file)
{
	const gchar *argv[] = { FOSSIL_CLIENT, "status", NULL };
	gchar *std_out = NULL;
	gchar *base_dir = get_base_dir(file);
	GSList *ret = NULL;

	g_return_val_if_fail(base_dir, NULL);
	if (!base_dir) return NULL;

	execute_custom_command(base_dir, (const gchar **) argv, (const gchar **) NULL,
			       &std_out, NULL, base_dir, NULL, NULL);
	g_return_val_if_fail(std_out, NULL);


	ret = parse_fossil_status(ret, base_dir, std_out, "EDITED", FILE_STATUS_MODIFIED);
	ret = parse_fossil_status(ret, base_dir, std_out, "UPDATED", FILE_STATUS_MODIFIED);
	ret = parse_fossil_status(ret, base_dir, std_out, "ADDED", FILE_STATUS_ADDED);
	ret = parse_fossil_status(ret, base_dir, std_out, "DELETED", FILE_STATUS_DELETED);

	g_free(std_out);
	g_free(base_dir);

	return ret;
}

static gint
fossil_revert_dir(gchar ** std_out, gchar ** std_err, const gchar * path,
	 GSList * list, const gchar * message)
{
	gchar *base_dir = get_base_dir(path);
    const char **argv = (const gchar **)FOSSIL_CMD_REVERT_DIR;
	const gchar *base_argv[] = { FOSSIL_CLIENT, "revert", NULL, NULL };
	gint ret;

	g_return_val_if_fail(base_dir, -1);
	if (!base_dir) return -1;

	if ( g_strcmp0(path, base_dir) == 0 )
		argv = (const gchar **)base_argv;

	ret = execute_custom_command(base_dir, argv, (const gchar **) NULL,
						std_out, std_err, path, list, message);
	g_free(base_dir);
	return ret;
}

static gint
fossil_status_extra(gchar ** std_out, gchar ** std_err, const gchar * path,
	 GSList * list, const gchar * message)
{
	gchar *base_dir = get_base_dir(path);
	gint ret = 0;

	g_return_val_if_fail(base_dir, -1);
	if (!base_dir) return -1;

	ret = execute_custom_command(base_dir, FOSSIL_CMD_STATUS, (const gchar **) NULL,
						std_out, std_err, path, list, message);

	if (ret)
	{
		g_free(base_dir);
		return ret;
	}

	if (!ret)
	{
		const gchar *argv[] = { FOSSIL_CLIENT, "changes", "--extra", "--classify", NULL };
		gchar *std_out1 = NULL;
		gchar *std_err1 = NULL;

		if (std_out)
		{
			std_out1 = *std_out;
			*std_out = NULL;
		}
		if (std_err)
		{
			std_err1 = *std_err;
			*std_err = NULL;
		}

		ret = execute_custom_command(base_dir, (const gchar **) argv, (const gchar **) NULL,
							std_out, std_err, path, list, message);

		if (std_out1)
		{
			*std_out = g_strconcat(std_out1, *std_out, NULL);
			g_free(std_out1);
		}
		if (std_err1)
		{
			*std_err = g_strconcat(std_err1, *std_err, NULL);
			g_free(std_err1);
		}
	}

	g_free(base_dir);
	return ret;
}

static const VC_COMMAND commands[VC_COMMAND_COUNT] = {
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_DIFF_FILE,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_DIFF_DIR,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_REVERT_FILE,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_BASE,
		NULL,
		NULL,
		fossil_revert_dir},
	{
		VC_COMMAND_STARTDIR_FILE,
		NULL,
		NULL,
		fossil_status_extra},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_ADD,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_REMOVE,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_LOG_FILE,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_LOG_DIR,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_COMMIT,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_BLAME,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_FILE,
		FOSSIL_CMD_SHOW,
		NULL,
		NULL},
	{
		VC_COMMAND_STARTDIR_BASE,
		FOSSIL_CMD_UPDATE,
		NULL,
		NULL}
};

VC_RECORD VC_FOSSIL = {
	commands,
	FOSSIL_CLIENT,
	get_base_dir,
	in_vc_fossil,
	get_commit_files_fossil,
};


#ifdef UNITTESTS
#include <check.h>

START_TEST(test_parse_fossil_info)
{
	const char *txt =
"\
project-name: Test\n\
repository:   /home/developer/project/../project.fossil\n\
local-root:   /home/developer/project/\n\
config-db:    /home/developer/.fossil\n\
project-code: f9e47309491679be1a7f37f11d9af42411c5bc5e\n\
checkout:     e84c78d8eef3f7eb9b868a5766971ef8bc8d26ed 2020-03-21 02:42:21 UTC\n\
parent:       1667f964039e1e41f06768c4721abadd704bc19c 2020-03-20 17:08:38 UTC\n\
tags:         trunk\n\
comment:      Test changes (user: developer)\n\
check-ins:    10\n\
";
	gchar *val;

	val = parse_fossil_info(txt, "unsupported-field:");
	fail_unless(val == NULL,
		"expected: unsupported-field: NULL, got: \"%s\"\n", val);
	g_free(val);

	val = parse_fossil_info(txt, "local-root:");
	fail_unless(g_strcmp0(val, "/home/developer/project/") == 0,
		"expected: local-root: \"/home/developer/project/\", got: \"%s\"\n", val);
	g_free(val);

	val = parse_fossil_info(txt, "checkout:");
	fail_unless(g_strcmp0(val, "e84c78d8eef3f7eb9b868a5766971ef8bc8d26ed") == 0,
		"expected: checkout: \"e84c78d8eef3f7eb9b868a5766971ef8bc8d26ed\", got: \"%s\"\n", val);
	g_free(val);

	val = parse_fossil_info(txt, "comment:");
	fail_unless(g_strcmp0(val, "Test changes") == 0,
		"expected: comment: \"Test changes\", got: \"%s\"\n", val);
	g_free(val);

	val = parse_fossil_info(txt, "user:");
	fail_unless(g_strcmp0(val, "developer") == 0,
		"expected: user: \"developer\", got: \"%s\"\n", val);
	g_free(val);
}

END_TEST;

START_TEST(test_parse_fossil_status)
{
	const char *txt =
"\
repository:   /home/developer/project/../project.fossil\n\
local-root:   /home/developer/project/\n\
config-db:    /home/developer/.fossil\n\
checkout:     e84c78d8eef3f7eb9b868a5766971ef8bc8d26ed 2020-03-21 02:42:21 UTC\n\
parent:       1667f964039e1e41f06768c4721abadd704bc19c 2020-03-20 17:08:38 UTC\n\
tags:         trunk\n\
comment:      Test changes (user: developer)\n\
ADDED      added.txt\n\
EDITED     edited1.txt\n\
EDITED     edited2.txt\n\
DELETED    deleted.txt\n\
UPDATED_BY_MERGE     updated.txt\n\
";
	GSList *list = NULL;
	const gchar *base_dir = "/home/developer/project";
	CommitItem *item = NULL;

 	list = parse_fossil_status(list, base_dir, txt, "status-unsupported", FILE_STATUS_UNKNOWN);
	fail_unless(list == NULL,
		"expected: status-unsupported-list: NULL, got: \"%p\"\n", list);
	g_slist_free(list);


    /* ADDED */
 	list = parse_fossil_status(list, base_dir, txt, "ADDED", FILE_STATUS_ADDED);
	fail_unless(list && list->data,
		"expected: ADDED-list: not-NULL, got: \"%p\"\n", list);
	fail_unless(g_slist_length(list) == 1,
		"expected: ADDED-list: size:1, got: %d\n", g_slist_length(list));

	item = (CommitItem *) (list->data);
	fail_unless(g_strcmp0(item->path, "/home/developer/project" "/" "added.txt") == 0
				&& item->status == FILE_STATUS_ADDED,
		"expected: ADDED-list: item[0]: {\"%s\", %d}, got: {\"%s\", %d}\n",
		"/home/developer/project" "/" "added.txt", FILE_STATUS_ADDED,
		item->path, item->status);
	g_free(item->path);
	g_free(item);
	g_slist_free(list);


    /* EDITED */
 	list = NULL;
 	list = parse_fossil_status(list, base_dir, txt, "EDITED", FILE_STATUS_MODIFIED);
	fail_unless(g_slist_length(list) == 2,
		"expected: EDITED-list: size:2, got: %d\n", g_slist_length(list));

	item = (CommitItem *) (list->data);
	fail_unless(g_strcmp0(item->path, "/home/developer/project" "/" "edited1.txt") == 0
				&& item->status == FILE_STATUS_MODIFIED,
		"expected: EDITED-list: item[0]: {\"%s\", %d}, got: {\"%s\", %d}\n",
		"/home/developer/project" "/" "edited1.txt", FILE_STATUS_MODIFIED,
		item->path, item->status);
	g_free(item->path);
	g_free(item);

	item = (CommitItem *) (g_slist_next(list)->data);
	fail_unless(g_strcmp0(item->path, "/home/developer/project" "/" "edited2.txt") == 0
				&& item->status == FILE_STATUS_MODIFIED,
		"expected: EDITED-list: item[1]: {\"%s\", %d}, got: {\"%s\", %d}\n",
		"/home/developer/project" "/" "edited2.txt", FILE_STATUS_MODIFIED,
		item->path, item->status);
	g_free(item->path);
	g_free(item);
	g_slist_free(list);


    /* UPDATED, partial match */
	list = NULL;
 	list = parse_fossil_status(list, base_dir, txt, "UPDATED", FILE_STATUS_MODIFIED);
	fail_unless(list && list->data,
		"expected: UPDATED-list: not-NULL, got: \"%p\"\n", list);
	fail_unless(g_slist_length(list) == 1,
		"expected: UPDATED-list: size:1, got: %d\n", g_slist_length(list));

	item = (CommitItem *) (list->data);
	fail_unless(g_strcmp0(item->path, "/home/developer/project" "/" "updated.txt") == 0
				&& item->status == FILE_STATUS_MODIFIED,
		"expected: UPDATED-list: item[0]: {\"%s\", %d}, got: {\"%s\", %d}\n",
		"/home/developer/project" "/" "updated.txt", FILE_STATUS_MODIFIED,
		item->path, item->status);
	g_free(item->path);
	g_free(item);
	g_slist_free(list);


    /* DELETED */
	list = NULL;
 	list = parse_fossil_status(list, base_dir, txt, "DELETED", FILE_STATUS_DELETED);
	fail_unless(g_slist_length(list) == 1,
		"expected: DELETED-list: size:1, got: %d\n", g_slist_length(list));

	item = (CommitItem *) (list->data);
	fail_unless(g_strcmp0(item->path, "/home/developer/project" "/" "deleted.txt") == 0
				&& item->status == FILE_STATUS_DELETED,
		"expected: DELETED-list: item[0]: {\"%s\", %d}, got: {\"%s\", %d}\n",
		"/home/developer/project" "/" "deleted.txt", FILE_STATUS_DELETED,
		item->path, item->status);
	g_free(item->path);
	g_free(item);
	g_slist_free(list);
}

END_TEST;


TCase *
vc_fossil_test_case_create(void)
{
	TCase *tc_vc_fossil = tcase_create("vc_fossil");
	tcase_add_test(tc_vc_fossil, test_parse_fossil_info);
	tcase_add_test(tc_vc_fossil, test_parse_fossil_status);
	return tc_vc_fossil;
}


#endif
