/*
 *  prefs.c
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <string.h>

#include "common.h"

#ifdef G_OS_UNIX
#include <vte/vte.h>
#endif

gchar *pref_gdb_executable;
gboolean pref_gdb_async_mode;
#ifndef G_OS_UNIX
gboolean pref_async_break_bugs;
#endif
gboolean pref_var_update_bug;

gboolean pref_auto_view_source;
gboolean pref_keep_exec_point;
gint pref_visual_beep_length;
#ifdef G_OS_UNIX
gboolean pref_debug_console_vte;
#endif

gint pref_sci_marker_first;
static gint pref_sci_marker_1st;
gint pref_sci_caret_policy;
gint pref_sci_caret_slop;
gboolean pref_unmark_current_line;

gboolean pref_scope_goto_cursor;
gboolean pref_seek_with_navqueue;
gint pref_panel_tab_pos;
gint pref_show_recent_items;
gint pref_show_toolbar_items;

gint pref_tooltips_fail_action;
gint pref_tooltips_send_delay;
gint pref_tooltips_length;

gint pref_memory_bytes_per_line;
gchar *pref_memory_font;

#ifdef G_OS_UNIX
static gboolean pref_terminal_save_pos;
gboolean pref_terminal_padding;
gint pref_terminal_window_x;
gint pref_terminal_window_y;
gint pref_terminal_width;
gint pref_terminal_height;
#endif  /* G_OS_UNIX */

gboolean pref_vte_blinken;
gchar *pref_vte_emulation;
gchar *pref_vte_font;
gint pref_vte_scrollback;

#if !GTK_CHECK_VERSION(3, 14, 0)
GdkColor pref_vte_colour_fore;
GdkColor pref_vte_colour_back;
#else
GdkRGBA pref_vte_colour_fore;
GdkRGBA pref_vte_colour_back;
#endif

typedef struct _MarkerStyle
{
	const gchar *name;
	gint mark;
	gint fore;
	gint back;
	gint alpha;
	gint default_mark;
	const gchar *default_fore;
	const gchar *default_back;
	guint default_alpha;
} MarkerStyle;

#define MARKER_COUNT 3

static MarkerStyle pref_marker_styles[MARKER_COUNT] =
{
	{ "disabled_break", 0, 0, 0, 0, SC_MARK_CIRCLE,     "#008000", "#C0E0D0", 256 },
	{ "enabled_break",  0, 0, 0, 0, SC_MARK_CIRCLE,     "#000080", "#C0D0F0", 256 },
	{ "execution_line", 0, 0, 0, 0, SC_MARK_SHORTARROW, "#808000", "#F0F090", 256 }
};

void prefs_apply(GeanyDocument *doc)
{
	gint i;
	ScintillaObject *sci = doc->editor->sci;
	MarkerStyle *style = pref_marker_styles;

	for (i = pref_sci_marker_first; i < pref_sci_marker_first + MARKER_COUNT; i++, style++)
	{
		scintilla_send_message(sci, SCI_MARKERDEFINE, i, style->mark);
		scintilla_send_message(sci, SCI_MARKERSETFORE, i, style->fore);
		scintilla_send_message(sci, SCI_MARKERSETBACK, i, style->back);
		scintilla_send_message(sci, SCI_MARKERSETALPHA, i, style->alpha);
	}
}

static StashGroup *scope_group;
static StashGroup *terminal_group;
static StashGroup *marker_group[MARKER_COUNT];

static void load_scope_prefs(GKeyFile *config)
{
	guint i;
	MarkerStyle *style = pref_marker_styles;

	stash_group_load_from_key_file(scope_group, config);
	stash_group_load_from_key_file(terminal_group, config);

	for (i = 0; i < MARKER_COUNT; i++, style++)
	{
		gchar *tmp_string;

		stash_group_load_from_key_file(marker_group[i], config);
		tmp_string = utils_get_setting_string(config, style->name, "fore",
			style->default_fore);
		style->fore = utils_parse_sci_color(tmp_string);
		g_free(tmp_string);
		tmp_string = utils_get_setting_string(config, style->name, "back",
			style->default_back);
		style->back = utils_parse_sci_color(tmp_string);
		g_free(tmp_string);
	}
}

