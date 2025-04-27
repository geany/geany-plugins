/*
 *      geanyvc.c - Plugin to geany light IDE to work with vc
 *
 *      Copyright 2007-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *      Copyright 2007-2009 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
 *      Copyright 2007-2009 Yura Siamashka <yurand2@gmail.com>
 *      Copyright 2020 Artur Shepilko <nomadbyte@gmail.com>
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

/* VC plugin */
/* This plugin allows to work with cvs/svn/git/bzr/fossil inside geany light IDE. */

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "geanyvc.h"
#include "SciLexer.h"

#ifdef USE_GTKSPELL
#include <gtkspell/gtkspell.h>
#endif

GeanyData *geany_data;
GeanyPlugin     *geany_plugin;


PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("GeanyVC"),
	_("Interface to different Version Control systems."
	  "\nThis plugins is currently not developed actively. "
	  "Would you like to help by contributing to this plugin?"),
	VERSION,
	"Yura Siamashka <yurand2@gmail.com>,\nFrank Lanitz <frank@frank.uvena.de>")

/* Some global variables */
static gboolean set_changed_flag;
static gboolean set_add_confirmation;
static gboolean set_maximize_commit_dialog;
static gboolean set_external_diff;
static gboolean set_editor_menu_entries;
static gboolean set_menubar_entry;
static gint commit_dialog_width = 0;
static gint commit_dialog_height = 0;
static GSList *commit_message_history = NULL;

static gchar *config_file;

static gboolean enable_cvs;
static gboolean enable_git;
static gboolean enable_fossil;
static gboolean enable_svn;
static gboolean enable_svk;
static gboolean enable_bzr;
static gboolean enable_hg;

#ifdef USE_GTKSPELL
static gchar *lang;
#endif

static GSList *VC = NULL;

/* The addresses of these strings act as enums, their contents are not used. */
/* absolute path dirname of file */
const gchar ABS_DIRNAME[] = "*ABS_DIRNAME*";
/* absolute path filename of file */
const gchar ABS_FILENAME[] = "*ABS_FILENAME*";

/* path to directory from base vc directory */
const gchar BASE_DIRNAME[] = "*BASE_DIRNAME*";
/* path to file from base vc directory */
const gchar BASE_FILENAME[] = "*BASE_FILENAME*";

/* basename of file */
const gchar BASENAME[] = "*BASENAME*";
/* list with absolute file names*/
const gchar FILE_LIST[] = "*FILE_LIST*";
/* message */
const gchar MESSAGE[] = "*MESSAGE*";


/* this string is used when action require to run several commands */
const gchar CMD_SEPARATOR[] = "*CMD-SEPARATOR*";
const gchar CMD_FUNCTION[] = "*CUSTOM_FUNCTION*";

/* commit status */
const gchar FILE_STATUS_MODIFIED[] = "Modified";
const gchar FILE_STATUS_ADDED[] = "Added";
const gchar FILE_STATUS_DELETED[] = "Deleted";
const gchar FILE_STATUS_UNKNOWN[] = "Unknown";

static GtkWidget *editor_menu_vc = NULL;
static GtkWidget *editor_menu_commit = NULL;
static GtkWidget *menu_item_sep = NULL;
static GtkWidget *menu_entry = NULL;


static void registrate(void);
static void add_menuitems_to_editor_menu(void);
static void remove_menuitems_from_editor_menu(void);


/* Doing some basic keybinding stuff */
enum
{
	VC_DIFF_FILE,
	VC_DIFF_DIR,
	VC_DIFF_BASEDIR,
	VC_COMMIT,
	VC_STATUS,
	VC_UPDATE,
	VC_REVERT_FILE,
	VC_REVERT_DIR,
	VC_REVERT_BASEDIR,
	COUNT_KB
};

enum
{
	COMMIT_MESSAGE_HISTORY_DISPLAY_TEXT = 0,
	COMMIT_MESSAGE_HISTORY_FULL_TEXT,
	COMMIT_MESSAGE_HISTORY_ACTIVE,
	COMMIT_MESSAGE_HISTORY_NUM_COLUMNS
};
#define COMMIT_MESSAGE_HISTORY_LENGTH 10


GSList *get_commit_files_null(G_GNUC_UNUSED const gchar * dir)
{
	return NULL;
}

static void
free_text_list(GSList * lst)
{
	GSList *tmp;
	if (!lst)
		return;
	for (tmp = lst; tmp != NULL; tmp = g_slist_next(tmp))
	{
		g_free((CommitItem *) (tmp->data));
	}
	g_slist_free(lst);
}

static void
free_commit_list(GSList * lst)
{
	GSList *tmp;
	if (!lst)
		return;
	for (tmp = lst; tmp != NULL; tmp = g_slist_next(tmp))
	{
		g_free(((CommitItem *) (tmp->data))->path);
		g_free((CommitItem *) (tmp->data));
	}
	g_slist_free(lst);
}

int
commititem_compare_by_path(const CommitItem * a, const CommitItem * b)
{
	return (g_strcmp0(a->path, b->path));
}

gchar *
find_subdir_path(const gchar * filename, const gchar * subdir)
{
	gboolean ret = FALSE;
	gchar *base;
	gchar *gitdir;
	gchar *base_prev = g_strdup(":");

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		base = g_strdup(filename);
	else
		base = g_path_get_dirname(filename);

	while (strcmp(base, base_prev) != 0)
	{
		gitdir = g_build_filename(base, subdir, NULL);
		ret = g_file_test(gitdir, G_FILE_TEST_IS_DIR);
		g_free(gitdir);
		if (ret)
			break;
		g_free(base_prev);
		base_prev = base;
		base = g_path_get_dirname(base);
	}

	g_free(base_prev);
	if (ret)
		return base;
	g_free(base);
	return NULL;
}

static gboolean
find_subdir(const gchar * filename, const gchar * subdir)
{
	gchar *basedir;
	basedir = find_subdir_path(filename, subdir);
	if (basedir)
	{
		g_free(basedir);
		return TRUE;
	}
	return FALSE;
}

gboolean
find_dir(const gchar * filename, const char *find, gboolean recursive)
{
	gboolean ret;
	gchar *base;
	gchar *dir;

	if (!filename)
		return FALSE;

	if (recursive)
	{
		ret = find_subdir(filename, find);
	}
	else
	{
		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
			base = g_strdup(filename);
		else
			base = g_path_get_dirname(filename);
		dir = g_build_filename(base, find, NULL);

		ret = g_file_test(dir, G_FILE_TEST_IS_DIR);

		g_free(base);
		g_free(dir);
	}
	return ret;
}


static const VC_RECORD *
find_vc(const char *filename)
{
	GSList *tmp;

	for (tmp = VC; tmp != NULL; tmp = g_slist_next(tmp))
	{
		if (((VC_RECORD *) tmp->data)->in_vc(filename))
		{
			return (VC_RECORD *) tmp->data;
		}
	}
	return NULL;
}

static void *
find_cmd_env(gint cmd_type, gboolean cmd, const gchar * filename)
{
	const VC_RECORD *vc;
	vc = find_vc(filename);
	if (vc)
	{
		if (cmd)
			return vc->commands[cmd_type].command;
		else
			return vc->commands[cmd_type].env;
	}
	return NULL;
}

/* Get list of commands for given command spec*/
static GSList *
get_cmd(const gchar ** argv, const gchar * dir, const gchar * filename, GSList * filelist,
	const gchar * message)
{
	gint i, j;
	gint len = 0;
	gchar **ret;
	gchar *abs_dir;
	gchar *base_filename;
	gchar *base_dirname;
	gchar *basename;
	GSList *head = NULL;
	GSList *tmp;
	GString *repl;

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		abs_dir = g_strdup(filename);
	else
		abs_dir = g_path_get_dirname(filename);
	basename = g_path_get_basename(filename);
	base_filename = get_relative_path(dir, filename);
	base_dirname = get_relative_path(dir, abs_dir);

	while (1)
	{
		if (argv[len] == NULL)
			break;
		len++;
	}
	if (filelist)
		ret = g_malloc0(sizeof(gchar *) * (len * g_slist_length(filelist) + 1));
	else
		ret = g_malloc0(sizeof(gchar *) * (len + 1));

	head = g_slist_alloc();
	head->data = ret;

	for (i = 0, j = 0; i < len; i++, j++)
	{
		if (argv[i] == CMD_SEPARATOR)
		{
			if (filelist)
				ret = g_malloc0(sizeof(gchar *) *
						(len * g_slist_length(filelist) + 1));
			else
				ret = g_malloc0(sizeof(gchar *) * (len + 1));
			j = -1;
			head = g_slist_append(head, ret);
		}
		else if (argv[i] == ABS_DIRNAME)
		{
			ret[j] = utils_get_locale_from_utf8(abs_dir);
		}
		else if (argv[i] == ABS_FILENAME)
		{
			ret[j] = utils_get_locale_from_utf8(filename);
		}
		else if (argv[i] == BASE_DIRNAME)
		{
			ret[j] = utils_get_locale_from_utf8(base_dirname);
		}
		else if (argv[i] == BASE_FILENAME)
		{
			ret[j] = utils_get_locale_from_utf8(base_filename);
		}
		else if (argv[i] == BASENAME)
		{
			ret[j] = utils_get_locale_from_utf8(basename);
		}
		else if (argv[i] == FILE_LIST)
		{
			for (tmp = filelist; tmp != NULL; tmp = g_slist_next(tmp))
			{
				ret[j] = utils_get_locale_from_utf8((gchar *) tmp->data);
				j++;
			}
			j--;
		}
		else if (argv[i] == MESSAGE)
		{
			ret[j] = utils_get_locale_from_utf8(message);
		}
		else
		{
			repl = g_string_new(argv[i]);
			utils_string_replace_all(repl, P_ABS_DIRNAME, abs_dir);
			utils_string_replace_all(repl, P_ABS_FILENAME, filename);
			utils_string_replace_all(repl, P_BASENAME, basename);
			ret[j] = g_string_free(repl, FALSE);
			SETPTR(ret[j], utils_get_locale_from_utf8(ret[j]));
		}
	}
	g_free(abs_dir);
	g_free(base_dirname);
	g_free(base_filename);
	g_free(basename);
	return head;
}


/* name should be in UTF-8, and can have a path. */
static void
show_output(const gchar * std_output, const gchar * name,
		const gchar * force_encoding, GeanyFiletype * ftype,
		gint line)
{
	GeanyDocument *doc, *cur_doc;

	if (std_output)
	{
		cur_doc = document_get_current();
		doc = document_find_by_filename(name);
		if (doc == NULL)
		{
			doc = document_new_file(name, ftype, std_output);
		}
		else
		{
			sci_set_text(doc->editor->sci, std_output);
			if (ftype)
				document_set_filetype(doc, ftype);
		}

		document_set_text_changed(doc, set_changed_flag);
		document_set_encoding(doc, (force_encoding ? force_encoding : "UTF-8"));

		navqueue_goto_line(cur_doc, doc, MAX(line + 1, 1));

	}
	else
	{
		ui_set_statusbar(FALSE, _("Could not parse the output of command"));
	}
}

