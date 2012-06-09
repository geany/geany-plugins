/*
 * gdb-io-run.c - Process execution and input functions for GDB wrapper library.
 * Copyright 2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. *
 */

#include <unistd.h>
#include <signal.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

extern GeanyFunctions *geany_functions;
#include "gdb-io-priv.h"


extern gint g_unlink(const gchar * filename);


GdbIoSetup gdbio_setup;


static const gchar *gdbio_args[] = { "gdb", "--interpreter=mi", "-nx", NULL };

static GPid gdbio_pid = 0;
static GPid target_pid = 0;
static GPid xterm_pid = 0;

static GSource *gdbio_src;
static gint gdbio_in;
static gint gdbio_out;
static GIOChannel *gdbio_ch_in;
static GIOChannel *gdbio_ch_out;
static guint gdbio_id_in;
static guint gdbio_id_out;

static GString send_buf = { NULL, 0, 0 };
static GString recv_buf = { NULL, 0, 0 };

static gchar *xterm_tty_file = NULL;


static gint sequence = SEQ_MIN;
static gboolean is_running = FALSE;
static gint process_token = 0;


static void send_to_gdb(const gchar *data);


/*
  Hash table to associate a "tokenized" GDB command with a function call.
  This stores a list of key-value pairs where the unique sequence-number
  (GDB token) is the key, and a ResponseHandler function pointer is the value.
*/
static GHashTable *sequencer;



#if !GLIB_CHECK_VERSION(2, 14, 0)
static void
g_string_append_vprintf(GString *str, const gchar *fmt, va_list args)
{
	gchar *tmp = g_strdup_vprintf(fmt, args);
	g_string_append(str, tmp);
	g_free(tmp);
}
#endif


/* Add a handler function to the sequencer */
gint
gdbio_send_seq_cmd(ResponseHandler func, const gchar * fmt, ...)
{
	va_list args;
	GString *data;
	if (!gdbio_pid)
	{
		return 0;
	}
	if (sequence >= SEQ_MAX)
	{
		sequence = SEQ_MIN;
	}
	else
	{
		sequence++;
	}
	if (!sequencer)
	{
		sequencer = g_hash_table_new(g_direct_hash, g_direct_equal);
	}
	g_hash_table_insert(sequencer, GINT_TO_POINTER(sequence), func);
	data = g_string_sized_new(128);
	g_string_append_printf(data, "%d", sequence);
	va_start(args, fmt);
	g_string_append_vprintf(data, fmt, args);
	va_end(args);
	send_to_gdb(data->str);
	g_string_free(data, TRUE);
	return sequence;
}


ResponseHandler
gdbio_seq_lookup(gint seq)
{
	return g_hash_table_lookup(sequencer, GINT_TO_POINTER(seq));
}


void
gdbio_pop_seq(gint seq)
{
	g_hash_table_remove(sequencer, GINT_TO_POINTER(seq));
}