static const char *obsolete_prefs[] = { "gdb_buffer_length", "gdb_wait_death",
	"gdb_send_interval", NULL };

static void save_scope_prefs(GKeyFile *config)
{
	guint i;
	MarkerStyle *style = pref_marker_styles;

	stash_group_save_to_key_file(scope_group, config);
	stash_group_save_to_key_file(terminal_group, config);

	for (i = 0; i < MARKER_COUNT; i++, style++)
	{
		gchar *tmp_string;

		stash_group_save_to_key_file(marker_group[i], config);
		tmp_string = g_strdup_printf("#%02X%02X%02X", style->fore & 0xFF,
			(style->fore >> 8) & 0xFF, style->fore >> 16);
		g_key_file_set_string(config, style->name, "fore", tmp_string);
		g_free(tmp_string);
		tmp_string = g_strdup_printf("#%02X%02X%02X", style->back & 0xFF,
			(style->back >> 8) & 0xFF, style->back >> 16);
		g_key_file_set_string(config, style->name, "back", tmp_string);
		g_free(tmp_string);
	}

	for (i = 0; obsolete_prefs[i]; i++)
		g_key_file_remove_key(config, "scope", obsolete_prefs[i], NULL);
}

static void prefs_configure(void)
{
	static const char *const view_source_items[] =
	{
		"thread_view_source",
		"break_view_source",
		"stack_separator1",
		"stack_view_source",
		NULL
	};

	const char *const *p;
	guint i;

	for (p = view_source_items; *p; p++)
		gtk_widget_set_visible(get_widget(*p), !pref_auto_view_source);

	foreach_document(i)
		prefs_apply(documents[i]);

	configure_panel();
}

char *prefs_file_name(void)
{
	return g_build_filename(geany->app->configdir, "plugins", "scope", "scope.conf", NULL);
}

static void on_document_save(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer gdata)
{
	char *configfile = prefs_file_name();

	if (doc->real_path && !utils_filenamecmp(doc->real_path, configfile))
	{
		GKeyFile *config = g_key_file_new();

		g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);
		load_scope_prefs(config);
		prefs_configure();
		configure_toolbar();
		g_key_file_free(config);
	}
	g_free(configfile);
}

static GtkWidget *config_item;