/*
 * Execute command by command spec, return std_out std_err
 *
 * @dir - start directory of command
 * @argv - command spec
 * @env - envirounment
 * @std_out - if not NULL here will be returned standard output converted to utf8 of last command in spec
 * @std_err - if not NULL here will be returned standard error converted to utf8 of last command in spec
 * @filename - filename for spec, commands will be running in it's basedir . Used to replace FILENAME, BASE_FILENAME in spec
 * @list - used to replace FILE_LIST in spec
 * @message - used to replace MESSAGE in spec
 *
 * @return - exit code of last command in spec
 */
gint
execute_custom_command(const gchar * dir, const gchar ** argv, const gchar ** env, gchar ** std_out,
			   gchar ** std_err, const gchar * filename, GSList * list,
			   const gchar * message)
{
	gint exit_code;
	GString *tmp;
	GSList *cur;
	GSList *largv = get_cmd(argv, dir, filename, list, message);
	GError *error = NULL;

	if (std_out)
		*std_out = NULL;
	if (std_err)
		*std_err = NULL;

	if (!largv)
	{
		return 0;
	}
	for (cur = largv; cur != NULL; cur = g_slist_next(cur))
	{
		argv = cur->data;
		if (cur != g_slist_last(largv))
		{
			utils_spawn_sync(dir, cur->data, (gchar **) env,
					 G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
					 G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL,
					 &exit_code, &error);
		}
		else
		{
			utils_spawn_sync(dir, cur->data, (gchar **) env,
					 G_SPAWN_SEARCH_PATH | (std_out ? 0 :
								G_SPAWN_STDOUT_TO_DEV_NULL) |
					 (std_err ? 0 : G_SPAWN_STDERR_TO_DEV_NULL), NULL, NULL,
					 std_out, std_err, &exit_code, &error);
		}
		if (error)
		{
			g_warning("geanyvc: s_spawn_sync error: %s", error->message);
			ui_set_statusbar(FALSE, _("geanyvc: s_spawn_sync error: %s"),
					 error->message);
			g_error_free(error);
		}

		/* need to convert output text from the encoding of the original file into
		   UTF-8 because internally Geany always needs UTF-8 */
		if (std_out && *std_out)
		{
			tmp = g_string_new(*std_out);
			utils_string_replace_all(tmp, "\r\n", "\n");
			utils_string_replace_all(tmp, "\r", "\n");
			SETPTR(*std_out, g_string_free(tmp, FALSE));

			if (!g_utf8_validate(*std_out, -1, NULL))
			{
				SETPTR(*std_out, encodings_convert_to_utf8(*std_out,
									   strlen(*std_out), NULL));
			}
			if (EMPTY(*std_out))
			{
				g_free(*std_out);
				*std_out = NULL;
			}
		}
		if (std_err && *std_err)
		{
			tmp = g_string_new(*std_err);
			utils_string_replace_all(tmp, "\r\n", "\n");
			utils_string_replace_all(tmp, "\r", "\n");
			SETPTR(*std_err, g_string_free(tmp, FALSE));

			if (!g_utf8_validate(*std_err, -1, NULL))
			{
				SETPTR(*std_err, encodings_convert_to_utf8(*std_err,
									   strlen(*std_err), NULL));
			}
			if (EMPTY(*std_err))
			{
				g_free(*std_err);
				*std_err = NULL;
			}
		}
		g_strfreev(cur->data);
	}
	g_slist_free(largv);
	return exit_code;
}

static gint
execute_command(const VC_RECORD * vc, gchar ** std_out, gchar ** std_err, const gchar * filename,
		gint cmd, GSList * list, const gchar * message)
{
	gchar *dir = NULL;
	gint ret;
	const gint action_command_cell = 1;

	if (std_out)
		*std_out = NULL;
	if (std_err)
		*std_err = NULL;

	if (vc->commands[cmd].function)
	{
		return vc->commands[cmd].function(std_out, std_err, filename, list, message);
	}

	if (vc->commands[cmd].startdir == VC_COMMAND_STARTDIR_FILE)
	{
		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
			dir = g_strdup(filename);
		else
			dir = g_path_get_dirname(filename);
	}
	else if (vc->commands[cmd].startdir == VC_COMMAND_STARTDIR_BASE)
	{
		dir = vc->get_base_dir(filename);
	}
	else
	{
		g_warning("geanyvc: unknown startdir type: %d", vc->commands[cmd].startdir);
	}

	ret = execute_custom_command(dir, vc->commands[cmd].command, vc->commands[cmd].env, std_out,
					 std_err, filename, list, message);

	ui_set_statusbar(TRUE, _("File %s: action %s executed via %s."),
			 filename, vc->commands[cmd].command[action_command_cell], vc->program);

	g_free(dir);
	return ret;
}

static gint
get_command_exit_status(gint exit_code)
{
	return (SPAWN_WIFEXITED(exit_code) ? SPAWN_WEXITSTATUS(exit_code) : exit_code);
}

static void
diff_external(const VC_RECORD * vc, const gchar * filename)
{
	gchar *new, *old;
	gchar *localename;

	g_return_if_fail(vc);
	g_return_if_fail(filename);
	g_return_if_fail(get_external_diff_viewer());

	/*  1) rename file to file.geany.~NEW~
	   2) revert file
	   3) rename file to file.geanyvc.~BASE~
	   4) rename file.geany.~NEW~ to origin file
	   5) show diff
	 */
	localename = utils_get_locale_from_utf8(filename);

	new = g_strconcat(filename, ".geanyvc.~NEW~", NULL);
	setptr(new, utils_get_locale_from_utf8(new));

	old = g_strconcat(filename, ".geanyvc.~BASE~", NULL);
	setptr(old, utils_get_locale_from_utf8(old));

	if (g_rename(localename, new) != 0)
	{
		g_warning(_
			  ("geanyvc: diff_external: Unable to rename '%s' to '%s'"),
			  localename, new);
		goto end;
	}

	execute_command(vc, NULL, NULL, filename, VC_COMMAND_REVERT_FILE, NULL, NULL);

	if (g_rename(localename, old) != 0)
	{
		g_warning(_
			  ("geanyvc: diff_external: Unable to rename '%s' to '%s'"),
			  localename, old);
		g_rename(new, localename);
		goto end;
	}
	g_rename(new, localename);

	vc_external_diff(old, localename);
	g_unlink(old);
end:
	g_free(old);
	g_free(new);
	g_free(localename);
}

/* Callback if menu item for a single file was activated */
static void
vcdiff_file_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gchar *text = NULL;
	gchar *name;
	const VC_RECORD *vc;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	if (doc->changed)
	{
		document_save_file(doc, FALSE);
	}

	vc = find_vc(doc->file_name);
	g_return_if_fail(vc);

	execute_command(vc, &text, NULL, doc->file_name, VC_COMMAND_DIFF_FILE, NULL, NULL);
	if (text)
	{
		if (set_external_diff && get_external_diff_viewer())
		{
			g_free(text);
			diff_external(vc, doc->file_name);
			return;
		}
		else
		{
			name = g_strconcat(doc->file_name, ".vc.diff", NULL);
			show_output(text, name, doc->encoding, NULL, 0);
			g_free(text);
			g_free(name);
		}

	}
	else
	{
		ui_set_statusbar(FALSE, _("No changes were made."));
	}
}



/* Callback if menu item for the base directory was activated */
static void
vcdiff_dir_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, gpointer data)
{
	gchar *text = NULL;
	gchar *dir;
	gint flags = GPOINTER_TO_INT(data);
	const VC_RECORD *vc;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	if (doc->changed)
	{
		document_save_file(doc, FALSE);
	}

	vc = find_vc(doc->file_name);
	g_return_if_fail(vc);

	if (flags & FLAG_BASEDIR)
	{
		dir = vc->get_base_dir(doc->file_name);
	}
	else if (flags & FLAG_DIR)
	{
		dir = g_path_get_dirname(doc->file_name);
	}
	else
		return;
	g_return_if_fail(dir);

	execute_command(vc, &text, NULL, dir, VC_COMMAND_DIFF_DIR, NULL, NULL);
	if (text)
	{
		if (set_external_diff && get_external_diff_viewer())
		{
			GSList *lst;

			g_free(text);
			lst = vc->get_commit_files(dir);

			if (lst)
			{
				GSList *list_item = NULL;
				gchar *prev_path = NULL;

				/* - sort the file-list by path; some files may appear with
				     multiple statuses (e.g. Modified and Added)
				   - diff each file only once
				*/
				g_slist_sort(lst,(GCompareFunc)commititem_compare_by_path);

				foreach_slist(list_item, lst)
				{
					CommitItem *item = (CommitItem *)(list_item->data);

					if (flags & FLAG_DIR && !g_str_has_prefix(item->path, dir)) continue;

					if (g_strcmp0(item->path, prev_path))
					{
						diff_external(vc, item->path);
						prev_path = item->path;
					}
				}
				free_commit_list(lst);
			}
		}
		else
		{
			gchar *name;
			name = g_strconcat(dir, ".vc.diff", NULL);
			show_output(text, name, doc->encoding, NULL, 0);
			g_free(text);
			g_free(name);
		}
	}
	else
	{
		ui_set_statusbar(FALSE, _("No changes were made."));
	}
	g_free(dir);
}

static void
vcblame_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gchar *text = NULL;
	const VC_RECORD *vc;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	vc = find_vc(doc->file_name);
	g_return_if_fail(vc);

	execute_command(vc, &text, NULL, doc->file_name, VC_COMMAND_BLAME, NULL, NULL);
	if (text)
	{
		show_output(text, "*VC-BLAME*", NULL,
			doc->file_type, sci_get_current_line(doc->editor->sci));
		g_free(text);
	}
	else
	{
		ui_set_statusbar(FALSE, _("No history available"));
	}
}


static void
vclog_file_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gchar *output = NULL;
	const VC_RECORD *vc;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	vc = find_vc(doc->file_name);
	g_return_if_fail(vc);

	execute_command(vc, &output, NULL, doc->file_name, VC_COMMAND_LOG_FILE, NULL, NULL);
	if (output)
	{
		show_output(output, "*VC-LOG*", NULL, NULL, 0);
		g_free(output);
	}
}

static void
vclog_dir_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gchar *base_name = NULL;
	gchar *text = NULL;
	const VC_RECORD *vc;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	base_name = g_path_get_dirname(doc->file_name);

	vc = find_vc(base_name);
	g_return_if_fail(vc);

	execute_command(vc, &text, NULL, base_name, VC_COMMAND_LOG_DIR, NULL, NULL);
	if (text)
	{
		show_output(text, "*VC-LOG*", NULL, NULL, 0);
		g_free(text);
	}

	g_free(base_name);
}

static void
vclog_basedir_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gchar *text = NULL;
	const VC_RECORD *vc;
	GeanyDocument *doc;
	gchar *basedir;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	vc = find_vc(doc->file_name);
	g_return_if_fail(vc);

	basedir = vc->get_base_dir(doc->file_name);
	g_return_if_fail(basedir);

	execute_command(vc, &text, NULL, basedir, VC_COMMAND_LOG_DIR, NULL, NULL);
	if (text)
	{
		show_output(text, "*VC-LOG*", NULL, NULL, 0);
		g_free(text);
	}
	g_free(basedir);
}

