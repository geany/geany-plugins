/*
 * geanydebug.c - Integrated debugger plugin for the Geany IDE
 * Copyright 2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 */


#include "geany.h"

#include <glib/gstdio.h>

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "support.h"
#include "prefs.h"
#include "document.h"
#include "editor.h"
#include "utils.h"
#include "ui_utils.h"
#include "keybindings.h"
#include "project.h"
#include "msgwindow.h"

#include "plugindata.h"
#include "geanyfunctions.h"

#include "gdb-io.h"
#include "gdb-ui.h"



#define UNIX_NAME "geanygdb"
#define UNIX_NAME_OLD "debugger"


PLUGIN_VERSION_CHECK(115)
PLUGIN_SET_INFO(_("Debugger"), _("Integrated debugging with GDB."), VERSION, _("Jeff Pohlmeyer"))

GeanyData *geany_data;
GeanyFunctions *geany_functions;

static GtkNotebook *msgbook;
static GtkWidget *compwin;
static GtkWidget *frame;
static gchar *config_file;


static void show_compwin()
{
	gint page = gtk_notebook_page_num(msgbook, compwin);
	gtk_notebook_set_current_page(msgbook, page);
}



static void
info_message_cb(const gchar * msg)
{
	show_compwin();
	msgwin_compiler_add(COLOR_BLACK, "%s", msg);
}


static void
warn_message_cb(const gchar * msg)
{
	show_compwin();
	msgwin_compiler_add(COLOR_RED, "%s", msg);
}


#define NOTEBOOK GTK_NOTEBOOK(geany->main_widgets->notebook)
//#define DOCS ((document*)(doc_array->data))

//static gint
//doc_idx_to_tab_idx(gint idx)
//{
	// FIXME
//	return 0;
//	  return (
//			  (idx>=0) && ((guint)idx<doc_array->len) && DOCS[idx].is_valid
//	  ) ? gtk_notebook_page_num(NOTEBOOK, GTK_WIDGET(DOCS[idx].sci)):-1;
//}


static void
goto_file_line_cb(const gchar * filename, const gchar * line, const gchar * reason)
{
	gint pos;
	gint page;
	GeanyDocument *doc;

	gint line_num = gdbio_atoi((gchar *) line) - 1;
	if (reason)
	{
		msgwin_compiler_add(COLOR_BLUE, "%s", reason);
	}
	doc = document_open_file(filename, FALSE, NULL, NULL);
	if (!(doc && doc->is_valid))
	{
		return;
	}
	page = gtk_notebook_page_num(NOTEBOOK, GTK_WIDGET(doc->editor->sci));
	gtk_notebook_set_current_page(NOTEBOOK, page);
	pos = sci_get_position_from_line(doc->editor->sci, line_num);
	sci_ensure_line_is_visible(doc->editor->sci, line_num);
	while (gtk_events_pending())
	{
		gtk_main_iteration();
	}
	sci_set_current_position(doc->editor->sci, pos, TRUE);
	gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
	gtk_window_present(GTK_WINDOW(geany->main_widgets->window));
}


/*
static gchar *
get_current_word()
{
	const gchar *word_chars = NULL;
	gint pos, linenum, bol, bow, eow;
	gchar *text = NULL;
	gchar *rv = NULL;
	document *doc = document_get_current();
	if (!(doc && doc->is_valid))
	{
		return NULL;
	}
	pos = sci_get_current_position(doc->sci);
	linenum = sci_get_line_from_position(doc->sci, pos);
	bol = sci_get_position_from_line(doc->sci, linenum);
	bow = pos - bol;
	eow = pos - bol;
	text = sci_get_line(doc->sci, linenum);
	word_chars = GEANY_WORDCHARS;
	while ((bow > 0) && (strchr(word_chars, text[bow - 1]) != NULL))
	{
		bow--;
	}
	while (text[eow] && (strchr(word_chars, text[eow]) != NULL))
	{
		eow++;
	}
	text[eow] = '\0';
	rv = g_strdup(text + bow);
	g_free(text);
	return rv;
}*/

static gboolean word_check_left(gchar c)
{
	if (g_ascii_isalnum(c) || c == '_' || c == '.')
		return TRUE;
	return FALSE;
}