static gboolean
gerror(const gchar * msg, GError ** err)
{
	if (*err)
	{
		if (msg)
		{
			gdbio_error_func("%s\n%s\n", msg, (*err)->message);
		}
		else
		{
			gdbio_error_func("%s\n", (*err)->message);
		}
		g_error_free(*err);
		*err = NULL;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


gint
gdbio_atoi(const gchar * str)
{
	gchar *tail = NULL;
	gint rv = strtol(str, &tail, 10);
	return (tail && !*tail) ? rv : -1;
}


void
gdbio_error_func(const gchar * fmt, ...)
{
	va_list args;
	gchar *msg;
	va_start(args, fmt);
	msg = g_strdup_vprintf(fmt, args);
	if (gdbio_setup.error_func)
	{
		gdbio_setup.error_func(g_strstrip(msg));
	}
	else
	{
		g_printerr("%s", msg);
	}
	g_free(msg);
	va_end(args);
}


void
gdbio_info_func(const gchar * fmt, ...)
{
	va_list args;
	gchar *msg;
	va_start(args, fmt);
	msg = g_strdup_vprintf(fmt, args);
	if (gdbio_setup.info_func)
	{
		gdbio_setup.info_func(g_strstrip(msg));
	}
	else
	{
		g_printerr("%s", msg);
	}
	g_free(msg);
	va_end(args);
}


gint
gdbio_wait(gint ms)
{
	struct timespec req = { 0, 0 }, rem =
	{
	0, 0};
	gint rms = ms;
	if (ms >= 1000)
	{
		req.tv_sec = ms / 1000;
		rms = ms % 1000;
	}
	req.tv_nsec = rms * 1000000;	/* 1 millisecond = 1,000,000 nanoseconds */
	do
	{
		nanosleep(&req, &rem);
		if ((rem.tv_sec || rem.tv_nsec))
		{
			memcpy(&req, &rem, sizeof(req));
			memset(&rem, 0, sizeof(rem));
		}
		else
		{
			break;
		}

	}
	while (1);
	return ms;
}



void
gdbio_send_cmd(const gchar * fmt, ...)
{
	va_list args;
	gchar *data;
	if (!gdbio_pid)
	{
		return;
	}
	va_start(args, fmt);
	data = g_strdup_vprintf(fmt, args);
	va_end(args);

	send_to_gdb(data);
	g_free(data);
}



void
gdbio_set_running(gboolean running)
{
	is_running = running;
}



static void
kill_xterm(void)
{
	if (xterm_pid)
	{
		kill(xterm_pid, SIGKILL);
		xterm_pid = 0;
	}
}



static gchar *
start_xterm(const gchar * term_cmd)
{
	const gchar *term_args[] = { "xterm", "-title", "Debug terminal", "-e", NULL, NULL, NULL };
	GError *err = NULL;
	gint i = 0;
	gchar *tty_name = NULL;
	const gchar *exe_name = basename(term_cmd);
	gchar *all;
	if (!gdbio_setup.temp_dir)
	{
		gdbio_error_func(_("tty temporary directory not specified!\n"));
		return NULL;
	}
	if (!g_file_test(gdbio_setup.temp_dir, G_FILE_TEST_IS_DIR))
	{
		gdbio_error_func(_("tty temporary directory not found!\n"));
		return NULL;
	}
	if (!xterm_tty_file)
	{
		xterm_tty_file = g_strdup_printf("%s/%d.tty", gdbio_setup.temp_dir, getpid());
	}
	if (g_file_set_contents(xterm_tty_file, "", -1, &err))
	{
		g_unlink(xterm_tty_file);
	}
	else
	{
		gerror("writing ttyname logfile", &err);
		g_unlink(xterm_tty_file);
		return FALSE;
	}
	if (!gdbio_setup.tty_helper)
	{
		gdbio_error_func(_("tty helper program not specified!\n"));
		return NULL;
	}
	if (!
	    (g_file_test(gdbio_setup.tty_helper, G_FILE_TEST_IS_EXECUTABLE) &&
	     g_file_test(gdbio_setup.tty_helper, G_FILE_TEST_IS_REGULAR)))
	{
		gdbio_error_func(_("tty helper program not found!\n"));
		return NULL;
	}
	term_args[0] = term_cmd;
	if (g_str_equal(exe_name, "xterm") || g_str_equal(exe_name, "konsole"))
	{
		term_args[1] = "-T";
	}
	else
	{
		if (g_str_equal(exe_name, "gnome-terminal"))
		{
			term_args[1] = "--title";
			term_args[3] = "-x";
		}
		else
		{
			if (g_str_equal(exe_name, "rxvt") || g_str_equal(exe_name, "urxvt"))
			{
				term_args[1] = "-title";
			}
			else
			{
				term_args[1] = "-e";
				term_args[2] = NULL;
			}
		}
	}
	i = 0;
	while (term_args[i])
	{
		i++;
	}
	term_args[i] = gdbio_setup.tty_helper;
	term_args[i + 1] = xterm_tty_file;
	all = g_strjoinv("\"  \"", (gchar **) term_args);
	gdbio_info_func("\"%s\"\n", all);
	g_free(all);
	if (g_spawn_async(NULL, (gchar **) term_args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &xterm_pid, &err))
	{
		gchar *contents = NULL;
		gsize len;
		gint ms = 0;
		do
		{
			if (g_file_test(xterm_tty_file, G_FILE_TEST_EXISTS))
			{
				if (g_file_get_contents(xterm_tty_file, &contents, &len, &err))
				{
					g_strstrip(contents);
					if (strlen(contents))
					{
						tty_name = g_strdup(contents);
						gdbio_info_func(_("Attaching to terminal %s\n"),
								tty_name);
					}
					break;
				}
				else
				{
					gerror("Error getting tty name:", &err);
				}
			}
			ms += gdbio_wait(250);
		}
		while (ms <= 10000);
		if (ms > 10000)
		{
			gdbio_error_func(_("Timeout waiting for TTY name.\n"));
			kill_xterm();
		}
	}
	else
	{
		gerror("Error starting terminal: ", &err);
	}
	g_unlink(xterm_tty_file);
	return tty_name;
}



static void
free_buf(GString * buf)
{
	if (buf->str)
	{
		g_free(buf->str);
		buf->str = NULL;
		buf->len = 0;
		buf->allocated_len = 0;
	}
}

static void
shutdown_channel(GIOChannel ** ch)
{
	if (*ch)
	{
		GError *err = NULL;
		gint fd = g_io_channel_unix_get_fd(*ch);
		g_io_channel_shutdown(*ch, TRUE, &err);
		gerror("Shutting down channel", &err);
		g_io_channel_unref(*ch);
		*ch = NULL;
		if (fd >= 0)
		{
			close(fd);
		}
	}
}


static void
on_gdb_exit(GPid pid, gint status, gpointer data)
{
	gdbio_pid = 0;
	gdbio_info_func(_("GDB exited (pid=%d)\n"), pid);
	g_spawn_close_pid(pid);


	shutdown_channel(&gdbio_ch_in);

	g_source_remove(gdbio_id_out);
	shutdown_channel(&gdbio_ch_out);

	free_buf(&send_buf);
	if (recv_buf.len)
	{
		gdbio_info_func("%s\n", recv_buf.str);
	}
	free_buf(&recv_buf);

	if (target_pid)
	{
		kill(target_pid, SIGKILL);
		target_pid = 0;
	}
	gdbio_set_running(FALSE);
	gdblx_scanner_done();
	gdbio_do_status(GdbDead);
}



static gboolean
on_send_to_gdb(gpointer data)
{
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	GIOChannel *src = gdbio_ch_in;

	if (send_buf.len)
	{
		while (send_buf.len)
		{
			st = g_io_channel_write_chars(src, send_buf.str, send_buf.len, &count,
						      &err);
			g_string_erase(&send_buf, 0, count);
			if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
			{
				gerror("Error sending command", &err);
				break;
			}
		}
		st = g_io_channel_flush(src, &err);
		gerror("Error pushing command", &err);
		gdbio_wait(10);
	}
	return (send_buf.len > 0);
}



static void
delay_send_to_gdb(const gchar *data)
{
	g_string_append(&send_buf, data);
	g_idle_add(on_send_to_gdb, NULL);
}



static void
send_to_gdb(const gchar *data)
{
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	GIOChannel *src = gdbio_ch_in;
	GString *text = g_string_new(data);

	if (text->len)
	{
		while (text->len)
		{
			st = g_io_channel_write_chars(src, text->str, text->len, &count,
						      &err);
			g_string_erase(text, 0, count);
			if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
			{
				gerror("Error sending command", &err);
				/* puffer string for later retry */
				delay_send_to_gdb(text->str);
				break;
			}
		}
		st = g_io_channel_flush(src, &err);
		gerror("Error pushing command", &err);
	}
	g_string_free(text, TRUE);
}



void
gdbio_target_exited(const gchar * reason)
{
	gdbio_info_func(_("Target process exited. (pid=%d; %s%s)\n"), target_pid,
			reason
			&& g_ascii_isdigit(reason[0]) ? _("code=") : _("reason:"),
			reason ? reason : "unknown");
	target_pid = 0;
	kill_xterm();
	gdbio_set_running(FALSE);
	gdbio_do_status(GdbFinished);
	if (process_token)
	{
		gdbio_pop_seq(process_token);
		process_token = 0;
	}
}

static GdbStatus gdbio_status = GdbDead;

void
gdbio_do_status(GdbStatus s)
{
	gdbio_status = s;
	if (gdbio_setup.status_func)
	{
		gdbio_setup.status_func(s);
	}
}



void
gdbio_pause_target(void)
{
	if (target_pid)
	{
		kill(target_pid, SIGINT);
	}
}



static void
target_killed(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	gdbio_pop_seq(seq);
	if (h)
	{
		g_hash_table_destroy(h);
	}
	if (strncmp(resp, "^done", 5) == 0)
	{
		gdbio_target_exited("killed by GDB");
	}
}



void
gdbio_kill_target(gboolean force)
{
	if (target_pid)
	{
		gchar pidstr[64];
		GPid this_pid = target_pid;
		gint ms = 0;
		snprintf(pidstr, sizeof(pidstr) - 1, "/proc/%d", target_pid);
		if (!g_file_test(pidstr, G_FILE_TEST_IS_DIR))
		{
			gdbio_info_func(_("Directory %s not found!\n"), pidstr);
			pidstr[0] = '\0';
		}
		if (!force)
		{
			gdbio_info_func(_("Shutting down target program.\n"));
			gdbio_send_seq_cmd(target_killed, "kill\n");
			gdbio_wait(250);
			do_loop();
		}
		else
		{
			gdbio_info_func(_("Killing target program.\n"));
			kill(this_pid, SIGKILL);
		}
		while (1)
		{
			do_loop();
			if (ms >= 2000)
			{
				gdbio_info_func(_("Timeout waiting for target process.\n"));
				if (!force)
				{
					gdbio_info_func(_("Using a bigger hammer!\n"));
					gdbio_kill_target(TRUE);
				}
				break;
			}
			if (target_pid != this_pid)
			{
				break;
			}
			if ((pidstr[0]) && !g_file_test(pidstr, G_FILE_TEST_EXISTS))
			{
				break;
			}
			if (!(ms % 1000))
				gdbio_info_func(_("Waiting for target process to exit.\n"));
			ms += gdbio_wait(250);
		}
	}
	kill_xterm();
}

static gboolean
have_console(void)
{
	return (gdbio_status == GdbLoaded) || (gdbio_status == GdbStopped)
		|| (gdbio_status == GdbFinished);
}

void
gdbio_exit(void)
{
	gdbio_kill_target(!have_console());
	if (gdbio_pid)
	{
		GPid this_gdb = gdbio_pid;
		gint ms = 0;
		gchar pidstr[64];
		snprintf(pidstr, sizeof(pidstr) - 1, "/proc/%d", this_gdb);
		if (is_running)
		{
			if (!g_file_test(pidstr, G_FILE_TEST_IS_DIR))
			{
				gdbio_info_func(_("Directory %s not found!\n"), pidstr);
				pidstr[0] = '\0';
			}
			do
			{
				do_loop();
				if (gdbio_pid == this_gdb)
				{
					gdbio_info_func(_("Killing GDB (pid=%d)\n"), this_gdb);
				}
				else
				{
					break;
				}
				kill(this_gdb, SIGKILL);
				ms += gdbio_wait(500);
				if (pidstr[0] && !g_file_test(pidstr, G_FILE_TEST_EXISTS))
				{
					break;
				}
				if (ms > 2000)
				{
					gdbio_error_func(_("Timeout trying to kill GDB.\n"));
					break;
				}
			}
			while (1);
			free_buf(&send_buf);
			gdbio_wait(500);
		}
		else
		{
			gdbio_info_func(_("Shutting down GDB\n"));
			gdbio_send_cmd("-gdb-exit\n");
			while (1)
			{
				do_loop();
				ms += gdbio_wait(250);
				if (pidstr[0] && !g_file_test(pidstr, G_FILE_TEST_EXISTS))
				{
					break;
				}
				if (gdbio_pid == this_gdb)
				{
					if (!(ms % 1000))
						gdbio_info_func(_("Waiting for GDB to exit.\n"));
				}
				else
				{
					break;
				}
				if (ms > 2000)
				{
					gdbio_info_func(_("Timeout waiting for GDB to exit.\n"));
					gdbio_set_running(TRUE);
					gdbio_exit();
					break;
				}
			}
		}
	}
	if (sequencer)
	{
		g_hash_table_destroy(sequencer);
		sequencer = NULL;
	}
	g_free(xterm_tty_file);
	xterm_tty_file = NULL;
}



static void
load_target(const gchar * exe_name)
{
	gdbio_set_running(FALSE);
	gdbio_send_cmd("-file-exec-and-symbols %s\n", exe_name);
	gdbio_send_seq_cmd(gdbio_parse_file_list, "-file-list-exec-source-files\n");

}



static gboolean
on_read_from_gdb(GIOChannel * src, GIOCondition cond, gpointer data)
{
	gchar buf[1024];
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	st = g_io_channel_read_chars(src, buf, sizeof(buf) - 1, &count, &err);
	buf[count] = '\0';
	g_string_append_len(&recv_buf, buf, count);
	gerror("Error reading response", &err);
	gdbio_consume_response(&recv_buf);
	gdbio_wait(10);
	return TRUE;
}


#define GDB_SPAWN_FLAGS \
G_SPAWN_SEARCH_PATH | \
G_SPAWN_DO_NOT_REAP_CHILD


void
gdbio_load(const gchar * exe_name)
{
	GError *err = NULL;
	/* need to execute gdb with LANG=C because we parse the output
	 * for break and watch points
	 * however, the debugged applications ought to run under the
	 * systems locale (i.e. the current LANG) */
	const gchar *exclude[] = { "LANG", NULL };
	gchar **gdbio_env = utils_copy_environment(exclude, "LANG", "C", NULL);
	const gchar *env_lang = g_getenv("LANG");
	gdbio_exit();
	if (g_spawn_async_with_pipes(NULL, (gchar **) gdbio_args, gdbio_env,
				     GDB_SPAWN_FLAGS, NULL,
				     NULL, &gdbio_pid, &gdbio_in, &gdbio_out, NULL, &err))
	{
		gdbio_info_func(_("Starting gdb (pid=%d)\n"), gdbio_pid);

		g_child_watch_add(gdbio_pid, on_gdb_exit, NULL);
		gdbio_src = g_child_watch_source_new(gdbio_pid);

		gdbio_ch_in = g_io_channel_unix_new(gdbio_in);
		g_io_channel_set_encoding(gdbio_ch_in, NULL, &err);
		gerror("Error setting encoding", &err);
		g_io_channel_set_buffered(gdbio_ch_in, FALSE);

		gdbio_ch_out = g_io_channel_unix_new(gdbio_out);
		g_io_channel_set_encoding(gdbio_ch_out, NULL, &err);
		gerror("Error setting encoding", &err);
		g_io_channel_set_buffered(gdbio_ch_out, FALSE);

		gdbio_id_out = g_io_add_watch(gdbio_ch_out, G_IO_IN, on_read_from_gdb, NULL);

		gdbio_send_cmd("-gdb-set width 0\n-gdb-set height 0\n");
		/* restore LANG for apps here, gdb is under LANG=C */
		gdbio_send_cmd("-gdb-set environment LANG=%s\n", env_lang);
		if (exe_name)
		{
			load_target(exe_name);
		}
	}
	else
	{
		gerror("Error starting debugger.", &err);
	}
	g_strfreev(gdbio_env);
}



void
gdbio_exec_target(const gchar * terminal_command)
{
	if (terminal_command)
	{
		gchar *tty_name = start_xterm(terminal_command);
		if (tty_name)
		{
			gdbio_send_cmd("-inferior-tty-set %s\n", tty_name);
			g_free(tty_name);
		}
		else
			return;
	}
	if (process_token)
	{
		gdbio_pop_seq(process_token);
	}
	gdbio_set_starting(TRUE);
	gdbio_do_status(GdbStartup);
	process_token = gdbio_send_seq_cmd(gdbio_target_started, "-exec-run\n");
}



void
gdbio_set_target_pid(GPid pid)
{
	gdbio_info_func(_("Started target process. (pid=%d)\n"), pid);
	target_pid = pid;
}



GPid
gdbio_get_target_pid(void)
{
	return target_pid;
}