/* Show status from the current directory */
static void
vcstatus_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gchar *base_name = NULL;
	gchar *text = NULL;
	const VC_RECORD *vc;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	if (doc->changed)
	{
		document_save_file(doc, FALSE);
	}

	base_name = g_path_get_dirname(doc->file_name);

	vc = find_vc(base_name);
	g_return_if_fail(vc);

	execute_command(vc, &text, NULL, base_name, VC_COMMAND_STATUS, NULL, NULL);
	if (text)
	{
		show_output(text, "*VC-STATUS*", NULL, NULL, 0);
		g_free(text);
	}

	g_free(base_name);
}

static void
vcshow_file_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gchar *output = NULL;
	const VC_RECORD *vc;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	vc = find_vc(doc->file_name);
	g_return_if_fail(vc);

	execute_command(vc, &output, NULL, doc->file_name, VC_COMMAND_SHOW, NULL, NULL);
	if (output)
	{
		gchar *name;
		name = g_strconcat(doc->file_name, ".vc.orig", NULL);
		show_output(output, name, doc->encoding, doc->file_type, 0);
		g_free(name);
		g_free(output);
	}
}

static gboolean
command_with_question_activated(gchar ** text, gint cmd, const gchar * question, gint flags)
{
	GtkWidget *dialog;
	gint result;
	gchar *dir;
	const VC_RECORD *vc;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_val_if_fail(doc != NULL && doc->file_name != NULL, FALSE);

	dir = g_path_get_dirname(doc->file_name);
	vc = find_vc(dir);
	g_return_val_if_fail(vc, FALSE);

	if (flags & FLAG_BASEDIR)
	{
		dir = vc->get_base_dir(dir);
	}

	if (doc->changed)
	{
		document_save_file(doc, FALSE);
	}

	if ((flags & FLAG_FORCE_ASK) || set_add_confirmation)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_QUESTION,
						GTK_BUTTONS_YES_NO, question,
						(flags & (FLAG_DIR | FLAG_BASEDIR) ? dir :
						 doc->file_name));
		result = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
	else
	{
		result = GTK_RESPONSE_YES;
	}

	if (result == GTK_RESPONSE_YES)
	{
		if (flags & FLAG_FILE)
			execute_command(vc, text, NULL, doc->file_name, cmd, NULL, NULL);
		if (flags & (FLAG_DIR | FLAG_BASEDIR))
			execute_command(vc, text, NULL, dir, cmd, NULL, NULL);
		if (flags & FLAG_RELOAD)
			document_reload_force(doc, NULL);
	}
	g_free(dir);
	return (result == GTK_RESPONSE_YES);
}

static void
vcrevert_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	command_with_question_activated(NULL, VC_COMMAND_REVERT_FILE,
					_("Do you really want to revert: %s?"),
					FLAG_RELOAD | FLAG_FILE | FLAG_FORCE_ASK);
}

static void
vcrevert_dir_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, gint flags)
{
	command_with_question_activated(NULL, VC_COMMAND_REVERT_DIR,
					_("Do you really want to revert: %s?"),
					FLAG_RELOAD | flags | FLAG_FORCE_ASK);
}

static void
vcadd_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	command_with_question_activated(NULL, VC_COMMAND_ADD,
					_("Do you really want to add: %s?"), FLAG_FILE);
}

static void
vcremove_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	if (command_with_question_activated(NULL, VC_COMMAND_REMOVE,
						_("Do you really want to remove: %s?"),
						FLAG_FORCE_ASK | FLAG_FILE))
	{
		document_remove_page(gtk_notebook_get_current_page
					 (GTK_NOTEBOOK(geany->main_widgets->notebook)));
	}
}

static void
vcupdate_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gchar *text = NULL;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	if (doc->changed)
	{
		document_save_file(doc, FALSE);
	}

	if (command_with_question_activated(&text, VC_COMMAND_UPDATE,
						_("Do you really want to update?"), FLAG_BASEDIR))
	{
		document_reload_force(doc, NULL);

		if (!EMPTY(text))
			show_output(text, "*VC-UPDATE*", NULL, NULL, 0);
		g_free(text);
	}
}

enum
{
	COLUMN_COMMIT,
	COLUMN_STATUS,
	COLUMN_PATH,
	NUM_COLUMNS
};

static GtkTreeModel *
create_commit_model(const GSList * commit)
{
	GtkListStore *store;
	GtkTreeIter iter;
	const GSList *cur;

	/* create list store */
	store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING);

	/* add data to the list store */

	for (cur = commit; cur != NULL; cur = g_slist_next(cur))
	{
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COLUMN_COMMIT, TRUE,
				   COLUMN_STATUS, ((CommitItem *) (cur->data))->status,
				   COLUMN_PATH, ((CommitItem *) (cur->data))->path, -1);
	}

	return GTK_TREE_MODEL(store);
}

static gboolean
get_commit_files_foreach(GtkTreeModel * model, G_GNUC_UNUSED GtkTreePath * path, GtkTreeIter * iter,
			 gpointer data)
{
	GSList **files = (GSList **) data;
	gboolean commit;
	gchar *filename;

	gtk_tree_model_get(model, iter, COLUMN_COMMIT, &commit, -1);
	if (!commit)
		return FALSE;

	gtk_tree_model_get(model, iter, COLUMN_PATH, &filename, -1);
	*files = g_slist_prepend(*files, filename);
	return FALSE;
}

static gboolean
get_commit_diff_foreach(GtkTreeModel * model, G_GNUC_UNUSED GtkTreePath * path, GtkTreeIter * iter,
			gpointer data)
{
	GString *diff = data;
	gboolean commit;
	gchar *filename;
	gchar *tmp = NULL;
	gchar *status;
	const VC_RECORD *vc;

	gtk_tree_model_get(model, iter, COLUMN_COMMIT, &commit, -1);
	if (!commit)
		return FALSE;

	gtk_tree_model_get(model, iter, COLUMN_STATUS, &status, -1);

	if (! utils_str_equal(status, FILE_STATUS_MODIFIED))
	{
		g_free(status);
		return FALSE;
	}

	gtk_tree_model_get(model, iter, COLUMN_PATH, &filename, -1);

	vc = find_vc(filename);
	g_return_val_if_fail(vc, FALSE);

	execute_command(vc, &tmp, NULL, filename, VC_COMMAND_DIFF_FILE, NULL, NULL);
	if (tmp)
	{
		/* We temporarily add the filename to the diff output for parsing the diff output later,
		 * after we have finished parsing, we apply the tag "invisible" which hides the text. */
		g_string_append_printf(diff, "VC_DIFF%s\n", filename);
		g_string_append(diff, tmp);
		g_free(tmp);
	}
	else
	{
		g_warning("error: geanyvc: get_commit_diff_foreach: empty diff output");
	}
	g_free(filename);
	return FALSE;
}

static gchar *
get_commit_diff(GtkTreeView * treeview)
{
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	GString *ret = g_string_new(NULL);

	gtk_tree_model_foreach(model, get_commit_diff_foreach, ret);

	return g_string_free(ret, FALSE);
}

static void
set_diff_buff(GtkWidget * textview, GtkTextBuffer * buffer, const gchar * txt)
{
	GtkTextIter start, end;
	GtkTextMark *mark;
	gchar *filename;
	const gchar *tagname = "";
	const gchar *c, *p = txt;

	if (strlen(txt) > COMMIT_DIFF_MAXLENGTH)
	{
		gtk_text_buffer_set_text(buffer,
			_("The resulting differences cannot be displayed because "
			  "the changes are too big to display here and would slow down the UI significantly."
			  "\n\n"
			  "To view the differences, cancel this dialog and open the differences "
			  "in Geany directly by using the GeanyVC menu (Base Directory -> Diff)."), -1);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
		return;
	}
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_NONE);

	gtk_text_buffer_set_text(buffer, txt, -1);

	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);

	gtk_text_buffer_remove_all_tags(buffer, &start, &end);

	while (p)
	{
		c = NULL;
		if (*p == '-')
		{
			tagname = "deleted";
		}
		else if (*p == '+')
		{
			tagname = "added";
		}
		else if (*p == ' ')
		{
			tagname = "";
		}
		else if (strncmp(p, "VC_DIFF", 7) == 0)
		{   /* Lines starting with VC_DIFF are special and were added by our code to tell about
			 * filename to which the following diff lines belong. We use this file to create
			 * text marks which we then later use to scroll to if the corresponding file has been
			 * selected in the commit dialog's files list. */
			tagname = "invisible";
			c = strchr(p + 7, '\n');
		}
		else
		{
			tagname = "default";
		}
		gtk_text_buffer_get_iter_at_offset(buffer, &start,
						   g_utf8_pointer_to_offset(txt, p));

		if (c)
		{   /* create the mark *after* the start iter has been updated */
			filename = g_strndup(p + 7, c - p - 7);
			/* delete old text marks */
			mark = gtk_text_buffer_get_mark(buffer, filename);
			if (mark)
				gtk_text_buffer_delete_mark(buffer, mark);
			/* create a new one */
			gtk_text_buffer_create_mark(buffer, filename, &start, TRUE);
			g_free(filename);
		}

		p = strchr(p, '\n');
		if (p)
		{
			if (*tagname)
			{
				gtk_text_buffer_get_iter_at_offset(buffer, &end,
						g_utf8_pointer_to_offset(txt, p + 1));
				gtk_text_buffer_apply_tag_by_name(buffer, tagname, &start, &end);
			}
			p++;
		}
	}
}

static void
refresh_diff_view(GtkTreeView *treeview)
{
	gchar *diff;
	GtkWidget *diffView = ui_lookup_widget(GTK_WIDGET(treeview), "textDiff");
	diff = get_commit_diff(GTK_TREE_VIEW(treeview));
	set_diff_buff(diffView, gtk_text_view_get_buffer(GTK_TEXT_VIEW(diffView)), diff);
	g_free(diff);
}

static void
commit_toggle_commit(GtkTreeView *treeview, gchar * path_str)
{
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
	gboolean fixed;
	gchar *filename;
	GtkTextView *diffView = GTK_TEXT_VIEW(ui_lookup_widget(GTK_WIDGET(treeview), "textDiff"));
	GtkTextMark *mark;

	/* get toggled iter */
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COLUMN_COMMIT, &fixed, COLUMN_PATH, &filename, -1);

	/* do something with the value */
	fixed ^= 1;

	/* set new value */
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_COMMIT, fixed, -1);

	if (! fixed)
	{
		mark = gtk_text_buffer_get_mark(gtk_text_view_get_buffer(diffView), filename);
		if (mark)
			gtk_text_buffer_delete_mark(gtk_text_view_get_buffer(diffView), mark);
	}

	refresh_diff_view(treeview);

	/* clean up */
	gtk_tree_path_free(path);
	g_free(filename);
}

static void
commit_toggled_cb(G_GNUC_UNUSED GtkCellRendererToggle * cell, gchar * path_str, gpointer data)
{
	commit_toggle_commit(GTK_TREE_VIEW(data), path_str);
}

static gboolean
toggle_all_commit_files (GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	(void)path;
	gtk_list_store_set(GTK_LIST_STORE(model), iter, COLUMN_COMMIT, *(gint*)data, -1);
	return FALSE;
}

static void
commit_all_toggled_cb(GtkToggleButton *check_box, gpointer treeview)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
	gint toggled = gtk_toggle_button_get_active(check_box);

	gtk_tree_model_foreach(model, toggle_all_commit_files, &toggled);

	refresh_diff_view(treeview);
}

