/*
 *  debug.c
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

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "common.h"
#include "spawn.h"

extern guint thread_count;
extern guint thread_prompt;

typedef enum _GdbState
{
	INACTIVE,
	ACTIVE,
	KILLING
} GdbState;

static GdbState gdb_state = INACTIVE;
static GPid gdb_pid;

static gboolean wait_prompt;
static GString *commands;

DebugState debug_state(void)
{
	DebugState state;

	if (gdb_state == INACTIVE)
		state = DS_INACTIVE;
	else if (gdb_state == KILLING || wait_prompt || commands->len)
		state = DS_BUSY;
	else if (thread_count)
	{
		if (thread_state <= THREAD_RUNNING)
			state = pref_gdb_async_mode || thread_prompt ? DS_READY : DS_BUSY;
		else
			state = DS_DEBUG;
	}
	else /* at prompt, no threads */
		state = DS_HANGING;

	return state;
}


void on_debug_list_source(GArray *nodes)
{
	ParseLocation loc;

	parse_location(nodes, &loc);

	iff (loc.line, "no line or abs file")
		debug_send_format(N, "02-break-insert -t %s:%d\n05", loc.file, loc.line);

	parse_location_free(&loc);
}

static gboolean debug_auto_run;
static gboolean debug_auto_exit;
static gboolean debug_load_error;

void on_debug_error(GArray *nodes)
{
	debug_auto_run = FALSE;  /* may be an initialization command failure */
	on_error(nodes);
}

void on_debug_loaded(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);

	if (!debug_load_error && (*token + !*program_load_script >= '1'))
	{
		breaks_apply();
		inspects_apply();
		view_dirty(VIEW_WATCHES);

		if (program_temp_breakpoint)
		{
			if (*program_temp_break_location)
			{
				debug_send_format(N, "02-break-insert -t %s\n05",
					program_temp_break_location);
			}
			else
			{
				/* 1st loc, dette koi ---*/
				debug_send_command(N, "-gdb-set listsize 1\n"
					"02-file-list-exec-source-file\n"
					"-gdb-set listsize 10");
			}
		}
		else
			debug_send_command(N, "05");
	}
}

void on_debug_load_error(GArray *nodes)
{
	debug_load_error = TRUE;
	on_error(nodes);
}

void on_debug_exit(G_GNUC_UNUSED GArray *nodes)
{
	gdb_state = KILLING;
}

void on_debug_auto_run(G_GNUC_UNUSED GArray *nodes)
{
	if (debug_auto_run && !thread_count)
	{
		if (breaks_active())
			debug_send_command(N, "-exec-run");
		else
			dialogs_show_msgbox(GTK_MESSAGE_INFO, _("No breakpoints. Hanging."));
	}
}

void on_debug_auto_exit(void)
{
	if (debug_auto_exit)
	{
		debug_send_command(N, "-gdb-exit");
		gdb_state = KILLING;
	}
}

#define G_IO_FAILURE (G_IO_ERR | G_IO_HUP | G_IO_NVAL)  /* always used together */

static GIOChannel *send_channel = NULL;
static guint send_source_id = 0;
static guint wait_result;

static gboolean send_commands_cb(GIOChannel *channel, GIOCondition condition,
	G_GNUC_UNUSED gpointer gdata)
{
	SpawnWriteData data = { commands->str, commands->len };
	gboolean result = spawn_write_data(channel, condition, &data);
	gssize count = commands->len - data.size;

	if (count > 0)
	{
		const char *s = commands->str;

		dc_output(0, commands->str, count);
		wait_prompt = TRUE;

		do
		{
			s = strchr(s, '\n');
			if (s - commands->str >= count)
				break;

			wait_result++;
		} while (*++s);

		g_string_erase(commands, 0, count);
		update_state(DS_BUSY);
	}

	return result;
}

static void send_source_destroy_cb(G_GNUC_UNUSED gpointer gdata)
{
	send_source_id = 0;
}

/*
 * We need to release the initial stdin cb to avoid it being called constantly, and attach a
 * source when we have data to send. Unfortunately, glib does not allow re-attaching removed
 * sources, so we create one each time.
 */

