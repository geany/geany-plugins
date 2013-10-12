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

#ifdef G_OS_UNIX
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#else  /* G_OS_UNIX */
#include <windows.h>

#define WNOHANG 0

static int waitpid(HANDLE pid, int *stat_loc, int options)
{
	if (options == WNOHANG)
	{
		DWORD status;

		if (GetExitCodeProcess(pid, &status))
		{
			if (status == STILL_ACTIVE)
				return 0;

			if (stat_loc)
				*stat_loc = status;
			return 1;
		}
	}

	errno = EINVAL;
	return -1;
}

#define SIGKILL 9

static int kill(HANDLE pid, int sig)
{
	if (TerminateProcess(pid, sig))
		return 0;

	errno = EINVAL;
	return -1;
}
#endif  /* G_OS_UNIX */

#include "common.h"

extern guint thread_count;
extern guint thread_prompt;

typedef enum _GdbState
{
	INACTIVE,
	ACTIVE,
	KILLING
} GdbState;

static GdbState gdb_state = INACTIVE;
static GSource *gdb_source;
static GPid gdb_pid;

static GPollFD gdb_in = { -1, G_IO_OUT | G_IO_ERR, 0 };
static GPollFD gdb_out = { -1, G_IO_IN | G_IO_HUP | G_IO_ERR, 0 };
static GPollFD gdb_err = { -1, G_IO_IN | G_IO_HUP | G_IO_ERR, 0 };

static void free_gdb(void)
{
	g_spawn_close_pid(gdb_pid);
	close(gdb_in.fd);
	close(gdb_out.fd);
	close(gdb_err.fd);
}

static gboolean io_error_show(gpointer gdata)
{
	show_error("%s", (gchar *) gdata);
	g_free(gdata);
	return FALSE;
}

#ifdef G_OS_UNIX
static void gdb_io_check(ssize_t count, const char *operation, G_GNUC_UNUSED int eagain)
{
	if (count == -1 && errno != EAGAIN && gdb_state != KILLING)
#else  /* G_OS_UNIX */
static void gdb_io_check(ssize_t count, const char *operation, int eagain)
{
	if (count == -1 && errno != EAGAIN && errno != eagain && gdb_state != KILLING)
#endif  /* G_OS_UNIX */
	{
		plugin_idle_add(geany_plugin, io_error_show,
			g_strdup_printf(_("%s: %s."), operation, g_strerror(errno)));

		if (kill(gdb_pid, SIGKILL) == -1)
		{
			plugin_idle_add(geany_plugin, io_error_show,
				g_strdup_printf(_("%s: %s."), "kill(gdb)", g_strerror(errno)));
		}
		gdb_state = KILLING;
	}
}

static guint wait_result;
static gboolean wait_prompt;
static GString *commands;

static void send_commands(void)
{
	ssize_t count = write(gdb_in.fd, commands->str, commands->len);

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
	else
		gdb_io_check(count, "write(gdb_in)", ENOSPC);
}

#ifndef G_OS_UNIX
static DWORD last_send_ticks;
#endif

static void debug_send_commands(void)
{
	send_commands();
	if (commands->len)
	{
	#ifdef G_OS_UNIX
		g_source_add_poll(gdb_source, &gdb_in);
	#else
		last_send_ticks = GetTickCount();
	#endif
	}
}

static void pre_parse(char *string, gboolean overflow)
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

		if (overflow)
			dc_error("overflow");
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

		if (option_library_messages || !g_str_has_prefix(message, "=library-"))
		{
			dc_output_nl(1, string, -1);

			if (overflow)
				dc_error("overflow");
		}

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

		parse_message(message, string);
	}
}

static guint MAXLEN;
static GString *received;
static gboolean leading_receive;
static char *reading_pos;

static gboolean source_prepare(G_GNUC_UNUSED GSource *source, gint *timeout)
{
	*timeout = -1;
	return gdb_state != INACTIVE && reading_pos > received->str;
}

#ifdef G_OS_UNIX
static gboolean source_check(G_GNUC_UNUSED GSource *source)
{
	return gdb_state != INACTIVE && (gdb_err.revents || reading_pos > received->str ||
		gdb_out.revents || (commands->len && gdb_in.revents));
}
#else  /* G_OS_UNIX */
static gboolean peek_pipe(GPollFD *fd)
{
	DWORD available;
	return !PeekNamedPipe((HANDLE) _get_osfhandle(fd->fd), NULL, 0, NULL, &available,
		NULL) || available;
}