static void
add_commit_columns(GtkTreeView * treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* column for fixed toggles */
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(renderer, "toggled", G_CALLBACK(commit_toggled_cb), treeview);

	column = gtk_tree_view_column_new_with_attributes(_("Commit Y/N"),
							  renderer, "active", COLUMN_COMMIT, NULL);

	/* set this column to a fixed sizing (of 80 pixels) */
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column), GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 80);
	gtk_tree_view_append_column(treeview, column);

	/* column for status */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Status"),
							  renderer, "text", COLUMN_STATUS, NULL);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_STATUS);
	gtk_tree_view_append_column(treeview, column);

	/* column for path of file to commit */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Path"),
							  renderer, "text", COLUMN_PATH, NULL);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_PATH);
	gtk_tree_view_append_column(treeview, column);
}

static GdkColor *
get_diff_color(G_GNUC_UNUSED GeanyDocument * doc, gint style)
{
	static GdkColor c = { 0, 0, 0, 0 };
	const GeanyLexerStyle *s;

	s = highlighting_get_style(GEANY_FILETYPES_DIFF, style);
	c.red = (s->foreground & 0xff) << 0x8;
	c.green = s->foreground & 0xff00;
	c.blue = (s->foreground & 0xff0000) >> 0x8;

	return &c;
}

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
	g_object_ref (widget), (GDestroyNotify) g_object_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

static void commit_tree_selection_changed_cb(GtkTreeSelection *sel, GtkTextView *textview)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTextMark *mark;
	gchar *path;
	gboolean set;

	if (! gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, COLUMN_COMMIT, &set, COLUMN_PATH, &path, -1);

	if (set)
	{
		mark = gtk_text_buffer_get_mark(gtk_text_view_get_buffer(textview), path);
		if (mark)
			gtk_text_view_scroll_to_mark(textview, mark, 0.0, TRUE, 0.0, 0.0);
	}
	g_free(path);
}

static void commit_message_update_cb(GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter))
	{
		gchar *commit_message;
		GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
		gboolean active = FALSE;

		gtk_tree_model_get(model, &iter,
			COMMIT_MESSAGE_HISTORY_FULL_TEXT, &commit_message,
			COMMIT_MESSAGE_HISTORY_ACTIVE, &active, -1);

		if (active)
		{
			GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
			gtk_text_buffer_set_text(buffer, commit_message, -1);
		}
		g_free(commit_message);
	}
}

static GtkWidget *
create_commit_message_history_combobox(void)
{
	GtkWidget *combobox;
	GtkCellRenderer *renderer;
	GtkTreeIter iter;
	GtkListStore *store;
	GSList *list_item = NULL;
	GString *display_message_string;
	gchar *display_message;

	combobox = gtk_combo_box_new();
	renderer = gtk_cell_renderer_text_new();
	store = gtk_list_store_new(
		COMMIT_MESSAGE_HISTORY_NUM_COLUMNS,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_BOOLEAN);

	foreach_slist(list_item, commit_message_history)
	{
		display_message_string = g_string_new(list_item->data);
		/* replace line breaks */
		utils_string_replace_all(display_message_string, "\n", " ");
		/* shorten commit message */
		if (display_message_string->len > 80)
		{
			g_string_truncate(display_message_string, 80);
			g_string_append(display_message_string, "...");
		}
		display_message = g_string_free(display_message_string, FALSE);

		/* by _prepend() we reverse the list order for display on purpose:
		 * most recent item should be on top */
		gtk_list_store_prepend(store, &iter);
		gtk_list_store_set(store, &iter,
			COMMIT_MESSAGE_HISTORY_DISPLAY_TEXT, display_message,
			COMMIT_MESSAGE_HISTORY_FULL_TEXT, list_item->data,
			COMMIT_MESSAGE_HISTORY_ACTIVE, TRUE, -1);

		g_free(display_message);
	}

	gtk_list_store_prepend(store, &iter);
	gtk_list_store_set(store, &iter,
		COMMIT_MESSAGE_HISTORY_DISPLAY_TEXT, _("Choose a previous commit message"),
		COMMIT_MESSAGE_HISTORY_FULL_TEXT, NULL,
		COMMIT_MESSAGE_HISTORY_ACTIVE, FALSE, -1);

	gtk_combo_box_set_model(GTK_COMBO_BOX(combobox), GTK_TREE_MODEL(store));
	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);
	g_object_unref(store);

	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), renderer,
		"text", COMMIT_MESSAGE_HISTORY_DISPLAY_TEXT,
		"sensitive", COMMIT_MESSAGE_HISTORY_ACTIVE, NULL);

	return combobox;
}

static gboolean commit_tree_view_key_release_cb(GtkWidget *widget, GdkEventKey *event,
												G_GNUC_UNUSED gpointer user_data)
{
	if (event->keyval == GDK_KEY_space ||
		event->keyval == GDK_KEY_Return ||
		event->keyval == GDK_KEY_KP_Enter)
	{
		GtkTreeView *treeview = GTK_TREE_VIEW(widget);
		GtkTreeModel *model;
		GtkTreeIter iter;
		GtkTreeSelection *sel;
		GtkTreePath *path;
		gchar *path_str;

		sel = gtk_tree_view_get_selection(treeview);
		if (! gtk_tree_selection_get_selected(sel, &model, &iter))
			return FALSE;

		path = gtk_tree_model_get_path(model, &iter);
		if (path != NULL)
		{
			path_str = gtk_tree_path_to_string(path);

			commit_toggle_commit(treeview, path_str);
			gtk_tree_path_free(path);
			g_free(path_str);
		}
	}
	return FALSE;
}

static gboolean commit_text_line_number_update_cb(GtkWidget *widget, GdkEvent *event,
												  gpointer user_data)
{
	GtkWidget *text_view = widget;
	GtkLabel *line_column_label = GTK_LABEL(user_data);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
	GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
	GtkTextIter iter;
	gint line, column;
	gchar text[64];

	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	line = gtk_text_iter_get_line(&iter) + 1;
	column = gtk_text_iter_get_line_offset(&iter);

	g_snprintf(text, sizeof(text), _("Line: %d Column: %d"), line, column);
	gtk_label_set_text(line_column_label, text);

	return FALSE;
}

static GtkWidget *
create_commitDialog(void)
{
	GtkWidget *commitDialog;
	GtkWidget *dialog_vbox1;
	GtkWidget *vpaned1;
	GtkWidget *scrolledwindow1;
	GtkWidget *treeSelect;
	GtkWidget *vpaned2;
	GtkWidget *scrolledwindow2;
	GtkWidget *textDiff;
	GtkWidget *frame1;
	GtkWidget *alignment1;
	GtkWidget *scrolledwindow3;
	GtkWidget *textCommitMessage;
	GtkWidget *label1;
	GtkWidget *dialog_action_area1;
	GtkWidget *btnCancel;
	GtkWidget *btnCommit;
	GtkWidget *select_cbox;
	GtkWidget *bottom_vbox;
	GtkWidget *commit_text_vbox;
	GtkWidget *lineColumnLabel;
	GtkWidget *commitMessageHistoryComboBox;
	GtkTreeSelection *sel;

	gchar *rcstyle = g_strdup_printf("style \"geanyvc-diff-font\"\n"
					 "{\n"
					 "    font_name=\"%s\"\n"
					 "}\n"
					 "widget \"*.GeanyVCCommitDialogDiff\" style \"geanyvc-diff-font\"",
					 geany_data->interface_prefs->editor_font);

	gtk_rc_parse_string(rcstyle);
	g_free(rcstyle);

	commitDialog = gtk_dialog_new();
	gtk_container_set_border_width(GTK_CONTAINER(commitDialog), 5);
	gtk_widget_set_events(commitDialog,
				  GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
				  GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_window_set_title(GTK_WINDOW(commitDialog), _("Commit"));
	gtk_window_set_position(GTK_WINDOW(commitDialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal(GTK_WINDOW(commitDialog), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(commitDialog), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(commitDialog), GDK_WINDOW_TYPE_HINT_DIALOG);

	dialog_vbox1 = gtk_dialog_get_content_area(GTK_DIALOG(commitDialog));
	gtk_widget_show(dialog_vbox1);

	vpaned1 = gtk_vpaned_new();
	gtk_widget_show(vpaned1);
	gtk_box_pack_start(GTK_BOX(dialog_vbox1), vpaned1, TRUE, TRUE, 0);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow1);
	gtk_paned_pack1(GTK_PANED(vpaned1), scrolledwindow1, FALSE, TRUE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow1), GTK_POLICY_AUTOMATIC,
					   GTK_POLICY_AUTOMATIC);

	treeSelect = gtk_tree_view_new();
	gtk_widget_show(treeSelect);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), treeSelect);
	gtk_widget_set_events(treeSelect,
				  GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
				  GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	vpaned2 = gtk_vpaned_new();
	gtk_widget_show(vpaned2);
	gtk_paned_pack2(GTK_PANED(vpaned1), vpaned2, TRUE, TRUE);

	scrolledwindow2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow2);
	gtk_paned_pack1(GTK_PANED(vpaned2), scrolledwindow2, TRUE, TRUE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow2), GTK_POLICY_AUTOMATIC,
					   GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow2), GTK_SHADOW_IN);

	bottom_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(bottom_vbox);
	gtk_paned_pack2(GTK_PANED(vpaned2), bottom_vbox, FALSE, FALSE);

	select_cbox = GTK_WIDGET(gtk_check_button_new_with_mnemonic(_("_De-/select all files")));
	gtk_box_pack_start(GTK_BOX(bottom_vbox), select_cbox, FALSE, FALSE, 2);
	gtk_widget_show(select_cbox);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(select_cbox), TRUE);
	g_signal_connect(select_cbox, "toggled", G_CALLBACK(commit_all_toggled_cb),
			treeSelect);

	textDiff = gtk_text_view_new();
	gtk_widget_set_name(textDiff, "GeanyVCCommitDialogDiff");
	gtk_widget_show(textDiff);
	gtk_container_add(GTK_CONTAINER(scrolledwindow2), textDiff);
	gtk_widget_set_events(textDiff,
				  GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
				  GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textDiff), FALSE);

	frame1 = gtk_frame_new(NULL);
	gtk_widget_show(frame1);
	gtk_box_pack_start(GTK_BOX(bottom_vbox), frame1, TRUE, TRUE, 2);
	gtk_frame_set_shadow_type(GTK_FRAME(frame1), GTK_SHADOW_NONE);

	alignment1 = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_widget_show(alignment1);
	gtk_container_add(GTK_CONTAINER(frame1), alignment1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment1), 0, 0, 12, 0);

	commit_text_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(commit_text_vbox);
	gtk_container_add(GTK_CONTAINER(alignment1), commit_text_vbox);

	scrolledwindow3 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow3);
	gtk_box_pack_start(GTK_BOX(commit_text_vbox), scrolledwindow3, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow3), GTK_POLICY_AUTOMATIC,
					   GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow3), GTK_SHADOW_IN);

	textCommitMessage = gtk_text_view_new();
	gtk_widget_show(textCommitMessage);
	gtk_container_add(GTK_CONTAINER(scrolledwindow3), textCommitMessage);
	gtk_widget_set_events(textCommitMessage,
				  GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
				  GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	label1 = gtk_label_new(_("<b>Commit message:</b>"));
	gtk_widget_show(label1);
	gtk_frame_set_label_widget(GTK_FRAME(frame1), label1);
	gtk_label_set_use_markup(GTK_LABEL(label1), TRUE);

	/* line/column status label */
	lineColumnLabel = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(lineColumnLabel), 0, 0.5);
	gtk_box_pack_end(GTK_BOX(commit_text_vbox), lineColumnLabel, FALSE, TRUE, 0);
	gtk_widget_show(lineColumnLabel);

	/* last commit message dropdown */
	commitMessageHistoryComboBox = create_commit_message_history_combobox();
	gtk_box_pack_end(GTK_BOX(commit_text_vbox), commitMessageHistoryComboBox, FALSE, TRUE, 0);
	gtk_widget_show(commitMessageHistoryComboBox);

	dialog_action_area1 = gtk_dialog_get_action_area(GTK_DIALOG(commitDialog));
	gtk_widget_show(dialog_action_area1);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(dialog_action_area1), GTK_BUTTONBOX_END);

	btnCancel = gtk_button_new_from_stock("gtk-cancel");
	gtk_widget_show(btnCancel);
	gtk_dialog_add_action_widget(GTK_DIALOG(commitDialog), btnCancel, GTK_RESPONSE_CANCEL);

	btnCommit = gtk_button_new_with_mnemonic(_("C_ommit"));
	gtk_widget_show(btnCommit);
	gtk_dialog_add_action_widget(GTK_DIALOG(commitDialog), btnCommit, GTK_RESPONSE_APPLY);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeSelect));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
	g_signal_connect(sel, "changed", G_CALLBACK(commit_tree_selection_changed_cb), textDiff);

	g_signal_connect(treeSelect, "key-release-event",
		G_CALLBACK(commit_tree_view_key_release_cb), NULL);
	g_signal_connect(textCommitMessage, "key-release-event",
		G_CALLBACK(commit_text_line_number_update_cb), lineColumnLabel);
	g_signal_connect(textCommitMessage, "button-release-event",
		G_CALLBACK(commit_text_line_number_update_cb), lineColumnLabel);
	g_signal_connect(commitMessageHistoryComboBox, "changed",
		G_CALLBACK(commit_message_update_cb), textCommitMessage);
	/* initial setup */
	commit_text_line_number_update_cb(textCommitMessage, NULL, lineColumnLabel);

	/* Store pointers to all widgets, for use by lookup_widget(). */
	GLADE_HOOKUP_OBJECT_NO_REF(commitDialog, commitDialog, "commitDialog");
	GLADE_HOOKUP_OBJECT_NO_REF(commitDialog, dialog_vbox1, "dialog_vbox1");
	GLADE_HOOKUP_OBJECT(commitDialog, vpaned1, "vpaned1");
	GLADE_HOOKUP_OBJECT(commitDialog, scrolledwindow1, "scrolledwindow1");
	GLADE_HOOKUP_OBJECT(commitDialog, treeSelect, "treeSelect");
	GLADE_HOOKUP_OBJECT(commitDialog, vpaned2, "vpaned2");
	GLADE_HOOKUP_OBJECT(commitDialog, scrolledwindow2, "scrolledwindow2");
	GLADE_HOOKUP_OBJECT(commitDialog, textDiff, "textDiff");
	GLADE_HOOKUP_OBJECT(commitDialog, frame1, "frame1");
	GLADE_HOOKUP_OBJECT(commitDialog, alignment1, "alignment1");
	GLADE_HOOKUP_OBJECT(commitDialog, scrolledwindow3, "scrolledwindow3");
	GLADE_HOOKUP_OBJECT(commitDialog, textCommitMessage, "textCommitMessage");
	GLADE_HOOKUP_OBJECT(commitDialog, label1, "label1");
	GLADE_HOOKUP_OBJECT_NO_REF(commitDialog, dialog_action_area1, "dialog_action_area1");
	GLADE_HOOKUP_OBJECT(commitDialog, btnCancel, "btnCancel");
	GLADE_HOOKUP_OBJECT(commitDialog, btnCommit, "btnCommit");
	GLADE_HOOKUP_OBJECT(commitDialog, select_cbox, "select_cbox");

	return commitDialog;
}