static void create_send_source(void)
{
	GSource *send_source = g_io_create_watch(send_channel, G_IO_OUT | G_IO_FAILURE);

	g_io_channel_unref(send_channel);
	g_source_set_callback(send_source, (GSourceFunc) send_commands_cb, NULL,
		send_source_destroy_cb);
	send_source_id = g_source_attach(send_source, NULL);
}

#define HAS_SPAWN_LEAVE_STDIN_OPEN 0

static gboolean obtain_send_channel_cb(GIOChannel *channel, GIOCondition condition,
	G_GNUC_UNUSED gpointer gdata)
{
#if HAS_SPAWN_LEAVE_STDIN_OPEN
	if (condition & G_IO_FAILURE)
		g_io_channel_shutdown(channel, FALSE, NULL);
	else
	{
		g_io_channel_ref(channel);
		send_channel = channel;
		create_send_source();  /* for the initialization commands */
	}
#else
	if (!(condition & G_IO_FAILURE))
	{
		gint stdin_fd = dup(g_io_channel_unix_get_fd(channel));

	#ifdef G_OS_UNIX
		send_channel = g_io_channel_unix_new(stdin_fd);
		g_io_channel_set_flags(send_channel, G_IO_FLAG_NONBLOCK, NULL);
	#else
		send_channel = g_io_channel_win32_new_fd(stdin_fd);
	#endif
		g_io_channel_set_encoding(send_channel, NULL, NULL);
		g_io_channel_set_buffered(send_channel, FALSE);
		create_send_source();  /* for the initialization commands */
	}
#endif

	return FALSE;
}

static void debug_parse(char *string, const char *error)
{
	if (*string && strchr("~@&", *string))
	{
		char *text = string + 1;
		const char *end;

		if (*text == '"')
		{
			end = parse_string(text, '\n');
			dc_output(1, text, -1);
		}
		else
		{
			dc_output(1, string, -1);
			end = NULL;
		}

		if (error)
			dc_error("%s, ignoring to EOLN", error);
		else if (!end)
			dc_error("\" expected");
		else if (g_str_has_prefix(string, "~^(Scope)#07"))
			on_inspect_signal(string + 12);
	}
 	else if (!strcmp(string, "(gdb) "))  /* gdb.info says "(gdb)" */
 	{
		dc_output(3, "(gdb) ", 6);
		wait_prompt = wait_result;
	}
	else
	{
		char *message;

		for (message = string; isdigit(*message); message++);

		if (error || option_library_messages || !g_str_has_prefix(message, "=library-"))
			dc_output_nl(1, string, -1);

		if (*message == '^')
		{
			iff (wait_result, "extra result")
				wait_result--;
		}

		if (*string == '0' && message > string + 1)
		{
			memmove(string, string + 1, message - string - 1);
			message[-1] = '\0';
		}
		else
			string = NULL;  /* no token */

		if (error)
			dc_error("%s, ignoring to EOLN", error);
		else
			parse_message(message, string);
	}
}

static gboolean leading_receive;  /* FALSE for continuation of a too long / incomplete line */

static void receive_output_cb(GString *string, GIOCondition condition,
	G_GNUC_UNUSED gpointer gdata)
{
	if (condition & (G_IO_IN | G_IO_PRI))
	{
		char *term = string->str + string->len - 1;
		const char *error = NULL;

		switch (*term)
		{
			case '\n' : if (string->len >= 2 && term[-1] == '\r') term--;  /* falldown */
			case '\r' : *term = '\0'; break;
			case '\0' : error = "binary zero encountered"; break;
			default : error = "line too long or incomplete";
		}

		if (leading_receive)
			debug_parse(string->str, error);

		leading_receive = !error;
	}

	if (!commands->len)
		views_update(debug_state());

	update_state(debug_state());
}

static void receive_errors_cb(GString *string, GIOCondition condition,
	G_GNUC_UNUSED gpointer gdata)
{
	if (condition & (G_IO_IN | G_IO_PRI))
		dc_output(2, string->str, -1);
}

static void gdb_finalize(void)
{
	signal(SIGINT, SIG_DFL);

	if (send_channel)
	{
		g_io_channel_shutdown(send_channel, FALSE, NULL);
		g_io_channel_unref(send_channel);
		send_channel = NULL;

		if (send_source_id)
			g_source_remove(send_source_id);
	}
}