static gboolean
word_check_right(gchar c)
{
	if (g_ascii_isalnum(c) || c == '_')
		return TRUE;
	return FALSE;
}

static gchar *
get_current_word()
{
	gchar *txt;
	GeanyDocument *doc;

	gint pos;
	gint cstart, cend;
	gchar c;
	gint text_len;

	doc = document_get_current();
	g_return_val_if_fail(doc != NULL && doc->file_name != NULL, NULL);

	text_len = sci_get_selected_text_length(doc->editor->sci);
	if (text_len > 1)
	{
		txt = g_malloc(text_len + 1);
		sci_get_selected_text(doc->editor->sci, txt);
		return txt;
	}

	pos = sci_get_current_position(doc->editor->sci);
	if (pos > 0)
		pos--;

	cstart = pos;
	c = sci_get_char_at(doc->editor->sci, cstart);

	if (!word_check_left(c))
		return NULL;

	while (word_check_left(c))
	{
		cstart--;
		if (cstart >= 0)
			c = sci_get_char_at(doc->editor->sci, cstart);
		else
			break;
	}
	cstart++;

	cend = pos;
	c = sci_get_char_at(doc->editor->sci, cend);
	while (word_check_right(c) && cend < sci_get_length(doc->editor->sci))
	{
		cend++;
		c = sci_get_char_at(doc->editor->sci, cend);
	}

	if (cstart == cend)
		return NULL;
	txt = g_malloc0(cend - cstart + 1);

	sci_get_text_range(doc->editor->sci, cstart, cend, txt);
	return txt;
}




static LocationInfo *
location_query_cb()
{
	GeanyDocument *doc = document_get_current();
	if (!(doc && doc->is_valid))
	{
		return NULL;
	}
	if (doc->file_name)
	{
		LocationInfo *abi;
		gint line;
		abi = g_new0(LocationInfo, 1);
		line = sci_get_current_line(doc->editor->sci);
		abi->filename = g_strdup(doc->file_name);
		if (line >= 0)
		{
			abi->line_num = g_strdup_printf("%d", line + 1);
		}
		abi->symbol = get_current_word();
		return abi;
	}
	return NULL;
}



static void
update_settings_cb()
{
	GKeyFile *kf = g_key_file_new();
	gchar *data;

	g_key_file_set_string(kf, UNIX_NAME, "mono_font", gdbui_setup.options.mono_font);
	g_key_file_set_string(kf, UNIX_NAME, "term_cmd", gdbui_setup.options.term_cmd);
	g_key_file_set_boolean(kf, UNIX_NAME, "show_tooltips", gdbui_setup.options.show_tooltips);
	g_key_file_set_boolean(kf, UNIX_NAME, "show_icons", gdbui_setup.options.show_icons);

	data = g_key_file_to_data(kf, NULL, NULL);
	utils_write_file(config_file, data);
	g_free(data);

	g_key_file_free(kf);
	gtk_widget_destroy(GTK_BIN(frame)->child);
	gdbui_create_widgets(frame);
	gtk_widget_show_all(frame);
}




#define CLEAR() if (err) { g_error_free(err); err=NULL; }

#define GET_KEY_STR(k) { \
  gchar *tmp=g_key_file_get_string(kf,UNIX_NAME,#k"",&err); \
  if (tmp) { \
	if (*tmp && !err) { \
	  g_free(gdbui_setup.options.k); \
	  gdbui_setup.options.k=tmp; \
	} else { g_free(tmp); } \
  } \
  CLEAR(); \
}


static void locale_init(void)
{
#ifdef ENABLE_NLS
	gchar *locale_dir = NULL;

#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif

#ifdef G_OS_WIN32
	gchar *install_dir = g_win32_get_package_installation_directory("geany", NULL);
	/* e.g. C:\Program Files\geany\lib\locale */
	locale_dir = g_strconcat(install_dir, "\\share\\locale", NULL);
	g_free(install_dir);
#else
	locale_dir = g_strdup(LOCALEDIR);
#endif

	bindtextdomain(GETTEXT_PACKAGE, locale_dir);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	g_free(locale_dir);
#endif
}


