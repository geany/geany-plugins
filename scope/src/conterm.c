/*
 *  conterm.c
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
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "common.h"

#define NFD 5
#define DS_COPY (DS_BASICS | DS_EXTRA_1)

#ifdef G_OS_UNIX
#include <vte/vte.h>
#include <gp_vtecompat.h>
/* instead of detecting N kinds of *nix */
#if defined(HAVE_UTIL_H)
#include <util.h>
#elif defined(HAVE_LIBUTIL_H)
#include <libutil.h>
#elif defined(HAVE_PTY_H)
#include <pty.h>
#endif
int grantpt(int fd);
int unlockpt(int fd);

static GtkWidget *program_window;
static VteTerminal *program_terminal;
static GtkWidget *terminal_parent;
static GtkWidget *terminal_window;
static GtkCheckMenuItem *terminal_show;

void on_terminal_show(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkWidget *terminal = GTK_WIDGET(program_terminal);

	if (gtk_check_menu_item_get_active(terminal_show))
	{
		gtk_container_remove(GTK_CONTAINER(program_window), terminal);
		gtk_widget_set_size_request(terminal, pref_terminal_width, pref_terminal_height);
		gtk_container_add(GTK_CONTAINER(terminal_window), terminal);
		gtk_widget_show(terminal_parent);
		gtk_window_move(GTK_WINDOW(terminal_parent), pref_terminal_window_x,
			pref_terminal_window_y);
	}
	else
	{
		gtk_window_get_position(GTK_WINDOW(terminal_parent), &pref_terminal_window_x,
			&pref_terminal_window_y);
		gtk_widget_get_size_request(terminal, &pref_terminal_width, &pref_terminal_height);
		gtk_widget_hide(terminal_parent);
		gtk_container_remove(GTK_CONTAINER(terminal_window), terminal);
		gtk_widget_set_size_request(terminal, -1, -1);
		gtk_container_add(GTK_CONTAINER(program_window), terminal);
	}
}

void terminal_clear(void)
{
	vte_terminal_reset(program_terminal, TRUE, TRUE);
}

void terminal_standalone(gboolean alone)
{
	gtk_check_menu_item_set_active(terminal_show, alone);
}

static gboolean on_terminal_parent_delete(G_GNUC_UNUSED GtkWidget *widget,
	G_GNUC_UNUSED GdkEvent *event, G_GNUC_UNUSED gpointer gdata)
{
	terminal_standalone(FALSE);
	return TRUE;
}

static void on_terminal_copy(G_GNUC_UNUSED const MenuItem *menu_item)
{
	vte_terminal_copy_clipboard(program_terminal);
}

static void on_terminal_paste(G_GNUC_UNUSED const MenuItem *menu_item)
{
	vte_terminal_paste_clipboard(program_terminal);
}

static void on_terminal_feed(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gdouble value = 4;

	if (dialogs_show_input_numeric(_("Feed Terminal"), _("Enter char # (0..255):"), &value,
		0, 255, 1))
	{
		char text = (char) value;
		vte_terminal_feed_child(program_terminal, &text, 1);
	}
}

static void on_terminal_select_all(G_GNUC_UNUSED const MenuItem *menu_item)
{
	vte_terminal_select_all(program_terminal);
}

static void on_terminal_clear(G_GNUC_UNUSED const MenuItem *menu_item)
{
	vte_terminal_reset(program_terminal, TRUE, TRUE);
}

/* show terminal on program startup */
gboolean terminal_auto_show;
/* hide terminal on program exit */
gboolean terminal_auto_hide;
/* show terminal on program exit with non-zero exit code */
gboolean terminal_show_on_error;

