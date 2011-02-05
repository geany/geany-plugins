/*
 *      dbm_gdm.c
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*
 * 		Implementation of struct _dbg_module for bash
 */

#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <gtk/gtk.h>

#include "breakpoint.h"
#include "debug_module.h"

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

/* module features */
#define MODULE_FEATURES 0

/* bash spawn flags */
#define BASH_SPAWN_FLAGS \
	G_SPAWN_SEARCH_PATH | \
	G_SPAWN_DO_NOT_REAP_CHILD

/* callbacks to use for messaging, error reporting and state change alerting */
static dbg_callbacks* dbg_cbs;

/* terminal device name */
gchar *terminal_device = NULL;

/* bash command line arguments*/
static gchar *bash_args[] = { "bash", "--debugger", NULL, NULL };

/* bash pid*/
static GPid bash_pid = 0;

/* target pid*/
static GPid target_pid = 0;

/* GSource to watch bash exit */
static GSource *bash_src;

/* channels for bash input/output */
static gint bash_in;
static gint bash_out;
static GIOChannel *bash_ch_in;
static GIOChannel *bash_ch_out;

/* bash output event source id */
static guint bash_id_out;

/* buffer for the error message */
static char err_message[1000];

/* locals list */
static GList *locals = NULL;

/* watches list */
static GList *watches = NULL;

/* forward declarations */
static void stop();
static variable* add_watch(gchar* expression);
static void update_watches();
static void update_locals();

/* list of start messages, to show them in init if initialization is successfull */
static GList *start_messages = NULL;

/*
 * frees startup messages list
 */
static void free_start_messages()
{
	g_list_foreach(start_messages, (GFunc)g_free, NULL);
	g_list_free(start_messages);
	start_messages = NULL;
}

/*
 * shutdown GIOChannel
 */
static void shutdown_channel(GIOChannel ** ch)
{
	if (*ch)
	{
		GError *err = NULL;
		gint fd = g_io_channel_unix_get_fd(*ch);
		g_io_channel_shutdown(*ch, TRUE, &err);
		g_io_channel_unref(*ch);
		*ch = NULL;
		if (fd >= 0)
		{
			close(fd);
		}
	}
}

/*
 * called on bash exit
 */
static void on_bash_exit(GPid pid, gint status, gpointer data)
{
	bash_pid = target_pid = 0;
	g_spawn_close_pid(pid);
	shutdown_channel(&bash_ch_in);
	shutdown_channel(&bash_ch_out);
	
	/* delete locals */
	g_list_foreach(locals, (GFunc)g_free, NULL);
	locals = NULL;
	
	/* delete watches */
	g_list_foreach(watches, (GFunc)g_free, NULL);
	watches = NULL;
	
	g_source_destroy(bash_src);
	
	dbg_cbs->set_exited(0);
}

/*
 * reads bash stdout until the end
 */
static gchar** read_to_the_end()
{
	struct pollfd pfd;
	pfd.fd = bash_out;
	pfd.events = POLLIN;
	pfd.revents = 0;
	
	GString *out = g_string_new("");
	
	while(poll(&pfd, 1, 100))
	{
		gchar curbuf[1024];

		GIOStatus st;
		GError *err = NULL;
		gsize count;

		g_io_channel_read_chars(bash_ch_out, curbuf, sizeof(curbuf) - 1, &count, &err);
		gboolean have_error = err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF);
		if (!have_error && count)
		{
			curbuf[count] = '\0';
			g_string_append(out, curbuf);
		}
	}

	gchar **lines = g_strsplit(out->str, "\n", 0);
	g_string_free(out, TRUE);

	return lines;
}

/*
 * asyncronous bash output reader
 * looks for a stopped event, then notifies "debug" module and removes async handler
 */
static gboolean on_read_from_bash(GIOChannel * src, GIOCondition cond, gpointer data)
{
	gchar *line;
	gint length;
	
	if (G_IO_STATUS_NORMAL != g_io_channel_read_line(src, &line, NULL, &length, NULL))
		return TRUE;		

	*(line + length) = '\0';

	g_free(line);

	return TRUE;
}

/*
 * execute "command" asyncronously
 * after writing command to an input channel
 * connects reader to output channel and exits
 * after execution
 */ 
static void exec_async_command(gchar* command)
{
#ifdef DEBUG_OUTPUT
	dbg_cbs->send_message(command, "red");
#endif
	
	/* write command to bash input channel */
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	
	char bash_command[1000];
	sprintf(bash_command, "%s\n", command);
	
	while (strlen(bash_command))
	{
		st = g_io_channel_write_chars(bash_ch_in, bash_command, strlen(bash_command), &count, &err);
		strcpy(bash_command, bash_command + count);
		if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
		{
#ifdef DEBUG_OUTPUT
			dbg_cbs->send_message("Error sending command", "red");
#endif
			break;
		}
	}

	/* flush the chanel */
	st = g_io_channel_flush(bash_ch_in, &err);

	/* connect read callback to the output chanel */
	bash_id_out = g_io_add_watch(bash_ch_out, G_IO_IN, on_read_from_bash, NULL);
}

/*
 * execute "command" syncronously
 * i.e. reading output right
 * after execution
 */ 