static void
add_commit_message_to_history(const gchar *commit_message)
{
	GSList *list_item = NULL;
	gchar *message = g_strdup(commit_message);

	/* add commit message to the history */
	commit_message_history = g_slist_append(commit_message_history, (gpointer)message);

	/* House keeping: keep only a maximum of COMMIT_MESSAGE_HISTORY_LENGTH items and delete the rest
	 * We remove the first element from the list as often as the list is too long.
	 * While the loop is not the most efficient way, it should be ok since the list never has
	 * much more than 10 items */
	while (g_slist_length(commit_message_history) > COMMIT_MESSAGE_HISTORY_LENGTH)
	{
		list_item = commit_message_history; /* remember the element to delete */
		commit_message_history = list_item->next; /* start the list with the next item */
		/* delete the current item and its data */
		list_item->next = NULL;
		g_slist_free_full(list_item, g_free);
	}
}

static void
vccommit_activated(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	GeanyDocument *doc;
	gint result;
	const VC_RECORD *vc;
	GSList *lst;
	GtkTreeModel *model;
	GtkWidget *commit = create_commitDialog();
	GtkWidget *treeview = ui_lookup_widget(commit, "treeSelect");
	GtkWidget *diffView = ui_lookup_widget(commit, "textDiff");
	GtkWidget *messageView = ui_lookup_widget(commit, "textCommitMessage");
	GtkWidget *vpaned1 = ui_lookup_widget(commit, "vpaned1");
	GtkWidget *vpaned2 = ui_lookup_widget(commit, "vpaned2");

	GtkTextBuffer *mbuf;
	GtkTextBuffer *diffbuf;

	GtkTextIter begin;
	GtkTextIter end;
	GSList *selected_files = NULL;

	gchar *dir;
	gchar *message;
	gchar *diff;

	gint height;

#ifdef USE_GTKSPELL
	GtkSpellChecker *speller = NULL;
	GError *spellcheck_error = NULL;
#endif

	doc = document_get_current();
	g_return_if_fail(doc);
	g_return_if_fail(doc->file_name);
	vc = find_vc(doc->file_name);
	g_return_if_fail(vc);
	dir = vc->get_base_dir(doc->file_name);

	lst = vc->get_commit_files(dir);
	if (!lst)
	{
		g_free(dir);
		ui_set_statusbar(FALSE, _("Nothing to commit."));
		return;
	}

	model = create_commit_model(lst);
	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), model);
	g_object_unref(model);

	/* add columns to the tree view */
	add_commit_columns(GTK_TREE_VIEW(treeview));

	diff = get_commit_diff(GTK_TREE_VIEW(treeview));
	diffbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(diffView));

	gtk_text_buffer_create_tag(diffbuf, "deleted", "foreground-gdk",
				   get_diff_color(doc, SCE_DIFF_DELETED), NULL);

	gtk_text_buffer_create_tag(diffbuf, "added", "foreground-gdk",
				   get_diff_color(doc, SCE_DIFF_ADDED), NULL);

	gtk_text_buffer_create_tag(diffbuf, "default", "foreground-gdk",
				   get_diff_color(doc, SCE_DIFF_POSITION), NULL);

	gtk_text_buffer_create_tag(diffbuf, "invisible", "invisible",
				   TRUE, NULL);

	set_diff_buff(diffView, diffbuf, diff);

	if (set_maximize_commit_dialog)
	{
		gtk_window_maximize(GTK_WINDOW(commit));
	}
	else
	{
		gtk_widget_set_size_request(commit, 700, 500);
		gtk_window_set_default_size(GTK_WINDOW(commit),
			commit_dialog_width, commit_dialog_height);
	}

	gtk_widget_show_now(commit);
	gtk_window_get_size(GTK_WINDOW(commit), NULL, &height);
	gtk_paned_set_position(GTK_PANED(vpaned1), height * 25 / 100);
	gtk_paned_set_position(GTK_PANED(vpaned2), height * 50 / 100);

#ifdef USE_GTKSPELL
	speller = gtk_spell_checker_new();

	if (EMPTY(lang) || gtk_spell_checker_set_language(speller, lang, &spellcheck_error))
		gtk_spell_checker_attach(speller, GTK_TEXT_VIEW(messageView));
	else
	{
		if (spellcheck_error != NULL)
		{
			ui_set_statusbar(TRUE, _("Error initializing GeanyVC spell checking: %s. Check your configuration."),
				spellcheck_error->message);
			g_error_free(spellcheck_error);
			spellcheck_error = NULL;
		}

		g_object_unref(g_object_ref_sink(speller));
		speller = NULL;
	}
#endif

	/* put the input focus to the commit message text view */
	gtk_widget_grab_focus(messageView);

	result = gtk_dialog_run(GTK_DIALOG(commit));
	if (result == GTK_RESPONSE_APPLY)
	{
		mbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(messageView));
		gtk_text_buffer_get_start_iter(mbuf, &begin);
		gtk_text_buffer_get_end_iter(mbuf, &end);
		message = gtk_text_buffer_get_text(mbuf, &begin, &end, FALSE);
		add_commit_message_to_history(message);
		gtk_tree_model_foreach(model, get_commit_files_foreach, &selected_files);
		if (!EMPTY(message) && selected_files)
		{
			gchar *err_output = NULL;
			gint exit_code;

			exit_code = execute_command(vc, NULL, &err_output, dir, VC_COMMAND_COMMIT, selected_files,
					message);

			if (err_output)
			{
				gint status = get_command_exit_status(exit_code);

				/* - log the commit error (may be a very long message) into the Message Window
				   - overwrite the status bar with a more concise message
				*/
				g_warning("geanyvc: vccommit_activated: Commit failed (status:%d).", status);
				ui_set_statusbar(TRUE, _("Commit failed (status: %d, error: %s)."), status, err_output);
				ui_set_statusbar(FALSE, _("Commit failed; see status in the Message window."));
				g_free(err_output);
			}
			else
			{
				ui_set_statusbar(FALSE, _("Changes committed."));
			}
			free_text_list(selected_files);
		}
		g_free(message);
	}
	/* remember commit dialog widget size */
	gtk_window_get_size(GTK_WINDOW(commit),
		&commit_dialog_width, &commit_dialog_height);

	gtk_widget_destroy(commit);
	free_commit_list(lst);
	g_free(dir);
	g_free(diff);
}

typedef struct _VCFileMenu
{
	GtkWidget *menu;
	struct
	{
		GtkWidget *diff;
		GtkWidget *blame;
		GtkWidget *log;
		GtkWidget *revert;
		GtkWidget *add;
		GtkWidget *remove;
		GtkWidget *show;
	} item;
} VCFileMenu;

static VCFileMenu *menu_vc_file_menu = NULL;
static VCFileMenu *editor_menu_vc_file_menu = NULL;

static GtkWidget *menu_vc_diff_dir = NULL;
static GtkWidget *menu_vc_diff_basedir = NULL;
static GtkWidget *menu_vc_log_dir = NULL;
static GtkWidget *menu_vc_log_basedir = NULL;
static GtkWidget *menu_vc_status = NULL;
static GtkWidget *menu_vc_revert_dir = NULL;
static GtkWidget *menu_vc_revert_basedir = NULL;
static GtkWidget *menu_vc_update = NULL;
static GtkWidget *menu_vc_commit = NULL;