static MenuItem terminal_menu_items[] =
{
	{ "terminal_copy",          on_terminal_copy,         DS_COPY, NULL, NULL },
	{ "terminal_paste",         on_terminal_paste,        0, NULL, NULL },
	{ "terminal_feed",          on_terminal_feed,         0, NULL, NULL },
	{ "terminal_select_all",    on_terminal_select_all,   0, NULL, NULL },
	{ "terminal_clear",         on_terminal_clear,        0, NULL, NULL },
	{ "terminal_show_hide",     on_menu_display_booleans, 0, NULL, GINT_TO_POINTER(3) },
	{ "terminal_auto_show",     on_menu_update_boolean,   0, NULL, &terminal_auto_show },
	{ "terminal_auto_hide",     on_menu_update_boolean,   0, NULL, &terminal_auto_hide },
	{ "terminal_show_on_error", on_menu_update_boolean,   0, NULL, &terminal_show_on_error },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint terminal_menu_extra_state(void)
{
	return vte_terminal_get_has_selection(program_terminal) << DS_INDEX_1;
}

static MenuInfo terminal_menu_info = { terminal_menu_items, terminal_menu_extra_state, 0 };

void on_vte_realize(VteTerminal *vte, G_GNUC_UNUSED gpointer gdata)
{
	vte_terminal_set_emulation(vte, pref_vte_emulation);
	vte_terminal_set_font_from_string(vte, pref_vte_font);
	vte_terminal_set_scrollback_lines(vte, pref_vte_scrollback);
	vte_terminal_set_scroll_on_output(vte, TRUE);
	vte_terminal_set_color_foreground(vte, &pref_vte_colour_fore);
	vte_terminal_set_color_background(vte, &pref_vte_colour_back);
#if VTE_CHECK_VERSION(0, 17, 1)
	vte_terminal_set_cursor_blink_mode(vte,
		pref_vte_blinken ? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF);
#else
	vte_terminal_set_cursor_blinks(vte, pref_vte_blinken);
#endif
}

static VteTerminal *debug_console = NULL;  /* NULL -> GtkTextView "context" */

static void console_output(int fd, const char *text, gint length)
{
	static const char fd_colors[NFD] = { '6', '7', '1', '7', '5' };
	static char setaf[5] = { '\033', '[', '3', '?', 'm' };
	static int last_fd = -1;
	gint i;

	if (last_fd == 3 && fd != 0)
		vte_terminal_feed(debug_console, "\r\n", 2);

	if (fd != last_fd)
	{
		setaf[3] = fd_colors[fd];
		vte_terminal_feed(debug_console, setaf, sizeof setaf);
		last_fd = fd;
	}

	if (length == -1)
		length = strlen(text);

	for (i = 0; i < length; i++)
	{
		if (text[i] == '\n')
		{
			vte_terminal_feed(debug_console, text, i);
			vte_terminal_feed(debug_console, "\r", 2);
			length -= i;
			text += i;
			i = 0;
		}
	}

	vte_terminal_feed(debug_console, text, length);
}

static void console_output_nl(int fd, const char *text, gint length)
{
	dc_output(fd, text, length);
	vte_terminal_feed(debug_console, "\r\n", 2);
}
#endif  /* G_OS_UNIX */

static GtkTextView *debug_context;
static GtkTextBuffer *context;
static GtkTextTag *fd_tags[NFD];
#define DC_LIMIT 32768  /* approx */
#define DC_DELTA 6144
static guint dc_chars = 0;

void context_output(int fd, const char *text, gint length)
{
	static int last_fd = -1;
	GtkTextIter end;
	gchar *utf8;

	gtk_text_buffer_get_end_iter(context, &end);

	if (last_fd == 3 && fd != 0)
		gtk_text_buffer_insert(context, &end, "\n", 1);

	if (fd != last_fd)
		last_fd = fd;

	if (length == -1)
		length = strlen(text);

	dc_chars += length;
	utf8 = g_locale_to_utf8(text, length, NULL, NULL, NULL);

	if (utf8)
	{
		gtk_text_buffer_insert_with_tags(context, &end, utf8, -1, fd_tags[fd], NULL);
		g_free(utf8);
	}
	else
		gtk_text_buffer_insert_with_tags(context, &end, text, length, fd_tags[fd], NULL);

	if (dc_chars > DC_LIMIT + (DC_DELTA / 2))
	{
		GtkTextIter start, delta;

		gtk_text_buffer_get_start_iter(context, &start);
		gtk_text_buffer_get_iter_at_offset(context, &delta, DC_DELTA);
		gtk_text_buffer_delete(context, &start, &delta);
		gtk_text_buffer_get_end_iter(context, &end);
		dc_chars = gtk_text_buffer_get_char_count(context);
	}

	gtk_text_buffer_place_cursor(context, &end);
      gtk_text_view_scroll_mark_onscreen(debug_context, gtk_text_buffer_get_insert(context));
}

void context_output_nl(int fd, const char *text, gint length)
{
	dc_output(fd, text, length);
	dc_output(fd, "\n", 1);
}

static gboolean on_console_button_3_press(G_GNUC_UNUSED GtkWidget *widget,
	GdkEventButton *event, GtkMenu *menu)
{
	if (event->button == 3)
	{
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

void (*dc_output)(int fd, const char *text, gint length);
void (*dc_output_nl)(int fd, const char *text, gint length);

void dc_error(const char *format, ...)
{
	char *string;
	va_list args;

	va_start(args, format);
	string = g_strdup_vprintf(format, args);
	va_end(args);

	dc_output_nl(4, string, -1);
	g_free(string);
	plugin_blink();
}

void dc_clear(void)
{
#ifdef G_OS_UNIX
	if (debug_console)
		vte_terminal_reset(debug_console, TRUE, TRUE);
	else
#endif
	{
		gtk_text_buffer_set_text(context, "", -1);
		dc_chars = 0;
	}
}

gboolean dc_update(void)
{
	if (thread_state == THREAD_AT_ASSEMBLER)
		debug_send_format(T, "04-data-disassemble -s $pc -e $pc+1 0");

	return TRUE;
}

static void on_console_copy(G_GNUC_UNUSED const MenuItem *menu_item)
{
#ifdef G_OS_UNIX
	if (debug_console)
		vte_terminal_copy_clipboard(debug_console);
	else
#endif
	{
     		g_signal_emit_by_name(debug_context, "copy-clipboard");
     	}
}

static void on_console_select_all(G_GNUC_UNUSED const MenuItem *menu_item)
{
#ifdef G_OS_UNIX
	if (debug_console)
		vte_terminal_select_all(program_terminal);
	else
#endif
	{
		g_signal_emit_by_name(debug_context, "select-all");
	}
}

static void on_console_clear(G_GNUC_UNUSED const MenuItem *menu_item)
{
	dc_clear();
}

static gboolean on_console_key_press(G_GNUC_UNUSED GtkWidget *widget,
	GdkEventKey *event, G_GNUC_UNUSED gpointer gdata)
{
	gboolean insert = event->keyval == GDK_Insert || event->keyval == GDK_KP_Insert;

	if ((insert || (event->keyval >= 0x21 && event->keyval <= 0x7F &&
		event->state <= GDK_SHIFT_MASK)) && (debug_state() & DS_ACTIVE))
	{
		char command[2] = { event->keyval, '\0' };
		view_command_line(insert ? NULL : command, NULL, NULL, TRUE);
		return TRUE;
	}

	return FALSE;
}

static MenuItem console_menu_items[] =
{
	{ "console_copy",       on_console_copy,       DS_COPY, NULL, NULL },
	{ "console_select_all", on_console_select_all, 0,       NULL, NULL },
	{ "console_clear",      on_console_clear,      0,       NULL, NULL },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint console_menu_extra_state(void)
{
#ifdef G_OS_UNIX
	if (debug_console)
		return vte_terminal_get_has_selection(debug_console) << DS_INDEX_1;
#endif
	return gtk_text_buffer_get_has_selection(context) << DS_INDEX_1;
}

static MenuInfo console_menu_info = { console_menu_items, console_menu_extra_state, 0 };

void conterm_load_config(void)
{
	gchar *configfile = g_build_filename(geany_data->app->configdir, "geany.conf", NULL);
	GKeyFile *config = g_key_file_new();
	gchar *tmp_string;

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);
	pref_vte_blinken = utils_get_setting_boolean(config, "VTE", "cursor_blinks", FALSE);
	pref_vte_emulation = utils_get_setting_string(config, "VTE", "emulation", "xterm");
	pref_vte_font = utils_get_setting_string(config, "VTE", "font", "Monospace 10");
	pref_vte_scrollback = utils_get_setting_integer(config, "VTE", "scrollback_lines", 500);
	tmp_string = utils_get_setting_string(config, "VTE", "colour_fore", "#ffffff");
#if !GTK_CHECK_VERSION(3, 14, 0)
	gdk_color_parse(tmp_string, &pref_vte_colour_fore);
#else
	gdk_rgba_parse(&pref_vte_colour_fore, tmp_string);
#endif
	g_free(tmp_string);
	tmp_string = utils_get_setting_string(config, "VTE", "colour_back", "#000000");
#if !GTK_CHECK_VERSION(3, 14, 0)
	gdk_color_parse(tmp_string, &pref_vte_colour_back);
#else
	gdk_rgba_parse(&pref_vte_colour_back, tmp_string);
#endif
	g_free(tmp_string);
	g_key_file_free(config);
	g_free(configfile);
}

static void context_apply_config(GtkWidget *console)
{
#if !GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_modify_base(console, GTK_STATE_NORMAL, &pref_vte_colour_back);
	gtk_widget_modify_cursor(console, &pref_vte_colour_fore, &pref_vte_colour_back);
#else
	GString *css_string;
	GtkStyleContext *context;
	GtkCssProvider *provider;
	gchar *css_code, *color, *background_color;

	color = gdk_rgba_to_string (&pref_vte_colour_fore);
	background_color = gdk_rgba_to_string (&pref_vte_colour_back);

	gtk_widget_set_name(console, "scope-console");
	context = gtk_widget_get_style_context(console);
	provider = gtk_css_provider_new();
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	css_string = g_string_new(NULL);
	g_string_printf(css_string, "#scope-console { color: %s; background-color: %s; }",
		color, background_color);
	css_code = g_string_free(css_string, FALSE);

	gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider), css_code, -1, NULL);

	g_free(css_code);
	g_object_unref(provider);
#endif
	ui_widget_modify_font_from_string(console, pref_vte_font);
}

void conterm_apply_config(void)
{
#ifdef G_OS_UNIX
	on_vte_realize(program_terminal, NULL);

	if (debug_console)
		on_vte_realize(debug_console, NULL);
	else
#endif
	{
		context_apply_config(GTK_WIDGET(debug_context));
	}
}

#ifdef G_OS_UNIX
static int pty_slave = -1;
char *slave_pty_name = NULL;
#endif

void conterm_init(void)
{
	GtkWidget *console;
#ifdef G_OS_UNIX
	gchar *error = NULL;
	int pty_master;
	char *pty_name;
#endif

	conterm_load_config();
#ifdef G_OS_UNIX
	program_window = get_widget("program_window");
	console = vte_terminal_new();
	gtk_widget_show(console);
	program_terminal = VTE_TERMINAL(console);
	g_object_ref(program_terminal);
	gtk_container_add(GTK_CONTAINER(program_window), console);
	g_signal_connect_after(program_terminal, "realize", G_CALLBACK(on_vte_realize), NULL);
	terminal_parent = get_widget("terminal_parent");
	g_signal_connect(terminal_parent, "delete-event", G_CALLBACK(on_terminal_parent_delete),
		NULL);
	terminal_window = get_widget("terminal_window");
	terminal_show = GTK_CHECK_MENU_ITEM(get_widget("terminal_show"));

	if (pref_terminal_padding)
	{
		gint vte_border_x, vte_border_y;

#if GTK_CHECK_VERSION(3, 4, 0)
		GtkStyleContext *context;
		GtkBorder border;

		context = gtk_widget_get_style_context (console);
		gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &border);
		vte_border_x = border.left + border.right;
		vte_border_y = border.top + border.bottom;
#elif VTE_CHECK_VERSION(0, 24, 0)
		GtkBorder *border = NULL;

		gtk_widget_style_get(console, "inner-border", &border, NULL);

		if (border)
		{
			vte_border_x = border->left + border->right;
			vte_border_y = border->top + border->bottom;
			gtk_border_free(border);
		}
		else
			vte_border_x = vte_border_y = 2;
#else  /* VTE 0.24.0 */
		/* VTE manual says "deprecated since 0.26", but it's since 0.24 */
		vte_terminal_get_padding(program_terminal, &vte_border_x, &vte_border_y);
#endif  /* VTE 0.24.0 */
		pref_terminal_width += vte_border_x;
		pref_terminal_height += vte_border_y;
		pref_terminal_padding = FALSE;
	}

	if (openpty(&pty_master, &pty_slave, NULL, NULL, NULL) == 0 &&
		grantpt(pty_master) == 0 && unlockpt(pty_master) == 0 &&
		(pty_name = ttyname(pty_slave)) != NULL)
	{
#if VTE_CHECK_VERSION(0, 25, 0)
		GError *gerror = NULL;
		VtePty *pty = vte_pty_new_foreign(pty_master, &gerror);

		if (pty)
		{
			vte_terminal_set_pty_object(program_terminal, pty);
			slave_pty_name = g_strdup(pty_name);
		}
		else
		{
			error = g_strdup(gerror->message);
			g_error_free(gerror);
		}
#else  /* VTE 0.25.0 */
		vte_terminal_set_pty(program_terminal, pty_master);
		slave_pty_name = g_strdup(pty_name);
#endif  /* VTE 0.25.0 */
	}
	else
		error = g_strdup_printf("pty: %s", g_strerror(errno));

	if (error)
	{
		gtk_widget_set_sensitive(program_window, FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(terminal_show), FALSE);
		msgwin_status_add(_("Scope: %s."), error);
		g_free(error);
	}
	else
		menu_connect("terminal_menu", &terminal_menu_info, GTK_WIDGET(program_terminal));
#else  /* G_OS_UNIX */
	gtk_widget_hide(get_widget("program_window"));
#endif  /* G_OS_UNIX */

#ifdef G_OS_UNIX
	if (pref_debug_console_vte)
	{
		console = vte_terminal_new();
		gtk_widget_show(console);
		debug_console = VTE_TERMINAL(console);
		dc_output = console_output;
		dc_output_nl = console_output_nl;
		g_signal_connect_after(debug_console, "realize", G_CALLBACK(on_vte_realize), NULL);
		menu_connect("console_menu", &console_menu_info, console);
	}
	else
#endif  /* G_OS_UNIX */
	{
		static const char *const colors[NFD] = { "#00C0C0", "#C0C0C0", "#C00000",
			"#C0C0C0", "#C000C0" };
		guint i;

		console = get_widget("debug_context");
		context_apply_config(console);
		debug_context = GTK_TEXT_VIEW(console);
		dc_output = context_output;
		dc_output_nl = context_output_nl;
		context = gtk_text_view_get_buffer(debug_context);

		for (i = 0; i < NFD; i++)
		{
			fd_tags[i] = gtk_text_buffer_create_tag(context, NULL, "foreground",
				colors[i], NULL);
		}
		g_signal_connect(console, "button-press-event",
			G_CALLBACK(on_console_button_3_press),
			menu_connect("console_menu", &console_menu_info, NULL));
	}

	gtk_container_add(GTK_CONTAINER(get_widget("debug_window")), console);
	g_signal_connect(console, "key-press-event", G_CALLBACK(on_console_key_press), NULL);
}

void conterm_finalize(void)
{
#ifdef G_OS_UNIX
	g_object_unref(program_terminal);
	g_free(slave_pty_name);
	close(pty_slave);
	/* close(pty_master) causes 100% CPU load on 0.24 and is not allowed on 0.26+ */
#endif  /* G_OS_UNIX */
}