static void gdb_exit_cb(G_GNUC_UNUSED GPid pid, gint status, G_GNUC_UNUSED gpointer gdata)
{
	GdbState saved_state = gdb_state;

	gdb_finalize();
	gdb_state = INACTIVE;

	if (saved_state == ACTIVE)
		show_error(_("GDB died unexpectedly with status %d."), status);
	else if (thread_count)
		ui_set_statusbar(FALSE, _("Program terminated."));

	views_clear();
	utils_lock_all(FALSE);
	update_state(DS_INACTIVE);
}

static void append_startup(const char *command, const gchar *value)
{
	if (value && *value)
	{
		char *locale = utils_get_locale_from_utf8(value);
		g_string_append_printf(commands, "%s %s\n", command, locale);
		g_free(locale);
	}
}

#if HAS_SPAWN_LEAVE_STDIN_OPEN
#define GDB_SPAWN_FLAGS (SPAWN_STDERR_UNBUFFERED | SPAWN_STDOUT_RECURSIVE | \
	SPAWN_STDERR_RECURSIVE | SPAWN_LEAVE_STDIN_OPEN)
#else
#define GDB_SPAWN_FLAGS (SPAWN_STDERR_UNBUFFERED | SPAWN_STDOUT_RECURSIVE | \
	SPAWN_STDERR_RECURSIVE)
#endif

#define GDB_BUFFER_SIZE ((1 << 20) - 1)  /* spawn adds 1 for '\0' */

static void load_program(void)
{
	char *args[] = { utils_get_locale_from_utf8(pref_gdb_executable), (char *) "--quiet",
		(char *) "--interpreter=mi2", NULL };
	GError *gerror = NULL;

	statusbar_update_state(DS_EXTRA_2);
	plugin_blink();
	while (gtk_events_pending())
		gtk_main_iteration();

	if (spawn_with_callbacks(NULL, NULL, args, NULL, GDB_SPAWN_FLAGS, obtain_send_channel_cb,
		NULL, receive_output_cb, NULL, GDB_BUFFER_SIZE, receive_errors_cb, NULL, 0,
		gdb_exit_cb, NULL, &gdb_pid, &gerror))
	{
		gchar **environment = g_strsplit(program_environment, "\n", -1);
		gchar *const *envar;
	#ifdef G_OS_UNIX
		extern char *slave_pty_name;
	#else
		GString *escaped = g_string_new(program_executable);
	#endif

		/* startup */
		gdb_state = ACTIVE;
		dc_clear();
		utils_lock_all(TRUE);
		signal(SIGINT, SIG_IGN);
		wait_result = 0;
		wait_prompt = TRUE;
		g_string_truncate(commands, 0);
		leading_receive = TRUE;

		if (pref_gdb_async_mode)
			g_string_append(commands, "-gdb-set target-async on\n");
		if (program_non_stop_mode)
			g_string_append(commands, "-gdb-set non-stop on\n");
	#ifdef G_OS_UNIX
		append_startup("010-file-exec-and-symbols", program_executable);
		append_startup("-gdb-set inferior-tty", slave_pty_name);
	#else  /* G_OS_UNIX */
		utils_string_replace_all(escaped, "\\", "\\\\");
		append_startup("010-file-exec-and-symbols", escaped->str);
		g_string_free(escaped, TRUE);
		g_string_append(commands, "-gdb-set new-console on\n");
	#endif  /* G_OS_UNIX */
		append_startup("-environment-cd", program_working_dir);  /* no escape needed */
		append_startup("-exec-arguments", program_arguments);
		for (envar = environment; *envar; envar++)
			append_startup("-gdb-set environment", *envar);
		g_strfreev(environment);
		append_startup("011source -v", program_load_script);
		g_string_append(commands, "07-list-target-features\n");
		breaks_query_async(commands);

		if (*program_executable || *program_load_script)
		{
			debug_load_error = FALSE;
			debug_auto_run = debug_auto_exit = program_auto_run_exit;
		}
		else
			debug_auto_run = debug_auto_exit = FALSE;

		if (option_open_panel_on_load)
			open_debug_panel();

		registers_query_names();
	}
	else
	{
		show_error(_("%s."), gerror->message);
		g_error_free(gerror);
	}

	g_free(args[0]);

	if (gdb_state == INACTIVE)
		statusbar_update_state(DS_INACTIVE);
}