static VCFileMenu ***
get_vc_file_menu_arrv(void)
{
	static VCFileMenu **arrv[] = {NULL, NULL, NULL};

	arrv[0] = &menu_vc_file_menu;
	if (set_editor_menu_entries == TRUE)
		arrv[1] = &editor_menu_vc_file_menu;

	return arrv;
}

static void
update_menu_items(void)
{
	GeanyDocument *doc;

	VCFileMenu ***file_menu_arrv = get_vc_file_menu_arrv();
	gboolean have_file;
	gboolean d_have_vc = FALSE;
	gboolean f_have_vc = FALSE;

	gchar *dir;

	doc = document_get_current();
	have_file = doc && doc->file_name && g_path_is_absolute(doc->file_name);

	if (have_file)
	{
		dir = g_path_get_dirname(doc->file_name);
		if (find_cmd_env(VC_COMMAND_DIFF_FILE, TRUE, dir))
			d_have_vc = TRUE;

		if (find_cmd_env(VC_COMMAND_DIFF_FILE, TRUE, doc->file_name))
			f_have_vc = TRUE;
		g_free(dir);
	}

	/* update items for each of the file menu sets */
	for ( ; file_menu_arrv[0] != NULL; ++file_menu_arrv)
	{
		VCFileMenu *fm = *(file_menu_arrv[0]);
		if (fm == NULL) continue;

		GtkWidget *menu_vc_diff_file = fm->item.diff;
		GtkWidget *menu_vc_blame = fm->item.blame;
		GtkWidget *menu_vc_log_file = fm->item.log;
		GtkWidget *menu_vc_revert_file = fm->item.revert;
		GtkWidget *menu_vc_add_file = fm->item.add;
		GtkWidget *menu_vc_remove_file = fm->item.remove;
		GtkWidget *menu_vc_show_file = fm->item.show;

		gtk_widget_set_sensitive(menu_vc_diff_file, f_have_vc);
		gtk_widget_set_sensitive(menu_vc_blame, f_have_vc);
		gtk_widget_set_sensitive(menu_vc_log_file, f_have_vc);
		gtk_widget_set_sensitive(menu_vc_revert_file, f_have_vc);
		gtk_widget_set_sensitive(menu_vc_remove_file, f_have_vc);
		gtk_widget_set_sensitive(menu_vc_add_file, d_have_vc && !f_have_vc);
		gtk_widget_set_sensitive(menu_vc_show_file, f_have_vc);
	}

	gtk_widget_set_sensitive(menu_vc_diff_dir, d_have_vc);
	gtk_widget_set_sensitive(menu_vc_diff_basedir, d_have_vc);

	gtk_widget_set_sensitive(menu_vc_log_dir, d_have_vc);
	gtk_widget_set_sensitive(menu_vc_log_basedir, d_have_vc);

	gtk_widget_set_sensitive(menu_vc_status, d_have_vc);

	gtk_widget_set_sensitive(menu_vc_revert_dir, f_have_vc);
	gtk_widget_set_sensitive(menu_vc_revert_basedir, f_have_vc);

	gtk_widget_set_sensitive(menu_vc_update, d_have_vc);
	gtk_widget_set_sensitive(menu_vc_commit, d_have_vc);
}


static void
kbdiff_file(G_GNUC_UNUSED guint key_id)
{
	vcdiff_file_activated(NULL, NULL);
}

static void
kbdiff_dir(G_GNUC_UNUSED guint key_id)
{
	vcdiff_dir_activated(NULL, GINT_TO_POINTER(FLAG_DIR));
}

static void
kbdiff_basedir(G_GNUC_UNUSED guint key_id)
{
	vcdiff_dir_activated(NULL, GINT_TO_POINTER(FLAG_BASEDIR));
}

static void
kbstatus(G_GNUC_UNUSED guint key_id)
{
	vcstatus_activated(NULL, NULL);
}

static void
kbcommit(G_GNUC_UNUSED guint key_id)
{
	vccommit_activated(NULL, NULL);
}

static void
kbrevert_file(G_GNUC_UNUSED guint key_id)
{
	vcrevert_activated(NULL, NULL);
}

static void
kbrevert_dir(G_GNUC_UNUSED guint key_id)
{
	vcrevert_dir_activated(NULL, FLAG_DIR);
}

static void
kbrevert_basedir(G_GNUC_UNUSED guint key_id)
{
	vcrevert_dir_activated(NULL, FLAG_BASEDIR);
}

static void
kbupdate(G_GNUC_UNUSED guint key_id)
{
	vcupdate_activated(NULL, NULL);
}


static struct
{
	GtkWidget *cb_changed_flag;
	GtkWidget *cb_confirm_add;
	GtkWidget *cb_max_commit;
	GtkWidget *cb_external_diff;
	GtkWidget *cb_editor_menu_entries;
	GtkWidget *cb_attach_to_menubar;
	GtkWidget *cb_cvs;
	GtkWidget *cb_git;
	GtkWidget *cb_fossil;
	GtkWidget *cb_svn;
	GtkWidget *cb_svk;
	GtkWidget *cb_bzr;
	GtkWidget *cb_hg;
#ifdef USE_GTKSPELL
	GtkWidget *spellcheck_lang_textbox;
#endif
}
widgets;

static void
save_config(void)
{
	GSList *list_item = NULL;
	gint list_item_count = 0;
	GKeyFile *config = g_key_file_new();
	gchar *config_dir = g_path_get_dirname(config_file);

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	g_key_file_set_boolean(config, "VC", "set_changed_flag", set_changed_flag);
	g_key_file_set_boolean(config, "VC", "set_add_confirmation", set_add_confirmation);
	g_key_file_set_boolean(config, "VC", "set_external_diff", set_external_diff);
	g_key_file_set_boolean(config, "VC", "set_maximize_commit_dialog",
				   set_maximize_commit_dialog);
	g_key_file_set_boolean(config, "VC", "set_editor_menu_entries", set_editor_menu_entries);
	g_key_file_set_boolean(config, "VC", "attach_to_menubar", set_menubar_entry);

	g_key_file_set_boolean(config, "VC", "enable_cvs", enable_cvs);
	g_key_file_set_boolean(config, "VC", "enable_git", enable_git);
	g_key_file_set_boolean(config, "VC", "enable_fossil", enable_fossil);
	g_key_file_set_boolean(config, "VC", "enable_svn", enable_svn);
	g_key_file_set_boolean(config, "VC", "enable_svk", enable_svk);
	g_key_file_set_boolean(config, "VC", "enable_bzr", enable_bzr);
	g_key_file_set_boolean(config, "VC", "enable_hg", enable_hg);

#ifdef USE_GTKSPELL
	g_key_file_set_string(config, "VC", "spellchecking_language", lang);
#endif

	if (commit_dialog_width > 0 && commit_dialog_height > 0)
	{
		g_key_file_set_integer(config, "CommitDialog",
			"commit_dialog_width", commit_dialog_width);
		g_key_file_set_integer(config, "CommitDialog",
			"commit_dialog_height", commit_dialog_height);
	}

	g_key_file_remove_group(config, "CommitMessageHistory", NULL); /* cleanup */
	foreach_slist(list_item, commit_message_history)
	{
		gchar *key = g_strdup_printf("message_%d", list_item_count);
		g_key_file_set_string(config, "CommitMessageHistory", key, list_item->data);
		g_free(key);

		list_item_count++;
	}

	if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)
		&& utils_mkdir(config_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
					_
					("Plugin configuration directory could not be created."));
	}
	else
	{
		/* write config to file */
		gchar *data = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(config_file, data);
		g_free(data);
	}

	g_free(config_dir);
	g_key_file_free(config);
}

static void
on_configure_response(G_GNUC_UNUSED GtkDialog * dialog, gint response,
			  G_GNUC_UNUSED gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		set_changed_flag =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_changed_flag));
		set_add_confirmation =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_confirm_add));
		set_maximize_commit_dialog =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_max_commit));

		set_external_diff =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_external_diff));

		set_editor_menu_entries =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_editor_menu_entries));
		set_menubar_entry =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_attach_to_menubar));

		enable_cvs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_cvs));
		enable_git = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_git));
		enable_fossil = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_fossil));
		enable_svn = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_svn));
		enable_svk = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_svk));
		enable_bzr = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_bzr));
		enable_hg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.cb_hg));

#ifdef USE_GTKSPELL
		g_free(lang);
		lang = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets.spellcheck_lang_textbox)));
#endif

		save_config();

		if (set_editor_menu_entries == FALSE)
			remove_menuitems_from_editor_menu();
		else
			add_menuitems_to_editor_menu();

		registrate();
	}
}

GtkWidget *
plugin_configure(GtkDialog * dialog)
{
	GtkWidget *vbox;

#ifdef USE_GTKSPELL
	GtkWidget *label_spellcheck_lang;
#endif

	vbox = gtk_vbox_new(FALSE, 6);

	widgets.cb_changed_flag =
		gtk_check_button_new_with_label(_
						("Set Changed-flag for document tabs created by the plugin"));
	gtk_widget_set_tooltip_text(widgets.cb_changed_flag,
				 _
				 ("If this option is activated, every new by the VC-plugin created document tab "
				  "will be marked as changed. Even this option is useful in some cases, it could cause "
				  "a big number of annoying \"Do you want to save\"-dialogs."));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_changed_flag), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_changed_flag), set_changed_flag);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_changed_flag, FALSE, FALSE, 2);

	widgets.cb_confirm_add =
		gtk_check_button_new_with_label(_("Confirm adding new files to a VCS"));
	gtk_widget_set_tooltip_text(widgets.cb_confirm_add,
				 _
				 ("Shows a confirmation dialog on adding a new (created) file to VCS."));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_confirm_add), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_confirm_add),
					 set_add_confirmation);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_confirm_add, TRUE, FALSE, 2);

	widgets.cb_max_commit = gtk_check_button_new_with_label(_("Maximize commit dialog"));
	gtk_widget_set_tooltip_text(widgets.cb_max_commit, _("Show commit dialog maximize."));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_max_commit), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_max_commit),
					 set_maximize_commit_dialog);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_max_commit, TRUE, FALSE, 2);

	widgets.cb_external_diff = gtk_check_button_new_with_label(_("Use external diff viewer"));
	gtk_widget_set_tooltip_text(widgets.cb_external_diff,
				 _("Use external diff viewer for file diff."));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_external_diff), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_external_diff),
					 set_external_diff);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_external_diff, TRUE, FALSE, 2);

	widgets.cb_editor_menu_entries = gtk_check_button_new_with_label(_("Show VC entries at editor menu"));
	gtk_widget_set_tooltip_text(widgets.cb_editor_menu_entries,
				 _("Show entries for VC functions inside editor menu"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_editor_menu_entries), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_editor_menu_entries), set_editor_menu_entries);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_editor_menu_entries, TRUE, FALSE, 2);

	widgets.cb_attach_to_menubar = gtk_check_button_new_with_label(_("Attach menu to menubar"));
	gtk_widget_set_tooltip_text(widgets.cb_attach_to_menubar,
				 _("Whether menu for this plugin are getting placed either "
				   "inside tools menu or directly inside Geany's menubar. "
				   "Will take in account after next start of GeanyVC"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_attach_to_menubar), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_attach_to_menubar),
		set_menubar_entry);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_attach_to_menubar, TRUE, FALSE, 2);

	widgets.cb_cvs = gtk_check_button_new_with_label(_("Enable CVS"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_cvs), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_cvs), enable_cvs);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_cvs, TRUE, FALSE, 2);

	widgets.cb_git = gtk_check_button_new_with_label(_("Enable GIT"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_git), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_git), enable_git);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_git, TRUE, FALSE, 2);

	widgets.cb_fossil = gtk_check_button_new_with_label(_("Enable Fossil"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_fossil), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_fossil), enable_fossil);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_fossil, TRUE, FALSE, 2);

	widgets.cb_svn = gtk_check_button_new_with_label(_("Enable SVN"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_svn), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_svn), enable_svn);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_svn, TRUE, FALSE, 2);

	widgets.cb_svk = gtk_check_button_new_with_label(_("Enable SVK"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_svk), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_svk), enable_svk);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_svk, TRUE, FALSE, 2);

	widgets.cb_bzr = gtk_check_button_new_with_label(_("Enable Bazaar"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_bzr), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_bzr), enable_bzr);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_bzr, TRUE, FALSE, 2);

	widgets.cb_hg = gtk_check_button_new_with_label(_("Enable Mercurial"));
	gtk_button_set_focus_on_click(GTK_BUTTON(widgets.cb_hg), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.cb_hg), enable_hg);
	gtk_box_pack_start(GTK_BOX(vbox), widgets.cb_hg, TRUE, FALSE, 2);