void prefs_init(void)
{
	guint i;
	MarkerStyle *style = pref_marker_styles;
	StashGroup *group;
	char *configdir = g_build_filename(geany->app->configdir, "plugins", "scope", NULL);
	char *configfile = prefs_file_name();
	GKeyFile *config = g_key_file_new();
	gboolean obsolete = FALSE;

	group = stash_group_new("scope");
	stash_group_add_string(group, &pref_gdb_executable, "gdb_executable", "gdb");
	stash_group_add_boolean(group, &pref_gdb_async_mode, "gdb_async_mode", FALSE);
#ifndef G_OS_UNIX
	stash_group_add_boolean(group, &pref_async_break_bugs, "async_break_bugs", TRUE);
#endif
	stash_group_add_boolean(group, &pref_var_update_bug, "var_update_bug", TRUE);
	stash_group_add_boolean(group, &pref_auto_view_source, "auto_view_source", FALSE);
	stash_group_add_boolean(group, &pref_keep_exec_point, "keep_exec_point", FALSE);
	stash_group_add_integer(group, &pref_visual_beep_length, "visual_beep_length", 25);
#ifdef G_OS_UNIX
	stash_group_add_boolean(group, &pref_debug_console_vte, "debug_console_vte", TRUE);
#endif
	stash_group_add_integer(group, &pref_sci_marker_1st, "sci_marker_first", 17);
	stash_group_add_integer(group, &pref_sci_caret_policy, "sci_caret_policy", CARET_SLOP |
		CARET_JUMPS | CARET_EVEN);
	stash_group_add_integer(group, &pref_sci_caret_slop, "sci_caret_slop", 3);
	stash_group_add_boolean(group, &pref_unmark_current_line, "unmark_current_line", FALSE);
	stash_group_add_boolean(group, &pref_scope_goto_cursor, "scope_run_to_cursor", FALSE);
	stash_group_add_boolean(group, &pref_seek_with_navqueue, "seek_with_navqueue", FALSE);
	stash_group_add_integer(group, &pref_panel_tab_pos, "panel_tab_pos", GTK_POS_TOP);
	stash_group_add_integer(group, &pref_show_recent_items, "show_recent_items", 10);
	stash_group_add_integer(group, &pref_show_toolbar_items, "show_toolbar_items", 0xFF);
	stash_group_add_integer(group, &pref_tooltips_fail_action, "tooltips_fail_action", 0);
	stash_group_add_integer(group, &pref_tooltips_send_delay, "tooltips_send_delay", 25);
	stash_group_add_integer(group, &pref_tooltips_length, "tooltips_length", 2048);
	stash_group_add_integer(group, &pref_memory_bytes_per_line, "memory_line_bytes", 16);
	stash_group_add_string(group, &pref_memory_font, "memory_font", "");
	scope_group = group;

	config_item = ui_add_config_file_menu_item(configfile, NULL, NULL);
	plugin_signal_connect(geany_plugin, NULL, "document-save", FALSE,
		G_CALLBACK(on_document_save), NULL);

	group = stash_group_new("terminal");
#ifdef G_OS_UNIX
	stash_group_add_boolean(group, &pref_terminal_save_pos, "save_pos", TRUE);
	stash_group_add_boolean(group, &pref_terminal_padding, "padding", TRUE);
	stash_group_add_integer(group, &pref_terminal_window_x, "window_x", 70);
	stash_group_add_integer(group, &pref_terminal_window_y, "window_y", 50);
	stash_group_add_integer(group, &pref_terminal_width, "width", 640);
	stash_group_add_integer(group, &pref_terminal_height, "height", 480);
#endif  /* G_OS_UNIX */
	terminal_group = group;

	for (i = 0; i < MARKER_COUNT; i++, style++)
	{
		group = stash_group_new(style->name);
		stash_group_add_integer(group, &style->mark, "mark", style->default_mark);
		stash_group_add_integer(group, &style->alpha, "alpha", style->default_alpha);
		marker_group[i] = group;
	}

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);
	load_scope_prefs(config);

	for (i = 0; obsolete_prefs[i]; i++)
	{
		GError *gerror = NULL;

		g_key_file_get_integer(config, "scope", obsolete_prefs[i], &gerror);

		if (gerror)
		{
			g_error_free(gerror);
			gerror = NULL;
		}
		else
		{
			obsolete = TRUE;
			break;
		}
	}

	pref_sci_marker_first = pref_sci_marker_1st;
	prefs_configure();
	program_load_config(config);

	if (obsolete || !g_file_test(configfile, G_FILE_TEST_IS_REGULAR))
	{
		gint error = utils_mkdir(configdir, TRUE);

		if (error)
			msgwin_status_add(_("Scope: %s: %s."), configdir, g_strerror(error));
		else
		{
			save_scope_prefs(config);
			if (utils_key_file_write_to_file(config, configfile))
				msgwin_status_add(_("Scope: created configuration file."));
		}
	}

	g_key_file_free(config);
	g_free(configfile);
	g_free(configdir);
}

void prefs_finalize(void)
{
	guint i;

#ifdef G_OS_UNIX
	if (pref_terminal_save_pos)
	{
		char *configfile = prefs_file_name();
		stash_group_save_to_file(terminal_group, configfile, G_KEY_FILE_KEEP_COMMENTS);
		g_free(configfile);
	}

	g_free(pref_vte_font);
	g_free(pref_vte_emulation);
#endif  /* G_OS_UNIX */

	gtk_widget_destroy(config_item);
	utils_stash_group_free(scope_group);
	utils_stash_group_free(terminal_group);
	for (i = 0; i < MARKER_COUNT; i++)
		utils_stash_group_free(marker_group[i]);
}