static gboolean exec_sync_command(gchar* command, gchar** output)
{

	dbg_cbs->report_error(command);

#ifdef DEBUG_OUTPUT
	dbg_cbs->send_message(command, "red");
#endif

	/* write command to bash input channel */
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	
	char bash_command[1000];
	sprintf(bash_command, "%s\n", command);
	while (strlen(bash_command))
	{
		st = g_io_channel_write_chars(bash_ch_in, bash_command, strlen(bash_command), &count, &err);
		strcpy(bash_command, bash_command + count);
		if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
		{
#ifdef DEBUG_OUTPUT
			dbg_cbs->send_message("Error sending command", "red");
#endif
			break;
		}
	}
	st = g_io_channel_flush(bash_ch_in, &err);
	
	gchar **lines = read_to_the_end ();
	int i = 0;
	while (lines[i++])
		dbg_cbs->send_message(lines[i - 1], "red");
	g_strfreev(lines);
	
	return TRUE;
}

/*
 * starts gdb and sets its parameners
 */
static gboolean init(dbg_callbacks* callbacks)
{
	dbg_cbs = callbacks;
 
	return TRUE;
}

static gboolean load(char* file, char* commandline, GList* env, GList *witer)
{
	GError *err = NULL;

	bash_args[2] = file;
	
	/* spawn bash */
	//gchar **bash_env = utils_copy_environment(NULL, NULL);
	if (!g_spawn_async_with_pipes(NULL, bash_args, NULL,
				     BASH_SPAWN_FLAGS, NULL,
				     NULL, &bash_pid, &bash_in, &bash_out, NULL, &err))
	{
		dbg_cbs->report_error(_("Failed to spawn bash process"));
		return 0;
	}
	
	/* set handler for bash process exit event */ 
	g_child_watch_add(bash_pid, on_bash_exit, NULL);
	bash_src = g_child_watch_source_new(bash_pid);

	/* create GIOChanel for reading from bash */
	bash_ch_in = g_io_channel_unix_new(bash_in);
	g_io_channel_set_encoding(bash_ch_in, NULL, NULL);
	g_io_channel_set_buffered(bash_ch_in, FALSE);
	
	/* create GIOChanel for writing to bash */
	bash_ch_out = g_io_channel_unix_new(bash_out);
	g_io_channel_set_encoding(bash_ch_out, NULL, NULL);
	g_io_channel_set_buffered(bash_ch_out, FALSE);

	/* reading starting bash messages */
	gchar **lines = read_to_the_end ();
	int i = 0;
	while (lines[i++])
		dbg_cbs->send_message(lines[i - 1], "red");
	g_strfreev(lines);
	
	return TRUE;
}

/*
 * starts debugging
 */
static void run(char* terminal_device)
{
	/* setting tty */
	gchar command[1000];
	sprintf (command, "tty %s", terminal_device);
	exec_sync_command(command, NULL);
}

/*
 * stops bash
 */
static void stop()
{
	/* write command to bash input channel */
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	
	char *bash_command = "quit";
	while (strlen(bash_command))
	{
		st = g_io_channel_write_chars(bash_ch_in, bash_command, strlen(bash_command), &count, &err);
		strcpy(bash_command, bash_command + count);
		if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
		{
#ifdef DEBUG_OUTPUT
			dbg_cbs->send_message("Error sending command", "red");
#endif
			break;
		}
	}
	st = g_io_channel_flush(bash_ch_in, &err);
	//exec_sync_command("-bash-exit", FALSE, NULL);
}

/*
 * resumes bash
 */
static void resume()
{
	//exec_async_command("-exec-continue");
}

/*
 * step over
 */
static void step_over()
{
	//exec_async_command("-exec-next");
}

/*
 * step into
 */
static void step_into()
{
	//exec_async_command("-exec-step");
}

/*
 * step out
 */
static void step_out()
{
	//exec_async_command("-exec-finish");
}

/*
 * execute until
 */
static void execute_until(gchar *file, int line)
{
}

/*
 * set breakpoint
 */
static gboolean set_break(breakpoint* bp, break_set_activity bsa)
{
	return TRUE;
}

/*
 * removes breakpoint
 */
static gboolean remove_break(breakpoint* bp)
{
	return TRUE;
}

/*
 * gets stack
 */
static GList* get_stack()
{
	GList *stack = NULL;
	return stack;
}


/*
 * updates watches list 
 */
static void update_watches()
{
}

/*
 * updates locals list 
 */
static void update_locals()
{
}

/*
 * get locals list 
 */
static GList* get_locals ()
{
	return g_list_copy(locals);
}

/*
 * get watches list 
 */
static GList* get_watches ()
{
	return g_list_copy(watches);
}

/*
 * get list of children 
 */
static GList* get_children (gchar* path)
{
	return NULL;
}

/*
 * add new watch 
 */
static variable* add_watch(gchar* expression)
{
	return NULL;	
}

/*
 * remove watch 
 */
static void remove_watch(gchar* path)
{
}

/*
 * evaluates given expression and returns the result
 */
static gchar *evaluate_expression(gchar *expression)
{
	return "";
}

/*
 * request bash interrupt 
 */
static gboolean request_interrupt()
{
	return FALSE;
}

/*
 * get bash error messages 
 */
static gchar* error_message()
{
	return err_message;
}

/*
 * define bash debug module 
 */
DBG_MODULE_DEFINE(bash);