#ifdef USE_GTKSPELL
	label_spellcheck_lang = gtk_label_new(_("Spellcheck language"));
	widgets.spellcheck_lang_textbox = gtk_entry_new();
	gtk_widget_show(widgets.spellcheck_lang_textbox);
	if (lang != NULL)
		gtk_entry_set_text(GTK_ENTRY(widgets.spellcheck_lang_textbox), lang);

	gtk_misc_set_alignment(GTK_MISC(label_spellcheck_lang), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label_spellcheck_lang);
	gtk_container_add(GTK_CONTAINER(vbox), widgets.spellcheck_lang_textbox);
#endif
	gtk_widget_show_all(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);
	return vbox;
}

static void
load_config(void)
{
#ifdef USE_GTKSPELL
	GError *error = NULL;
#endif

	gchar **commit_message_history_keys, **ptr = NULL;
	gchar *commit_message;
	GKeyFile *config = g_key_file_new();

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	set_changed_flag = utils_get_setting_boolean(config, "VC",
		"set_changed_flag", FALSE);
	set_add_confirmation = utils_get_setting_boolean(config, "VC",
		"set_add_confirmation", TRUE);
	set_maximize_commit_dialog = utils_get_setting_boolean(config, "VC",
		"set_maximize_commit_dialog", FALSE);
	set_external_diff = utils_get_setting_boolean(config, "VC",
		"set_external_diff", TRUE);
	set_editor_menu_entries = utils_get_setting_boolean(config, "VC",
		"set_editor_menu_entries", FALSE);
	enable_cvs = utils_get_setting_boolean(config, "VC", "enable_cvs",
		TRUE);
	enable_git = utils_get_setting_boolean(config, "VC", "enable_git",
		TRUE);
	enable_fossil = utils_get_setting_boolean(config, "VC", "enable_fossil",
		TRUE);
	enable_svn = utils_get_setting_boolean(config, "VC", "enable_svn",
		TRUE);
	enable_svk = utils_get_setting_boolean(config, "VC", "enable_svk",
		TRUE);
	enable_bzr = utils_get_setting_boolean(config, "VC", "enable_bzr",
		TRUE);
	enable_hg = utils_get_setting_boolean(config, "VC", "enable_hg",
		TRUE);
	set_menubar_entry = utils_get_setting_boolean(config, "VC", "attach_to_menubar",
		FALSE);

#ifdef USE_GTKSPELL
	lang = g_key_file_get_string(config, "VC", "spellchecking_language", &error);
	if (error != NULL)
	{
		/* Set default value. Using system standard language. */
		lang = NULL;
		g_error_free(error);
		error = NULL;
	}
#endif

	commit_dialog_width = utils_get_setting_integer(config, "CommitDialog",
		"commit_dialog_width", 700);
	commit_dialog_height = utils_get_setting_integer(config, "CommitDialog",
		"commit_dialog_height", 500);

	commit_message_history_keys = g_key_file_get_keys(config,"CommitMessageHistory", NULL, NULL);
	if (commit_message_history_keys != NULL)
	{
		foreach_strv(ptr, commit_message_history_keys)
		{
			commit_message = g_key_file_get_string(config, "CommitMessageHistory", *ptr, NULL);
			/* do not free the newly allocated commit_message string as we need it later in the list,
			 * will be finally freed when cleaning up the commit_message_history list */
			commit_message_history = g_slist_append(commit_message_history, commit_message);
		}
		g_strfreev(commit_message_history_keys);
	}
	g_key_file_free(config);
}

static void
registrate(void)
{
	gchar *path;
	if (VC)
	{
		g_slist_free(VC);
		VC = NULL;
	}
	REGISTER_VC(FOSSIL, enable_fossil);
	REGISTER_VC(GIT, enable_git);
	REGISTER_VC(SVN, enable_svn);
	REGISTER_VC(CVS, enable_cvs);
	REGISTER_VC(SVK, enable_svk);
	REGISTER_VC(BZR, enable_bzr);
	REGISTER_VC(HG, enable_hg);
}

static void
do_current_file_menu(GtkWidget ** parent_menu, const gchar * label, VCFileMenu ** file_menu)
{
	/* Menu which will hold the items in the current file menu.
	 * This menu has the same structure in case of Main menu and Editor's context menu.
	 * We construct the menu, attach it to the parent menu, and save it so that
	 * the updates could be applied in both Main menu and Editor context scopes.
	 */
	GtkWidget *cur_file_menu = NULL;
	GtkWidget *menu_vc_diff_file = NULL;
	GtkWidget *menu_vc_blame = NULL;
	GtkWidget *menu_vc_log_file = NULL;
	GtkWidget *menu_vc_revert_file = NULL;
	GtkWidget *menu_vc_add_file = NULL;
	GtkWidget *menu_vc_remove_file = NULL;
	GtkWidget *menu_vc_show_file = NULL;

	/* create the parent menu */
	*parent_menu = gtk_image_menu_item_new_with_mnemonic(label);
	g_signal_connect(* parent_menu, "activate", G_CALLBACK(update_menu_items), NULL);

	cur_file_menu = gtk_menu_new();

	/* Diff of current file */
	menu_vc_diff_file = gtk_menu_item_new_with_mnemonic(_("_Diff"));
	gtk_container_add(GTK_CONTAINER(cur_file_menu), menu_vc_diff_file);
	gtk_widget_set_tooltip_text(menu_vc_diff_file,
				 _("Make a diff from the current active file"));

	g_signal_connect(menu_vc_diff_file, "activate", G_CALLBACK(vcdiff_file_activated), NULL);

	/* Revert current file */
	menu_vc_revert_file = gtk_menu_item_new_with_mnemonic(_("_Revert"));
	gtk_container_add(GTK_CONTAINER(cur_file_menu), menu_vc_revert_file);
	gtk_widget_set_tooltip_text(menu_vc_revert_file,
				 _("Restore pristine working copy file (undo local edits)."));

	g_signal_connect(menu_vc_revert_file, "activate", G_CALLBACK(vcrevert_activated), NULL);


	gtk_container_add(GTK_CONTAINER(cur_file_menu), gtk_separator_menu_item_new());


	/* Blame for current file */
	menu_vc_blame = gtk_menu_item_new_with_mnemonic(_("_Blame"));
	gtk_container_add(GTK_CONTAINER(cur_file_menu), menu_vc_blame);
	gtk_widget_set_tooltip_text(menu_vc_blame,
				 _("Shows the changes made at one file per revision and author."));

	g_signal_connect(menu_vc_blame, "activate", G_CALLBACK(vcblame_activated), NULL);

	gtk_container_add(GTK_CONTAINER(cur_file_menu), gtk_separator_menu_item_new());

	/* History/log of current file */
	menu_vc_log_file = gtk_menu_item_new_with_mnemonic(_("_History (log)"));
	gtk_container_add(GTK_CONTAINER(cur_file_menu), menu_vc_log_file);
	gtk_widget_set_tooltip_text(menu_vc_log_file,
				 _("Shows the log of the current file"));

	g_signal_connect(menu_vc_log_file, "activate", G_CALLBACK(vclog_file_activated), NULL);

	/* base version of the current file */
	menu_vc_show_file = gtk_menu_item_new_with_mnemonic(_("_Original"));
	gtk_container_add(GTK_CONTAINER(cur_file_menu), menu_vc_show_file);
	gtk_widget_set_tooltip_text(menu_vc_show_file,
				 _("Shows the original of the current file"));

	g_signal_connect(menu_vc_show_file, "activate", G_CALLBACK(vcshow_file_activated), NULL);


	gtk_container_add(GTK_CONTAINER(cur_file_menu), gtk_separator_menu_item_new());

	/* add current file */
	menu_vc_add_file = gtk_menu_item_new_with_mnemonic(_("_Add to Version Control"));
	gtk_container_add(GTK_CONTAINER(cur_file_menu), menu_vc_add_file);
	gtk_widget_set_tooltip_text(menu_vc_add_file, _("Add file to repository."));

	g_signal_connect(menu_vc_add_file, "activate",
			 G_CALLBACK(vcadd_activated), NULL);

	/* remove current file */
	menu_vc_remove_file = gtk_menu_item_new_with_mnemonic(_("_Remove from Version Control"));
	gtk_container_add(GTK_CONTAINER(cur_file_menu), menu_vc_remove_file);
	gtk_widget_set_tooltip_text(menu_vc_remove_file, _("Remove file from repository."));

	g_signal_connect(menu_vc_remove_file, "activate", G_CALLBACK(vcremove_activated), NULL);

	/* connect to parent menu */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(*parent_menu), cur_file_menu);

	/* save the created file menu */
	if (*file_menu == NULL)
		*file_menu = g_new0(VCFileMenu, 1);

	if (*file_menu)
	{
		VCFileMenu *fm = *file_menu;

		fm->menu = cur_file_menu;
		fm->item.diff = menu_vc_diff_file;
		fm->item.blame = menu_vc_blame;
		fm->item.log = menu_vc_log_file;
		fm->item.revert = menu_vc_revert_file;
		fm->item.add = menu_vc_add_file;
		fm->item.remove = menu_vc_remove_file;
		fm->item.show = menu_vc_show_file;
	}
}