#define GET_KEY_BOOL(k) { \
  gboolean tmp=g_key_file_get_boolean(kf,UNIX_NAME,#k"",&err); \
  if (err) { CLEAR() } else { gdbui_setup.options.k=tmp; } \
}

void
plugin_init(GeanyData * data)
{
	GKeyFile *kf = g_key_file_new();
	GError *err = NULL;
	gchar *glob_file;
	gchar *user_file;
	gchar *old_config_dir;

	locale_init();

	gdbui_setup.main_window = geany->main_widgets->window;

	gdbio_setup.temp_dir = g_build_filename(geany->app->configdir, "plugins", UNIX_NAME, NULL);
	old_config_dir = g_build_filename(geany->app->configdir, "plugins", UNIX_NAME_OLD, NULL);

	if (g_file_test(old_config_dir, G_FILE_TEST_IS_DIR)
			&& !g_file_test(gdbio_setup.temp_dir, G_FILE_TEST_EXISTS))
		g_rename(old_config_dir, gdbio_setup.temp_dir);

	/*
	 * the tty helper binary is either in the user's config dir or globally
	 * installed in $LIBDIR/geany/
	 */
	glob_file = g_build_filename(LIBDIR, "geany", "ttyhelper", NULL);
	user_file = g_build_filename(geany->app->configdir, "plugins", UNIX_NAME, "ttyhelper", NULL);
	gdbio_setup.tty_helper = NULL;

	if (utils_mkdir(gdbio_setup.temp_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
					   _("Plugin configuration directory (%s) could not be created."), gdbio_setup.temp_dir);
	}

	/* the global ttyhelper has higher priority */
	if (!g_file_test(glob_file, G_FILE_TEST_IS_REGULAR))
	{
		if (g_file_test(user_file, G_FILE_TEST_IS_REGULAR)
				&& g_file_test(user_file, G_FILE_TEST_IS_EXECUTABLE))
		{
			gdbio_setup.tty_helper = g_strdup(user_file);
		}
	}
	else if (g_file_test(glob_file, G_FILE_TEST_IS_EXECUTABLE))
		gdbio_setup.tty_helper = g_strdup(glob_file);

	if (NULL == gdbio_setup.tty_helper)
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
					   _("geanygdb: ttyhelper program not found."));

	config_file = g_build_filename(gdbio_setup.temp_dir,"debugger.cfg", NULL);
	gdbui_opts_init();

	if (g_key_file_load_from_file(kf, config_file, G_KEY_FILE_NONE, NULL))
	{
		GET_KEY_STR(mono_font);
		GET_KEY_STR(term_cmd);
		GET_KEY_BOOL(show_tooltips);
		GET_KEY_BOOL(show_icons);
	}

	g_key_file_free(kf);

	gdbui_setup.warn_func = warn_message_cb;
	gdbui_setup.info_func = info_message_cb;
	gdbui_setup.opts_func = update_settings_cb;
	gdbui_setup.location_query = location_query_cb;
	gdbui_setup.line_func = goto_file_line_cb;


	g_free(old_config_dir);
	g_free(glob_file);
	g_free(user_file);
	msgbook = GTK_NOTEBOOK(ui_lookup_widget(geany->main_widgets->window, "notebook_info"));
	compwin = gtk_widget_get_parent(ui_lookup_widget(geany->main_widgets->window, "treeview5"));
	frame = gtk_frame_new(NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), frame,
				 gtk_label_new("Debug"));
	gdbui_set_tips(GTK_TOOLTIPS(ui_lookup_widget(geany->main_widgets->window, "tooltips")));
	gdbui_create_widgets(frame);
	gtk_widget_show_all(frame);
}


void
plugin_cleanup()
{
	gdbio_exit();
	update_settings_cb();

	g_free(config_file);
	g_free(gdbio_setup.temp_dir);
	g_free(gdbio_setup.tty_helper);

	gtk_widget_destroy(frame);
	gdbui_opts_done();
}


GtkWidget *
plugin_configure(G_GNUC_UNUSED GtkWidget * parent)
{
	gdbui_opts_dlg();
	return NULL;
}