static gboolean check_load_path(const gchar *pathname, gboolean file, int mode)
{
	if (!utils_check_path(pathname, file, mode))
	{
		show_errno(pathname);
		return FALSE;
	}

	return TRUE;
}

void on_debug_run_continue(G_GNUC_UNUSED const MenuItem *menu_item)
{
	if (gdb_state == INACTIVE)
	{
		if (check_load_path(program_executable, TRUE, R_OK | X_OK) &&
			check_load_path(program_working_dir, FALSE, X_OK) &&
			check_load_path(program_load_script, TRUE, R_OK))
		{
			load_program();
		}
	}
	else if (thread_count)
		debug_send_thread("-exec-continue");
	else
	{
		breaks_apply();
		inspects_apply();
		debug_send_command(N, "-exec-run");
	}
}

void on_debug_goto_cursor(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GeanyDocument *doc = document_get_current();

	debug_send_format(T, "%s %s:%d", pref_scope_goto_cursor ?
		"020-break-insert -t" : "-exec-until", doc->real_path, utils_current_line(doc));
}

void on_debug_goto_source(G_GNUC_UNUSED const MenuItem *menu_item)
{
	debug_send_thread("-exec-step");
}

void on_debug_step_into(G_GNUC_UNUSED const MenuItem *menu_item)
{
	debug_send_thread(thread_state == THREAD_AT_SOURCE ? "-exec-step"
		: "-exec-step-instruction");
}

void on_debug_step_over(G_GNUC_UNUSED const MenuItem *menu_item)
{
	debug_send_thread(thread_state == THREAD_AT_SOURCE ? "-exec-next"
		: "-exec-next-instruction");
}

void on_debug_step_out(G_GNUC_UNUSED const MenuItem *menu_item)
{
	debug_send_thread("-exec-finish");
}

void on_debug_terminate(const MenuItem *menu_item)
{
	switch (debug_state())
	{
		case DS_BUSY :
		{
			GError *gerror = NULL;

			gdb_state = KILLING;

			if (!spawn_kill_process(gdb_pid, &gerror))
			{
				show_error(_("%s."), gerror->message);
				g_error_free(gerror);
			}

			break;
		}
		case DS_READY :
		case DS_DEBUG :
		{
			if (menu_item && !debug_auto_exit)
			{
				debug_send_command(N, "kill");
				break;
			}
			/* falldown */
		}
		default :
		{
			debug_send_command(N, "-gdb-exit");
			gdb_state = KILLING;
			break;
		}
	}
}

void debug_send_command(gint tf, const char *command)
{
	if (gdb_state == ACTIVE)
	{
		const char *s;

		for (s = command; *s && !isspace(*s); s++);
		g_string_append_len(commands, command, s - command);

		if (tf && thread_id)
		{
			g_string_append_printf(commands, " --thread %s", thread_id);

			if (tf == F && frame_id && thread_state >= THREAD_STOPPED)
				g_string_append_printf(commands, " --frame %s", frame_id);
		}

		g_string_append(commands, s);
		g_string_append_c(commands, '\n');

		if (send_channel && !send_source_id)
			create_send_source();
	}
}

void debug_send_format(gint tf, const char *format, ...)
{
	va_list ap;
	char *command;

	va_start(ap, format);
	command = g_strdup_vprintf(format, ap);
	va_end(ap);
	debug_send_command(tf, command);
	g_free(command);
}

char *debug_send_evaluate(char token, gint scid, const gchar *expr)
{
	char *locale = utils_get_locale_from_utf8(expr);
	GString *escaped = g_string_sized_new(strlen(locale));
	const char *s;

	for (s = locale; *s; s++)
	{
		if (*s == '"' || *s == '\\')
			g_string_append_c(escaped, '\\');
		g_string_append_c(escaped, *s);
	}

	debug_send_format(F, "0%c%d-data-evaluate-expression \"%s\"", token, scid, escaped->str);
	g_string_free(escaped, TRUE);
	return locale;
}

void debug_init(void)
{
	commands = g_string_sized_new(0x3FFF);
}

void debug_finalize(void)
{
	if (gdb_state != INACTIVE)
	{
		spawn_kill_process(gdb_pid, NULL);
		gdb_finalize();
		statusbar_update_state(DS_INACTIVE);
	}

	g_string_free(commands, TRUE);
}