static void
do_current_dir_menu(GtkWidget ** parent_menu)
{
	GtkWidget *cur_dir_menu = NULL;
	/* Menu which will hold the items in the current file menu */
	cur_dir_menu = gtk_menu_new();

	*parent_menu = gtk_image_menu_item_new_with_mnemonic(_("_Directory"));
	g_signal_connect(* parent_menu, "activate", G_CALLBACK(update_menu_items), NULL);
	/* Diff of the current dir */
	menu_vc_diff_dir = gtk_menu_item_new_with_mnemonic(_("_Diff"));
	gtk_container_add(GTK_CONTAINER(cur_dir_menu), menu_vc_diff_dir);
	gtk_widget_set_tooltip_text(menu_vc_diff_dir,
				 _("Make a diff from the directory of the current active file"));

	g_signal_connect(menu_vc_diff_dir, "activate",
			 G_CALLBACK(vcdiff_dir_activated), GINT_TO_POINTER(FLAG_DIR));

	/* Revert current dir */
	menu_vc_revert_dir = gtk_menu_item_new_with_mnemonic(_("_Revert"));
	gtk_container_add(GTK_CONTAINER(cur_dir_menu), menu_vc_revert_dir);
	gtk_widget_set_tooltip_text(menu_vc_revert_dir,
				 _("Restore original files in the current folder (undo local edits)."));

	g_signal_connect(menu_vc_revert_dir, "activate",
			 G_CALLBACK(vcrevert_dir_activated), GINT_TO_POINTER(FLAG_DIR));

	gtk_container_add(GTK_CONTAINER(cur_dir_menu), gtk_separator_menu_item_new());
	/* History/log of the current dir */
	menu_vc_log_dir = gtk_menu_item_new_with_mnemonic(_("_History (log)"));
	gtk_container_add(GTK_CONTAINER(cur_dir_menu), menu_vc_log_dir);
	gtk_widget_set_tooltip_text(menu_vc_log_dir,
				 _("Shows the log of the current directory"));


	/* connect to parent menu */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(*parent_menu), cur_dir_menu);
}

static void
do_basedir_menu(GtkWidget ** parent_menu)
{
	GtkWidget *basedir_menu = NULL;
	/* Menu which will hold the items in the current file menu */
	basedir_menu = gtk_menu_new();

	*parent_menu = gtk_image_menu_item_new_with_mnemonic(_("_Base Directory"));
	g_signal_connect(* parent_menu, "activate", G_CALLBACK(update_menu_items), NULL);

	/* Complete diff of base directory */
	menu_vc_diff_basedir = gtk_menu_item_new_with_mnemonic(_("_Diff"));
	gtk_container_add(GTK_CONTAINER(basedir_menu), menu_vc_diff_basedir);
	gtk_widget_set_tooltip_text(menu_vc_diff_basedir, _("Make a diff from the top VC directory"));

	g_signal_connect(menu_vc_diff_basedir, "activate",
			 G_CALLBACK(vcdiff_dir_activated), GINT_TO_POINTER(FLAG_BASEDIR));

	/* Revert everything */
	menu_vc_revert_basedir = gtk_menu_item_new_with_mnemonic(_("_Revert"));
	gtk_container_add(GTK_CONTAINER(basedir_menu), menu_vc_revert_basedir);
	gtk_widget_set_tooltip_text(menu_vc_revert_basedir, _("Revert any local edits."));

	g_signal_connect(menu_vc_revert_basedir, "activate",
			 G_CALLBACK(vcrevert_dir_activated), GINT_TO_POINTER(FLAG_BASEDIR));

	gtk_container_add(GTK_CONTAINER(basedir_menu), gtk_separator_menu_item_new());
	g_signal_connect(menu_vc_log_dir, "activate",
			 G_CALLBACK(vclog_dir_activated), NULL);

	/* Complete History/Log of base directory */
	menu_vc_log_basedir = gtk_menu_item_new_with_mnemonic(_("_History (log)"));
	gtk_container_add(GTK_CONTAINER(basedir_menu), menu_vc_log_basedir);
	gtk_widget_set_tooltip_text(menu_vc_log_basedir,
				 _("Shows the log of the top VC directory"));

	g_signal_connect(menu_vc_log_basedir, "activate",
			 G_CALLBACK(vclog_basedir_activated), NULL);

	/* connect to parent menu */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(*parent_menu), basedir_menu);
}

static void
add_menuitems_to_editor_menu(void)
{
	/* Add file menu also to editor menu (at mouse cursor) */
	if (set_editor_menu_entries == TRUE && editor_menu_vc == NULL)
	{
		menu_item_sep = gtk_separator_menu_item_new();
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), menu_item_sep);
		do_current_file_menu(&editor_menu_vc, _("_VC file Actions"), &editor_menu_vc_file_menu);
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), editor_menu_vc);
		gtk_widget_show_all(editor_menu_vc);
		gtk_widget_show_all(menu_item_sep);
	}

	/* Add commit item to editor menu */
	if (set_editor_menu_entries == TRUE && editor_menu_commit == NULL)
	{
		editor_menu_commit = gtk_menu_item_new_with_mnemonic(_("VC _Commit..."));
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), editor_menu_commit);
		g_signal_connect(editor_menu_commit, "activate",
			G_CALLBACK(vccommit_activated), NULL);
		gtk_widget_show_all(editor_menu_commit);
	}
}

static void
remove_menuitems_from_editor_menu(void)
{
	if (editor_menu_vc != NULL)
	{
		gtk_widget_destroy(editor_menu_vc);
		editor_menu_vc = NULL;
	}
	if (editor_menu_vc_file_menu)
	{
		g_free(editor_menu_vc_file_menu);
		editor_menu_vc_file_menu = NULL;
	}
	if (editor_menu_commit != NULL)
	{
		gtk_widget_destroy(editor_menu_commit);
		editor_menu_commit = NULL;
	}
	if (menu_item_sep != NULL)
	{
		gtk_widget_destroy(menu_item_sep);
		menu_item_sep = NULL;
	}
}

static void
init_keybindings(void)
{
	GtkWidget *menu_vc_diff_file = menu_vc_file_menu->item.diff;
	GtkWidget *menu_vc_revert_file = menu_vc_file_menu->item.revert;

	/* init keybindings */
	GeanyKeyGroup *plugin_key_group =
				plugin_set_key_group(geany_plugin, "geanyvc", COUNT_KB, NULL);

	keybindings_set_item(plugin_key_group, VC_DIFF_FILE, kbdiff_file, 0, 0,
				 "vc_show_diff_of_file", _("Show diff of file"), menu_vc_diff_file);
	keybindings_set_item(plugin_key_group, VC_DIFF_DIR, kbdiff_dir, 0, 0,
				 "vc_show_diff_of_dir", _("Show diff of directory"), menu_vc_diff_dir);
	keybindings_set_item(plugin_key_group, VC_DIFF_BASEDIR, kbdiff_basedir, 0, 0,
				 "vc_show_diff_of_basedir", _("Show diff of basedir"),
				 menu_vc_diff_basedir);
	keybindings_set_item(plugin_key_group, VC_COMMIT, kbcommit, 0, 0, "vc_commit",
				 _("Commit changes"), menu_vc_commit);
	keybindings_set_item(plugin_key_group, VC_STATUS, kbstatus, 0, 0, "vc_status",
				 _("Show status"), menu_vc_status);
	keybindings_set_item(plugin_key_group, VC_REVERT_FILE, kbrevert_file, 0, 0,
				 "vc_revert_file", _("Revert single file"), menu_vc_revert_file);
	keybindings_set_item(plugin_key_group, VC_REVERT_DIR, kbrevert_dir, 0, 0, "vc_revert_dir",
				 _("Revert directory"), menu_vc_revert_dir);
	keybindings_set_item(plugin_key_group, VC_REVERT_BASEDIR, kbrevert_basedir, 0, 0,
				 "vc_revert_basedir", _("Revert base directory"), menu_vc_revert_basedir);
	keybindings_set_item(plugin_key_group, VC_UPDATE, kbupdate, 0, 0, "vc_update",
				 _("Update file"), menu_vc_update);
}

/* Called by Geany to initialize the plugin */
void
plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	GtkWidget *menu_vc = NULL;
	GtkWidget *menu_vc_menu = NULL;
	GtkWidget *menu_vc_file = NULL;
	GtkWidget *menu_vc_dir = NULL;
	GtkWidget *menu_vc_basedir = NULL;

	config_file =
		g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
				"VC", G_DIR_SEPARATOR_S, "VC.conf", NULL);

	load_config();
	registrate();

	external_diff_viewer_init();

	if (set_menubar_entry == TRUE)
	{
		GtkMenuShell *menubar;
		GList *menubar_children;

		menubar = GTK_MENU_SHELL(
				ui_lookup_widget(geany->main_widgets->window, "menubar1"));

		menu_vc = gtk_menu_item_new_with_mnemonic(_("_VC"));
		menubar_children = gtk_container_get_children(GTK_CONTAINER(menubar));
		gtk_menu_shell_insert(menubar, menu_vc, g_list_length(menubar_children) - 1);
		g_list_free(menubar_children);
	}
	else
	{
		menu_vc = gtk_image_menu_item_new_with_mnemonic(_("_Version Control"));
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_vc);
	}

	g_signal_connect(menu_vc, "activate", G_CALLBACK(update_menu_items), NULL);

	menu_vc_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_vc), menu_vc_menu);

	/* Create the current file Submenu */
	do_current_file_menu(&menu_vc_file, _("_File"), &menu_vc_file_menu);
	gtk_container_add(GTK_CONTAINER(menu_vc_menu), menu_vc_file);

	/* Create the current directory Submenu */
	do_current_dir_menu(&menu_vc_dir);
	gtk_container_add(GTK_CONTAINER(menu_vc_menu), menu_vc_dir);
	/* Create the current base directory Submenu */
	do_basedir_menu(&menu_vc_basedir);
	gtk_container_add(GTK_CONTAINER(menu_vc_menu), menu_vc_basedir);
	gtk_container_add(GTK_CONTAINER(menu_vc_menu), gtk_separator_menu_item_new());

	/* Status of basedir */
	menu_vc_status = gtk_menu_item_new_with_mnemonic(_("_Status"));
	gtk_container_add(GTK_CONTAINER(menu_vc_menu), menu_vc_status);
	gtk_widget_set_tooltip_text(menu_vc_status, _("Show status."));

	g_signal_connect(menu_vc_status, "activate", G_CALLBACK(vcstatus_activated), NULL);

	/* complete update */
	menu_vc_update = gtk_menu_item_new_with_mnemonic(_("_Update"));
	gtk_container_add(GTK_CONTAINER(menu_vc_menu), menu_vc_update);
	gtk_widget_set_tooltip_text(menu_vc_update, _("Update from remote repository."));

	g_signal_connect(menu_vc_update, "activate", G_CALLBACK(vcupdate_activated), NULL);

	/* Commit all changes */
	menu_vc_commit = gtk_menu_item_new_with_mnemonic(_("_Commit..."));
	gtk_container_add(GTK_CONTAINER(menu_vc_menu), menu_vc_commit);
	gtk_widget_set_tooltip_text(menu_vc_commit, _("Commit changes."));

	g_signal_connect(menu_vc_commit, "activate", G_CALLBACK(vccommit_activated), NULL);

	gtk_widget_show_all(menu_vc);

	/* initialize keybindings */
	init_keybindings();

	/* init entries inside editor menu */
	add_menuitems_to_editor_menu();

	ui_add_document_sensitive(menu_vc);
	menu_entry = menu_vc;
}


/* Called by Geany before unloading the plugin. */
void
plugin_cleanup(void)
{
	save_config();
	external_diff_viewer_deinit();
	remove_menuitems_from_editor_menu();
	gtk_widget_destroy(menu_entry);
	if (menu_vc_file_menu)
	{
		g_free(menu_vc_file_menu);
		menu_vc_file_menu = NULL;
	}
	g_slist_free(VC);
	VC = NULL;
	g_slist_free_full(commit_message_history, g_free);
	g_free(config_file);
}