static gboolean source_check(G_GNUC_UNUSED GSource *source)
{
	return gdb_state != INACTIVE && (reading_pos > received->str || peek_pipe(&gdb_err) ||
		peek_pipe(&gdb_out) || (commands->len &&
		GetTickCount() - last_send_ticks >= (guint) pref_gdb_send_interval * 10));
}
#endif  /* G_OS_UNIX */

static guint source_id = 0;

static gboolean source_dispatch(G_GNUC_UNUSED GSource *source,
	G_GNUC_UNUSED GSourceFunc callback, G_GNUC_UNUSED gpointer gdata)
{
	int status;
	pid_t result;
	ssize_t count;
	char buffer[0x200];
	char *pos;

	/* show errors */
	while ((count = read(gdb_err.fd, buffer, sizeof buffer - 1)) > 0)
		dc_output(2, buffer, count);

	gdb_io_check(count, "read(gdb_err)", EINVAL);

	/* receive */
	count = read(gdb_out.fd, received->str + received->len, MAXLEN - received->len);

	if (count > 0)
		g_string_set_size(received, received->len + count);
	else
		gdb_io_check(count, "read(gdb_out)", EINVAL);

	while (pos = reading_pos, (reading_pos = strchr(pos, '\n')) != NULL)
	{
		if (leading_receive)
		{
		#ifdef G_OS_UNIX
			*reading_pos++ = '\0';
		#else
			gboolean cr = reading_pos > received->str && reading_pos[-1] == '\r';
			reading_pos[-cr]= '\0';
			reading_pos++;
		#endif
			pre_parse(pos, FALSE);
		}
		else
		{
			reading_pos++;
			leading_receive = TRUE;
		}
	}

	g_string_erase(received, 0, pos - received->str);

	if (G_UNLIKELY(received->len == MAXLEN))
	{
		if (leading_receive)
		{
			reading_pos = received->str + MAXLEN;
			pre_parse(received->str, TRUE);
		}
		g_string_truncate(received, 0);
		leading_receive = FALSE;
	}

	reading_pos = received->str;
	result = waitpid(gdb_pid, &status, WNOHANG);

	if (result == 0)
	{
		if (commands->len)
		{
			/* send */
		#ifdef G_OS_UNIX
			send_commands();
			if (!commands->len)
				g_source_remove_poll(gdb_source, &gdb_in);
		#else
			debug_send_commands();
		#endif
		}
		else
		{
			/* idle update */
			DebugState state = debug_state();

			if (state & DS_SENDABLE)
				views_update(state);
		}
	}
	else if (gdb_state != INACTIVE)
	{
		GdbState state = gdb_state;
		/* shutdown */
		gdb_state = INACTIVE;
		signal(SIGINT, SIG_DFL);
		g_source_remove(source_id);

		if (result == -1)
			show_errno("waitpid(gdb)");
		else if (state == ACTIVE)
			show_error(_("GDB died unexpectedly with status %d."), status);
		else if (thread_count)
			ui_set_statusbar(FALSE, _("Program terminated."));

		free_gdb();
		views_clear();
		utils_lock_all(FALSE);
	}

	update_state(debug_state());

	return TRUE;
}

static void source_finalize(G_GNUC_UNUSED GSource *source)
{
	source_id = 0;
}

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

static gboolean debug_auto_run;
static gboolean debug_auto_exit;

void on_debug_list_source(GArray *nodes)
{
	ParseLocation loc;

	parse_location(nodes, &loc);

	iff (loc.line, "no line or abs file")
		debug_send_format(N, "02-break-insert -t %s:%d\n05", loc.file, loc.line);

	parse_location_free(&loc);
}

void on_debug_error(GArray *nodes)
{
	debug_auto_run = FALSE;
	on_error(nodes);
}

static gboolean debug_load_error;

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

static void gdb_exit(void)
{
	debug_send_command(N, "-gdb-exit");
	gdb_state = KILLING;
}

void on_debug_auto_exit(void)
{
	if (debug_auto_exit)
		gdb_exit();
}

static GSourceFuncs gdb_source_funcs =
{
	source_prepare,
	source_check,
	source_dispatch,
	source_finalize,
	NULL,
	NULL
};

static void append_startup(const char *command, const gchar *value)
{
	if (value && *value)
	{
		char *locale = utils_get_locale_from_utf8(value);
		g_string_append_printf(commands, "%s %s\n", command, locale);
		g_free(locale);
	}
}

#define GDB_SPAWN_FLAGS (G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD)

static void load_program(void)
{
	char *args[] = { utils_get_locale_from_utf8(pref_gdb_executable), (char *) "--quiet",
		(char *) "--interpreter=mi2", NULL };
	GError *gerror = NULL;

	statusbar_update_state(DS_EXTRA_2);
	plugin_blink();
	while (gtk_events_pending())
		gtk_main_iteration();

	if (g_spawn_async_with_pipes(NULL, args, NULL, GDB_SPAWN_FLAGS, NULL, NULL, &gdb_pid,
		&gdb_in.fd, &gdb_out.fd, &gdb_err.fd, &gerror))
	{
		gdb_state = ACTIVE;

		if (utils_set_nonblock(&gdb_in) && utils_set_nonblock(&gdb_out) &&
			utils_set_nonblock(&gdb_err))
		{
			gchar **environment = g_strsplit(program_environment, "\n", -1);
			gchar *const *envar;
		#ifdef G_OS_UNIX
			extern char *slave_pty_name;
		#else
			GString *escaped = g_string_new(program_executable);
		#endif

			/* startup */
			dc_clear();
			utils_lock_all(TRUE);
			signal(SIGINT, SIG_IGN);
			wait_result = 0;
			wait_prompt = TRUE;
			g_string_truncate(commands, 0);
			g_string_truncate(received, 0);
			reading_pos = received->str;
			leading_receive = TRUE;

			gdb_source = g_source_new(&gdb_source_funcs, sizeof(GSource));
			g_source_set_can_recurse(gdb_source, TRUE);
			source_id = g_source_attach(gdb_source, NULL);
			g_source_unref(gdb_source);
		#ifdef G_OS_UNIX
			g_source_add_poll(gdb_source, &gdb_out);
			g_source_add_poll(gdb_source, &gdb_err);
		#endif
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
		#endif  /* G_OS_UNIX */
			append_startup("-environment-cd", program_working_dir);
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
			debug_send_commands();
		}
		else
		{
			show_errno("fcntl(O_NONBLOCK)");

			if (kill(gdb_pid, SIGKILL) == -1)
				show_errno("kill(gdb)");
		}
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
		debug_send_command(N, "-exec-run");
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
		case DS_DEBUG :
		case DS_READY :
		{
			if (menu_item && !debug_auto_exit)
			{
				debug_send_command(N, "kill");
				break;
			}
		}
		case DS_HANGING :
		{
			gdb_exit();
			break;
		}
		default :
		{
			gdb_state = KILLING;
			if (kill(gdb_pid, SIGKILL) == -1)
				show_errno("kill(gdb)");
		}
	}
}

void debug_send_command(gint tf, const char *command)
{
	if (gdb_state == ACTIVE)
	{
		gsize previous_len = commands->len;
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

		if (!previous_len)
			debug_send_commands();
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
	received = g_string_sized_new(pref_gdb_buffer_length);
	MAXLEN = received->allocated_len - 1;
}

void debug_finalize(void)
{
	if (source_id)
	{
		signal(SIGINT, SIG_DFL);
		g_source_remove(source_id);
	}

	if (gdb_state != INACTIVE)
	{
		gint count = 0;

		if (kill(gdb_pid, SIGKILL) == 0)
		{
			g_usleep(G_USEC_PER_SEC / 1000);
			while (waitpid(gdb_pid, NULL, WNOHANG) == 0 && count++ < pref_gdb_wait_death)
				g_usleep(G_USEC_PER_SEC / 100);
		}

		free_gdb();  /* may be still alive, but we can't do anything more */
		statusbar_update_state(DS_INACTIVE);
	}

	g_string_free(received, TRUE);
	g_string_free(commands, TRUE);
}
