/*
 *      dbm_gdm.c
 *
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
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
 * 		Implementation of struct _dbg_module for GDB
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <wctype.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>
extern GeanyData		*geany_data;

#include "breakpoint.h"
#include "debug_module.h"
#include "gdb_mi.h"

/* module features */
#define MODULE_FEATURES MF_ASYNC_BREAKS

/* GDB spawn flags */
#define GDB_SPAWN_FLAGS \
	G_SPAWN_SEARCH_PATH | \
	G_SPAWN_DO_NOT_REAP_CHILD

/* GDB prompt */
#define GDB_PROMPT "(gdb) \n"

/* enumeration for GDB command execution status */
typedef enum _result_class {
	RC_DONE,
	RC_EXIT,
	RC_ERROR
} result_class;

/* structure to keep async command data (command line, messages) */
typedef struct _queue_item {
	gchar *message;
	gchar *command;
	gchar *error_message;
	gboolean format_error_message;
} queue_item;

/* enumeration for stop reason */
enum sr {
	SR_BREAKPOINT_HIT,
	SR_END_STEPPING_RANGE,
	SR_EXITED_NORMALLY,
	SR_SIGNAL_RECIEVED,
	SR_EXITED_SIGNALLED,
	SR_EXITED_WITH_CODE,
} stop_reason;

/* callbacks to use for messaging, error reporting and state change alerting */
static dbg_callbacks* dbg_cbs;

/* GDB command line arguments*/
static const gchar *gdb_args[] = { "gdb", "-i=mi", NULL };

/* GDB pid*/
static GPid gdb_pid = 0;

/* target pid*/
static GPid target_pid = 0;

/* GSource to watch GDB exit */
static guint gdb_src_id;

/* channels for GDB input/output */
static gint gdb_in;
static gint gdb_out;
static GIOChannel *gdb_ch_in;
static GIOChannel *gdb_ch_out;

/* GDB output event source id */
static guint gdb_id_out;

/* buffer for the error message */
char err_message[1000];

/* flag, showing that on debugger stop we have to call a callback */
gboolean requested_interrupt = FALSE;

/* autos list */
static GList *autos = NULL;

/* watches list */
static GList *watches = NULL;

/* loaded files list */
static GList *files = NULL;

/* set to true if library was loaded/unloaded
and it's necessary to refresh files list */
static gboolean file_refresh_needed = FALSE;

/* current frame number */
static int active_frame = 0;

/* forward declarations */
static void stop(void);
static variable* add_watch(gchar* expression);
static void update_watches(void);
static void update_autos(void);
static void update_files(void);

/*
 * print message using color, based on message type
 */
static void colorize_message(gchar *message)
{
	const gchar *color;
	if ('=' == *message)
		color = "rose";
	else if ('^' == *message)
		color = "brown";
	else if ('*' == *message)
		color = "blue";
	else if ('~' == *message)
		color = "grey";
	else
		color = "red";

	dbg_cbs->send_message(message, color);
}

/*
 * shutdown GIOChannel
 */
static void shutdown_channel(GIOChannel ** ch)
{
	if (*ch)
	{
		gint fd = g_io_channel_unix_get_fd(*ch);
		g_io_channel_shutdown(*ch, TRUE, NULL);
		g_io_channel_unref(*ch);
		*ch = NULL;
		if (fd >= 0)
		{
			close(fd);
		}
	}
}

/*
 * called on GDB exit
 */
static void on_gdb_exit(GPid pid, gint status, gpointer data)
{
	gdb_pid = target_pid = 0;
	g_spawn_close_pid(pid);
	shutdown_channel(&gdb_ch_in);
	shutdown_channel(&gdb_ch_out);

	/* delete autos */
	g_list_foreach(autos, (GFunc)g_free, NULL);
	g_list_free(autos);
	autos = NULL;

	/* delete watches */
	g_list_foreach(watches, (GFunc)g_free, NULL);
	g_list_free(watches);
	watches = NULL;

	/* delete files */
	g_list_foreach(files, (GFunc)g_free, NULL);
	g_list_free(files);
	files = NULL;

	g_source_remove(gdb_src_id);
	gdb_src_id = 0;

	dbg_cbs->set_exited(0);
}

/*
 * reads gdb_out until "(gdb)" prompt met
 */
static GList* read_until_prompt(void)
{
	GList *lines = NULL;

	gchar *line = NULL;
	gsize terminator;
	while (G_IO_STATUS_NORMAL == g_io_channel_read_line(gdb_ch_out, &line, NULL, &terminator, NULL))
	{
		if (!strcmp(GDB_PROMPT, line))
			break;

		line[terminator] = '\0';
		lines = g_list_prepend (lines, line);
	}

	return g_list_reverse(lines);
}

/*
 * write a command to a gdb channel and flush with a newline character
 */
static void gdb_input_write_line(const gchar *line)
{
	GError *err = NULL;
	gsize count;
	const char *p;
	char command[1000];
	g_snprintf(command, sizeof command, "%s\n", line);

	for (p = command; *p; p += count)
	{
		GIOStatus st = g_io_channel_write_chars(gdb_ch_in, p, strlen(p), &count, &err);
		if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
		{
			if (err)
			{
#ifdef DEBUG_OUTPUT
				dbg_cbs->send_message(err->message, "red");
#endif
				g_clear_error(&err);
			}
			break;
		}
	}

	g_io_channel_flush(gdb_ch_in, &err);
	if (err)
	{
#ifdef DEBUG_OUTPUT
		dbg_cbs->send_message(err->message, "red");
#endif
		g_clear_error(&err);
	}
}

/*
 * free memory occupied by a queue item
 */
static void free_queue_item(queue_item *item)
{
	g_free(item->message);
	g_free(item->command);
	g_free(item->error_message);
	g_free(item);
}

/*
 * free a list of "queue_item" structures
 */
static void free_commands_queue(GList *queue)
{
	/* all commands completed */
	queue = g_list_first(queue);
	g_list_foreach(queue, (GFunc)free_queue_item, NULL);
	g_list_free(queue);
}

/*
 * add a new command ("queue_item" structure) to a list
 */
static GList* add_to_queue(GList* queue, const gchar *message, const gchar *command, const gchar *error_message, gboolean format_error_message)
{
	queue_item *item = (queue_item*)g_malloc(sizeof(queue_item));

	memset((void*)item, 0, sizeof(queue_item));

	item->message = g_strdup(message);
	item->command = g_strdup(command);
	item->error_message = g_strdup(error_message);
	item->format_error_message = format_error_message;

	return g_list_append(queue, item);
}

/*
 * asynchronous output reader
 * reads from startup async commands.
 * looks for a command completion (normal or abnormal), if normal - executes next command
 */
static void exec_async_command(const gchar* command);
static gboolean on_read_async_output(GIOChannel * src, GIOCondition cond, gpointer data)
{
	gchar *line;
	gsize length;
	struct gdb_mi_record *record;

	if (G_IO_STATUS_NORMAL != g_io_channel_read_line(src, &line, NULL, &length, NULL))
		return TRUE;

	record = gdb_mi_record_parse(line);

	if (record && record->type == '^')
	{
		/* got some result */

		GList *lines;
		GList *commands = (GList*)data;

		if (gdb_id_out)
		{
			g_source_remove(gdb_id_out);
			gdb_id_out = 0;
		}

		lines = read_until_prompt();
		g_list_foreach(lines, (GFunc)g_free, NULL);
		g_list_free (lines);

		if (!strcmp(record->klass, "done"))
		{
			/* command completed successfully - run next command if exists */
			if (commands->next)
			{
				/* if there are commands left */
				queue_item *item;

				commands = commands->next;
				item = (queue_item*)commands->data;

				/* send message to debugger messages window */
				if (item->message)
				{
					dbg_cbs->send_message(item->message, "grey");
				}

				gdb_input_write_line(item->command);

				gdb_id_out = g_io_add_watch(gdb_ch_out, G_IO_IN, on_read_async_output, commands);
			}
			else
			{
				/* all commands completed */
				free_commands_queue(commands);

				/* removing read callback */
				if (gdb_id_out)
				{
					g_source_remove(gdb_id_out);
					gdb_id_out = 0;
				}

				/* update source files list */
				update_files();

				/* -exec-run */
				exec_async_command("-exec-run");
			}
		}
		else
		{
			queue_item *item = (queue_item*)commands->data;
			if(item->error_message)
			{
				if (item->format_error_message)
				{
					const gchar* gdb_msg = gdb_mi_result_var(record->first, "msg", GDB_MI_VAL_STRING);
					gchar *msg = g_strdup_printf(item->error_message, gdb_msg);

					dbg_cbs->report_error(msg);
					g_free(msg);
				}
				else
				{
					dbg_cbs->report_error(item->error_message);
				}
			}

			/* free commands queue */
			free_commands_queue(commands);

			stop();
		}
	}

	gdb_mi_record_free(record);
	g_free(line);

	return TRUE;
}

/*
 * asynchronous gdb output reader
 * looks for a stopped event, then notifies "debug" module and removes async handler
 */
enum dbs debug_get_state(void);
static gboolean on_read_from_gdb(GIOChannel * src, GIOCondition cond, gpointer data)
{
	gchar *line;
	gsize length;
	const gchar *id;
	struct gdb_mi_record *record;

	if (G_IO_STATUS_NORMAL != g_io_channel_read_line(src, &line, NULL, &length, NULL))
		return TRUE;

	record = gdb_mi_record_parse(line);

	if (! record || record->type != GDB_MI_TYPE_PROMPT)
	{
		line[length] = '\0';
		if ((record && '~' == record->type) || '~' == line[0])
		{
			colorize_message(line);
		}
		else
		{
			gchar *compressed = g_strcompress(line);
			colorize_message(compressed);
			g_free(compressed);
		}
	}

	if (! record)
	{
		g_free(line);
		return TRUE;
	}

	if (!target_pid &&
		/* FIXME: =thread-group-created doesn't seem to exist, at least not in latest GDB docs */
		(gdb_mi_record_matches(record, '=', "thread-group-created", "id", &id, NULL) ||
		 gdb_mi_record_matches(record, '=', "thread-group-started", "pid", &id, NULL)))
	{
		target_pid = atoi(id);
	}
	else if (gdb_mi_record_matches(record, '=', "thread-created", "id", &id, NULL))
	{
		dbg_cbs->add_thread(atoi(id));
	}
	else if (gdb_mi_record_matches(record, '=', "thread-exited", "id", &id, NULL))
	{
		dbg_cbs->remove_thread(atoi(id));
	}
	else if (gdb_mi_record_matches(record, '=', "library-loaded", NULL) ||
		 gdb_mi_record_matches(record, '=', "library-unloaded", NULL))
	{
		file_refresh_needed = TRUE;
	}
	else if (gdb_mi_record_matches(record, '*', "running", NULL))
		dbg_cbs->set_run();
	else if (gdb_mi_record_matches(record, '*', "stopped", NULL))
	{
		const gchar *reason;

		/* removing read callback (will be pulling all output left manually) */
		if (gdb_id_out)
		{
			g_source_remove(gdb_id_out);
			gdb_id_out = 0;
		}

		/* looking for a reason to stop */
		if ((reason = gdb_mi_result_var(record->first, "reason", GDB_MI_VAL_STRING)) != NULL)
		{
			if (!strcmp(reason, "breakpoint-hit"))
				stop_reason = SR_BREAKPOINT_HIT;
			else if (!strcmp(reason, "end-stepping-range"))
				stop_reason = SR_END_STEPPING_RANGE;
			else if (!strcmp(reason, "signal-received"))
				stop_reason = SR_SIGNAL_RECIEVED;
			else if (!strcmp(reason, "exited-normally"))
				stop_reason = SR_EXITED_NORMALLY;
			else if (!strcmp(reason, "exited-signalled"))
				stop_reason = SR_EXITED_SIGNALLED;
			else if (!strcmp(reason, "exited"))
				stop_reason = SR_EXITED_WITH_CODE;
			/* FIXME: handle "location-reached" */
		}
		else
		{
			/* somehow, sometimes there can be no stop reason */
			stop_reason = SR_EXITED_NORMALLY;
		}

		if (SR_BREAKPOINT_HIT == stop_reason || SR_END_STEPPING_RANGE == stop_reason || SR_SIGNAL_RECIEVED == stop_reason)
		{
			const gchar *thread_id = gdb_mi_result_var(record->first, "thread-id", GDB_MI_VAL_STRING);

			active_frame = 0;

			if (SR_BREAKPOINT_HIT == stop_reason || SR_END_STEPPING_RANGE == stop_reason)
			{
				/* update autos */
				update_autos();

				/* update watches */
				update_watches();

				/* update files */
				if (file_refresh_needed)
				{
					update_files();
					file_refresh_needed = FALSE;
				}
			}
			else
			{
				if (!requested_interrupt)
				{
					gchar *msg = g_strdup_printf(_("Program received signal %s (%s)"),
					                             (gchar *) gdb_mi_result_var(record->first, "signal-name", GDB_MI_VAL_STRING),
					                             (gchar *) gdb_mi_result_var(record->first, "signal-meaning", GDB_MI_VAL_STRING));

					dbg_cbs->report_error(msg);
					g_free(msg);
				}
				else
					requested_interrupt = FALSE;
			}

			dbg_cbs->set_stopped(thread_id ? atoi(thread_id) : 0);
		}
		else if (stop_reason == SR_EXITED_NORMALLY || stop_reason == SR_EXITED_SIGNALLED || stop_reason == SR_EXITED_WITH_CODE)
		{
			if (stop_reason == SR_EXITED_WITH_CODE)
			{
				const gchar *exit_code = gdb_mi_result_var(record->first, "exit-code", GDB_MI_VAL_STRING);
				long int code = exit_code ? strtol(exit_code, NULL, 8) : 0;
				gchar *message;

				message = g_strdup_printf(_("Program exited with code \"%i\""), (int)(char)code);
				dbg_cbs->report_error(message);

				g_free(message);
			}

			stop();
		}
	}
	else if (gdb_mi_record_matches(record, '^', "error", NULL))
	{
		GList *lines, *iter;
		const gchar *msg = gdb_mi_result_var(record->first, "msg", GDB_MI_VAL_STRING);

		/* removing read callback (will pulling all output left manually) */
		if (gdb_id_out)
		{
			g_source_remove(gdb_id_out);
			gdb_id_out = 0;
		}

		/* set debugger stopped if is running */
		if (DBS_STOPPED != debug_get_state())
		{
			/* FIXME: does GDB/MI ever return a thread-id with an error?? */
			const gchar *thread_id = gdb_mi_result_var(record->first, "thread-id", GDB_MI_VAL_STRING);

			dbg_cbs->set_stopped(thread_id ? atoi(thread_id) : 0);
		}

		/* reading until prompt */
		lines = read_until_prompt();
		for (iter = lines; iter; iter = iter->next)
		{
			gchar *l = (gchar*)iter->data;
			if (strcmp(l, GDB_PROMPT))
				colorize_message(l);
			g_free(l);
		}
		g_list_free (lines);

		/* send error message */
		dbg_cbs->report_error(msg);
	}

	g_free(line);
	gdb_mi_record_free(record);

	return TRUE;
}

/*
 * execute "command" asynchronously
 * after writing command to an input channel
 * connects reader to output channel and exits
 * after execution
 */
static void exec_async_command(const gchar* command)
{
#ifdef DEBUG_OUTPUT
	dbg_cbs->send_message(command, "red");
#endif

	gdb_input_write_line(command);

	/* connect read callback to the output channel */
	gdb_id_out = g_io_add_watch(gdb_ch_out, G_IO_IN, on_read_from_gdb, NULL);
}

/*
 * execute "command" synchronously
 * i.e. reading output right
 * after execution
 */
static result_class exec_sync_command(const gchar* command, gboolean wait4prompt, struct gdb_mi_record ** command_record)
{
	GList *lines, *iter;
	result_class rc;

#ifdef DEBUG_OUTPUT
	dbg_cbs->send_message(command, "red");
#endif

	/* write command to gdb input channel */
	gdb_input_write_line(command);

	if (!wait4prompt)
		return RC_DONE;

	if (command_record)
		*command_record = NULL;

	lines = read_until_prompt();

#ifdef DEBUG_OUTPUT
	for (iter = lines; iter; iter = iter->next)
	{
		dbg_cbs->send_message((gchar*)iter->data, "red");
	}
#endif

	rc = RC_ERROR;

	for (iter = lines; iter; iter = iter->next)
	{
		gchar *line = (gchar*)iter->data;
		struct gdb_mi_record *record = gdb_mi_record_parse(line);

		if (record && '^' == record->type)
		{
			if (gdb_mi_record_matches(record, '^', "done", NULL))
				rc = RC_DONE;
			else if (gdb_mi_record_matches(record, '^', "error", NULL))
			{
				/* save error message */
				const gchar *msg = gdb_mi_result_var(record->first, "msg", GDB_MI_VAL_STRING);
				strncpy(err_message, msg ? msg : "", G_N_ELEMENTS(err_message) - 1);

				rc = RC_ERROR;
			}
			else if (gdb_mi_record_matches(record, '^', "exit", NULL))
				rc = RC_EXIT;

			if (command_record)
			{
				*command_record = record;
				record = NULL;
			}
		}
		else if (! record || '&' != record->type)
		{
			colorize_message (line);
		}
		gdb_mi_record_free(record);
	}

	g_list_foreach(lines, (GFunc)g_free, NULL);
	g_list_free(lines);

	return rc;
}


/* escapes @str so it is valid to put it inside a quoted argument
 * escapes '\' and '"'
 * unlike g_strescape(), it doesn't escape non-ASCII characters so keeps
 * all of UTF-8 */
static gchar *escape_string(const gchar *str)
{
	gchar *new = g_malloc(strlen(str) * 2 + 1);
	gchar *p;

	for (p = new; *str; str++) {
		switch (*str) {
			/* FIXME: what to do with '\n'?  can't seem to find a way to escape it */
			case '\\':
			case '"':
				*p++ = '\\';
				/* fallthrough */
			default:
				*p++ = *str;
		}
	}
	*p = 0;

	return new;
}

/*
 * starts gdb, collects commands and start the first one
 */
static gboolean run(const gchar* file, const gchar* commandline, GList* env, GList *witer, GList *biter, const gchar* terminal_device, dbg_callbacks* callbacks)
{
	const gchar *exclude[] = { "LANG", NULL };
	gchar **gdb_env = utils_copy_environment(exclude, "LANG", "C", NULL);
	gchar *working_directory = g_path_get_dirname(file);
	GList *lines, *iter;
	GList *commands = NULL;
	gchar *command;
	gchar *escaped;
	int bp_index;
	queue_item *item;

	dbg_cbs = callbacks;

	/* spawn GDB */
	if (!g_spawn_async_with_pipes(working_directory, (gchar**)gdb_args, gdb_env,
				     GDB_SPAWN_FLAGS, NULL,
				     NULL, &gdb_pid, &gdb_in, &gdb_out, NULL, NULL))
	{
		dbg_cbs->report_error(_("Failed to spawn gdb process"));
		g_free(working_directory);
		g_strfreev(gdb_env);
		return FALSE;
	}
	g_free(working_directory);
	g_strfreev(gdb_env);

	/* move gdb to it's own process group */
	setpgid(gdb_pid, 0);

	/* set handler for gdb process exit event */
	gdb_src_id = g_child_watch_add(gdb_pid, on_gdb_exit, NULL);

	/* create GDB GIO channels */
	gdb_ch_in = g_io_channel_unix_new(gdb_in);
	gdb_ch_out = g_io_channel_unix_new(gdb_out);

	/* reading starting gdb messages */
	lines = read_until_prompt();
	for (iter = lines; iter; iter = iter->next)
	{
		gchar *unescaped = g_strcompress((gchar*)iter->data);
		if (strlen(unescaped))
		{
			colorize_message((gchar*)iter->data);
		}
		g_free(unescaped);
	}
	g_list_foreach(lines, (GFunc)g_free, NULL);
	g_list_free(lines);

	/* add initial watches to the list */
	while (witer)
	{
		gchar *name = (gchar*)witer->data;

		variable *var = variable_new(name, VT_WATCH);
		watches = g_list_append(watches, var);

		witer = witer->next;
	}

	/* collect commands */

	/* loading file */
	escaped = escape_string(file);
	command = g_strdup_printf("-file-exec-and-symbols \"%s\"", escaped);
	commands = add_to_queue(commands, _("~\"Loading target file.\\n\""), command, _("Error loading file"), FALSE);
	g_free(command);
	g_free(escaped);

	/* setting asynchronous mode */
	commands = add_to_queue(commands, NULL, "-gdb-set target-async 1", _("Error configuring GDB"), FALSE);

	/* setting null-stop array printing */
	commands = add_to_queue(commands, NULL, "-interpreter-exec console \"set print null-stop\"", _("Error configuring GDB"), FALSE);

	/* enable pretty printing */
	commands = add_to_queue(commands, NULL, "-enable-pretty-printing", _("Error configuring GDB"), FALSE);

	/* set locale */
	command = g_strdup_printf("-gdb-set environment LANG=%s", g_getenv("LANG"));
	commands = add_to_queue(commands, NULL, command, NULL, FALSE);
	g_free(command);

	/* set arguments */
	command = g_strdup_printf("-exec-arguments %s", commandline);
	commands = add_to_queue(commands, NULL, command, NULL, FALSE);
	g_free(command);

	/* set passed environment */
	iter = env;
	while (iter)
	{
		gchar *name, *value;

		name = (gchar*)iter->data;
		iter = iter->next;
		value = (gchar*)iter->data;

		command = g_strdup_printf("-gdb-set environment %s=%s", name, value);
		commands = add_to_queue(commands, NULL, command, NULL, FALSE);
		g_free(command);

		iter = iter->next;
	}

	/* set breaks */
	bp_index = 1;
	while (biter)
	{
		breakpoint *bp = (breakpoint*)biter->data;
		gchar *error_message;

		escaped = escape_string(bp->file);
		command = g_strdup_printf("-break-insert -f \"\\\"%s\\\":%i\"", escaped, bp->line);
		g_free(escaped);

		error_message = g_strdup_printf(_("Breakpoint at %s:%i cannot be set\nDebugger message: %s"), bp->file, bp->line, "%s");

		commands = add_to_queue(commands, NULL, command, error_message, TRUE);

		g_free(command);

		if (bp->hitscount)
		{
			command = g_strdup_printf("-break-after %i %i", bp_index, bp->hitscount);
			commands = add_to_queue(commands, NULL, command, error_message, TRUE);
			g_free(command);
		}
		if (strlen(bp->condition))
		{
			command = g_strdup_printf ("-break-condition %i %s", bp_index, bp->condition);
			commands = add_to_queue(commands, NULL, command, error_message, TRUE);
			g_free(command);
		}
		if (!bp->enabled)
		{
			command = g_strdup_printf ("-break-disable %i", bp_index);
			commands = add_to_queue(commands, NULL, command, error_message, TRUE);
			g_free(command);
		}

		g_free(error_message);

		bp_index++;
		biter = biter->next;
	}

	/* set debugging terminal */
	command = g_strconcat("-inferior-tty-set ", terminal_device, NULL);
	commands = add_to_queue(commands, NULL, command, NULL, FALSE);
	g_free(command);

	/* connect read callback to the output channel */
	gdb_id_out = g_io_add_watch(gdb_ch_out, G_IO_IN, on_read_async_output, commands);

	item = (queue_item*)commands->data;

	/* send message to debugger messages window */
	if (item->message)
	{
		dbg_cbs->send_message(item->message, "grey");
	}

	/* send first command */
	gdb_input_write_line(item->command);

	return TRUE;
}

/*
 * starts debugging
 */
static void restart(void)
{
	dbg_cbs->clear_messages();
	exec_async_command("-exec-run");
}

/*
 * stops GDB
 */
static void stop(void)
{
	exec_sync_command("-gdb-exit", FALSE, NULL);
}

/*
 * resumes GDB
 */
static void resume(void)
{
	exec_async_command("-exec-continue");
}

/*
 * step over
 */
static void step_over(void)
{
	exec_async_command("-exec-next");
}

/*
 * step into
 */
static void step_into(void)
{
	exec_async_command("-exec-step");
}

/*
 * step out
 */
static void step_out(void)
{
	exec_async_command("-exec-finish");
}

/*
 * execute until
 */
static void execute_until(const gchar *file, int line)
{
	gchar command[1000];
	g_snprintf(command, sizeof command, "-exec-until %s:%i", file, line);
	exec_async_command(command);
}

/*
 * gets breakpoint number by file and line
 */
static int get_break_number(char* file, int line)
{
	struct gdb_mi_record *record;
	const struct gdb_mi_result *table, *body, *bkpt;
	int break_number = -1;

	exec_sync_command("-break-list", TRUE, &record);
	if (! record)
		return -1;

	table = gdb_mi_result_var(record->first, "BreakpointTable", GDB_MI_VAL_LIST);
	body = gdb_mi_result_var(table, "body", GDB_MI_VAL_LIST);
	gdb_mi_result_foreach_matched (bkpt, body, "bkpt", GDB_MI_VAL_LIST)
	{
		const gchar *number = gdb_mi_result_var(bkpt->val->v.list, "number", GDB_MI_VAL_STRING);
		const gchar *location = gdb_mi_result_var(bkpt->val->v.list, "original-location", GDB_MI_VAL_STRING);
		const gchar *colon;
		gboolean break_found = FALSE;

		if (! number || ! location)
			continue;

		colon = strrchr(location, ':');
		if (colon && atoi(colon + 1) == line)
		{
			gchar *fname;

			/* quotes around filename (location not found or something?) */
			if (*location == '"' && colon - location > 2)
				fname = g_strndup(location + 1, colon - location - 2);
			else
				fname = g_strndup(location, colon - location);
			break_found = strcmp(fname, file) == 0;
			g_free(fname);
		}
		if (break_found)
		{
			break_number = atoi(number);
			break;
		}
	}

	gdb_mi_record_free(record);

	return break_number;
}

/*
 * set breakpoint
 */
static gboolean set_break(breakpoint* bp, break_set_activity bsa)
{
	char command[1000];
	if (BSA_NEW_BREAK == bsa)
	{
		/* new breakpoint */
		struct gdb_mi_record *record;
		const struct gdb_mi_result *bkpt;
		const gchar *number;
		gchar *escaped;
		int num = 0;

		/* 1. insert breakpoint */
		escaped = escape_string(bp->file);
		g_snprintf(command, sizeof command, "-break-insert \"\\\"%s\\\":%i\"", escaped, bp->line);
		if (RC_DONE != exec_sync_command(command, TRUE, &record) || !record)
		{
			gdb_mi_record_free(record);
			record = NULL;
			g_snprintf(command, sizeof command, "-break-insert -f \"\\\"%s\\\":%i\"", escaped, bp->line);
			if (RC_DONE != exec_sync_command(command, TRUE, &record) || !record)
			{
				gdb_mi_record_free(record);
				g_free(escaped);
				return FALSE;
			}
		}
		/* lookup break-number */
		bkpt = gdb_mi_result_var(record->first, "bkpt", GDB_MI_VAL_LIST);
		if ((number = gdb_mi_result_var(bkpt, "number", GDB_MI_VAL_STRING)))
			num = atoi(number);
		gdb_mi_record_free(record);
		g_free(escaped);
		/* 2. set hits count if differs from 0 */
		if (bp->hitscount)
		{
			g_snprintf(command, sizeof command, "-break-after %i %i", num, bp->hitscount);
			exec_sync_command(command, TRUE, NULL);
		}
		/* 3. set condition if exists */
		if (strlen(bp->condition))
		{
			g_snprintf(command, sizeof command, "-break-condition %i %s", num, bp->condition);
			if (RC_DONE != exec_sync_command(command, TRUE, NULL))
				return FALSE;
		}
		/* 4. disable if disabled */
		if (!bp->enabled)
		{
			g_snprintf(command, sizeof command, "-break-disable %i", num);
			exec_sync_command(command, TRUE, NULL);
		}

		return TRUE;
	}
	else
	{
		/* modify existing breakpoint */
		int bnumber = get_break_number(bp->file, bp->line);
		if (-1 == bnumber)
			return FALSE;

		if (BSA_UPDATE_ENABLE == bsa)
			g_snprintf(command, sizeof command, bp->enabled ? "-break-enable %i" : "-break-disable %i", bnumber);
		else if (BSA_UPDATE_HITS_COUNT == bsa)
			g_snprintf(command, sizeof command, "-break-after %i %i", bnumber, bp->hitscount);
		else if (BSA_UPDATE_CONDITION == bsa)
			g_snprintf(command, sizeof command, "-break-condition %i %s", bnumber, bp->condition);

		return RC_DONE == exec_sync_command(command, TRUE, NULL);
	}

	return FALSE;
}

/*
 * removes breakpoint
 */
static gboolean remove_break(breakpoint* bp)
{
	/* find break number */
	int number = get_break_number(bp->file, bp->line);
	if (-1 != number)
	{
		result_class rc;
		gchar command[100];

		g_snprintf(command, sizeof command, "-break-delete %i", number);
		rc = exec_sync_command(command, TRUE, NULL);

		return RC_DONE == rc;
	}
	return FALSE;
}

/*
 * get active  frame
 */
static int get_active_frame(void)
{
	return active_frame;
}

/*
 * select frame
 */
static void set_active_frame(int frame_number)
{
	gchar *command = g_strdup_printf("-stack-select-frame %i", frame_number);
	if (RC_DONE == exec_sync_command(command, TRUE, NULL))
	{
		active_frame = frame_number;
		update_autos();
		update_watches();
	}
	g_free(command);
}

static int get_active_thread(void)
{
	struct gdb_mi_record *record = NULL;
	int current_thread = 0;

	if (RC_DONE == exec_sync_command("-thread-info", TRUE, &record))
	{
		const gchar *id = gdb_mi_result_var(record->first, "current-thread-id", GDB_MI_VAL_STRING);
		current_thread = id ? atoi(id) : 0;
	}
	gdb_mi_record_free(record);

	return current_thread;
}

static gboolean set_active_thread(int thread_id)
{
	gchar *command = g_strdup_printf("-thread-select %i", thread_id);
	gboolean success = (RC_DONE == exec_sync_command(command, TRUE, NULL));

	if (success)
		set_active_frame(0);

	g_free(command);

	return success;
}

/*
 * gets stack
 */
static GList* get_stack(void)
{
	struct gdb_mi_record *record = NULL;
	const struct gdb_mi_result *stack_node, *frame_node;
	GList *stack = NULL;

	if (RC_DONE != exec_sync_command("-stack-list-frames", TRUE, &record) || ! record)
	{
		gdb_mi_record_free(record);
		return NULL;
	}

	stack_node = gdb_mi_result_var(record->first, "stack", GDB_MI_VAL_LIST);
	gdb_mi_result_foreach_matched (frame_node, stack_node, "frame", GDB_MI_VAL_LIST)
	{
		const gchar *addr = gdb_mi_result_var(frame_node->val->v.list, "addr", GDB_MI_VAL_STRING);
		const gchar *func = gdb_mi_result_var(frame_node->val->v.list, "func", GDB_MI_VAL_STRING);
		const gchar *line = gdb_mi_result_var(frame_node->val->v.list, "line", GDB_MI_VAL_STRING);
		const gchar *file, *fullname;
		frame *f = frame_new();

		f->address = g_strdup(addr);
		f->function = g_strdup(func);

		/* file: fullname | file | from */
		if ((fullname = file = gdb_mi_result_var(frame_node->val->v.list, "fullname", GDB_MI_VAL_STRING)) ||
			(file = gdb_mi_result_var(frame_node->val->v.list, "file", GDB_MI_VAL_STRING)) ||
			(file = gdb_mi_result_var(frame_node->val->v.list, "from", GDB_MI_VAL_STRING)))
		{
			f->file = g_strdup(file);
		}
		else
		{
			f->file = g_strdup("");
		}

		/* whether source is available */
		f->have_source = fullname ? TRUE : FALSE;

		/* line */
		f->line = line ? atoi(line) : 0;

		stack = g_list_prepend(stack, f);
	}
	gdb_mi_record_free(record);

	return g_list_reverse(stack);
}

/*
 * updates variables from vars list
 */
static void get_variables (GList *vars)
{
	while (vars)
	{
		gchar command[1000];

		variable *var = (variable*)vars->data;

		gchar *varname = var->internal->str;
		struct gdb_mi_record *record = NULL;
		const gchar *expression = NULL;
		const gchar *numchild = NULL;
		const gchar *value = NULL;
		const gchar *type = NULL;

		/* path expression */
		g_snprintf(command, sizeof command, "-var-info-path-expression \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);
		if (record)
			expression = gdb_mi_result_var(record->first, "path_expr", GDB_MI_VAL_STRING);
		g_string_assign(var->expression, expression ? expression : "");
		gdb_mi_record_free(record);

		/* children number */
		g_snprintf(command, sizeof command, "-var-info-num-children \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);
		if (record)
			numchild = gdb_mi_result_var(record->first, "numchild", GDB_MI_VAL_STRING);
		var->has_children = numchild && atoi(numchild) > 0;
		gdb_mi_record_free(record);

		/* value */
		g_snprintf(command, sizeof command, "-data-evaluate-expression \"%s\"", var->expression->str);
		exec_sync_command(command, TRUE, &record);
		if (record)
			value = gdb_mi_result_var(record->first, "value", GDB_MI_VAL_STRING);
		if (!value)
		{
			gdb_mi_record_free(record);
			g_snprintf(command, sizeof command, "-var-evaluate-expression \"%s\"", varname);
			exec_sync_command(command, TRUE, &record);
			if (record)
				value = gdb_mi_result_var(record->first, "value", GDB_MI_VAL_STRING);
		}
		g_string_assign(var->value, value ? value : "");
		gdb_mi_record_free(record);

		/* type */
		g_snprintf(command, sizeof command, "-var-info-type \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);
		if (record)
			type = gdb_mi_result_var(record->first, "type", GDB_MI_VAL_STRING);
		g_string_assign(var->type, type ? type : "");
		gdb_mi_record_free(record);

		vars = vars->next;
	}
}

/*
 * updates files list
 */
static void update_files(void)
{
	GHashTable *ht;
	struct gdb_mi_record *record = NULL;
	const struct gdb_mi_result *files_node;

	if (files)
	{
		/* free previous list */
		g_list_foreach(files, (GFunc)g_free, NULL);
		g_list_free(files);
		files = NULL;
	}

	exec_sync_command("-file-list-exec-source-files", TRUE, &record);
	if (! record)
		return;

	ht = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	files_node = gdb_mi_result_var(record->first, "files", GDB_MI_VAL_LIST);
	gdb_mi_result_foreach_matched (files_node, files_node, NULL, GDB_MI_VAL_LIST)
	{
		const gchar *fullname = gdb_mi_result_var(files_node->val->v.list, "fullname", GDB_MI_VAL_STRING);

		if (fullname && !g_hash_table_lookup(ht, fullname))
		{
			g_hash_table_insert(ht, (gpointer)fullname, (gpointer)1);
			files = g_list_append(files, g_strdup(fullname));
		}
	}

	g_hash_table_destroy(ht);
	gdb_mi_record_free(record);
}

/*
 * updates watches list
 */
static void update_watches(void)
{
	gchar command[1000];
	GList *updating = NULL;
	GList *iter;

	/* delete all GDB variables */
	for (iter = watches; iter; iter = iter->next)
	{
		variable *var = (variable*)iter->data;

		if (var->internal->len)
		{
			g_snprintf(command, sizeof command, "-var-delete %s", var->internal->str);
			exec_sync_command(command, TRUE, NULL);
		}

		/* reset all variables fields */
		variable_reset(var);
	}

	/* create GDB variables, adding successfully created
	variables to the list then passed for updating */
	for (iter = watches; iter; iter = iter->next)
	{
		variable *var = (variable*)iter->data;
		struct gdb_mi_record *record = NULL;
		const gchar *name;
		gchar *escaped;

		/* try to create variable */
		escaped = escape_string(var->name->str);
		g_snprintf(command, sizeof command, "-var-create - * \"%s\"", escaped);
		g_free(escaped);

		if (RC_DONE != exec_sync_command(command, TRUE, &record) || !record)
		{
			/* do not include to updating list, move to next watch */
			var->evaluated = FALSE;
			g_string_assign(var->internal, "");
			gdb_mi_record_free(record);

			continue;
		}

		/* find and assign internal name */
		name = gdb_mi_result_var(record->first, "name", GDB_MI_VAL_STRING);
		g_string_assign(var->internal, name ? name : "");
		gdb_mi_record_free(record);

		var->evaluated = name != NULL;

		/* add to updating list */
		updating = g_list_prepend(updating, var);
	}
	updating = g_list_reverse(updating);

	/* update watches */
	get_variables(updating);

	/* free updating list */
	g_list_free(updating);
}

/*
 * updates autos list
 */
static void update_autos(void)
{
	gchar command[1000];
	GList *unevaluated = NULL, *vars = NULL, *iter;

	/* remove all previous GDB variables for autos */
	for (iter = autos; iter; iter = iter->next)
	{
		variable *var = (variable*)iter->data;

		g_snprintf(command, sizeof command, "-var-delete %s", var->internal->str);
		exec_sync_command(command, TRUE, NULL);
	}

	g_list_foreach(autos, (GFunc)variable_free, NULL);
	g_list_free(autos);
	autos = NULL;

	/* add current autos to the list */

	struct gdb_mi_record *record = NULL;

	g_snprintf(command, sizeof command, "-stack-list-arguments 0 %i %i", active_frame, active_frame);
	if (RC_DONE == exec_sync_command(command, TRUE, &record) && record)
	{
		const struct gdb_mi_result *stack_args = gdb_mi_result_var(record->first, "stack-args", GDB_MI_VAL_LIST);

		gdb_mi_result_foreach_matched (stack_args, stack_args, "frame", GDB_MI_VAL_LIST)
		{
			const struct gdb_mi_result *args = gdb_mi_result_var(stack_args->val->v.list, "args", GDB_MI_VAL_LIST);

			gdb_mi_result_foreach_matched (args, args, "name", GDB_MI_VAL_STRING)
			{
				variable *var = variable_new(args->val->v.string, VT_ARGUMENT);
				vars = g_list_append(vars, var);
			}
		}
	}
	gdb_mi_record_free(record);

	if (RC_DONE == exec_sync_command("-stack-list-locals 0", TRUE, &record) && record)
	{
		const struct gdb_mi_result *locals = gdb_mi_result_var(record->first, "locals", GDB_MI_VAL_LIST);

		gdb_mi_result_foreach_matched (locals, locals, "name", GDB_MI_VAL_STRING)
		{
			variable *var = variable_new(locals->val->v.string, VT_LOCAL);
			vars = g_list_append(vars, var);
		}
	}
	gdb_mi_record_free(record);

	for (iter = vars; iter; iter = iter->next)
	{
		variable *var = iter->data;
		struct gdb_mi_record *create_record = NULL;
		gchar *escaped;
		const gchar *intname;

		/* create new gdb variable */
		escaped = escape_string(var->name->str);
		g_snprintf(command, sizeof command, "-var-create - * \"%s\"", escaped);
		g_free(escaped);

		/* form new variable */
		if (RC_DONE == exec_sync_command(command, TRUE, &create_record) && create_record &&
			(intname = gdb_mi_result_var(create_record->first, "name", GDB_MI_VAL_STRING)))
		{
			var->evaluated = TRUE;
			g_string_assign(var->internal, intname);
			autos = g_list_append(autos, var);
		}
		else
		{
			var->evaluated = FALSE;
			g_string_assign(var->internal, "");
			unevaluated = g_list_append(unevaluated, var);
		}
		gdb_mi_record_free(create_record);
	}
	g_list_free(vars);

	/* get values for the autos (without incorrect variables) */
	get_variables(autos);

	/* add incorrect variables */
	autos = g_list_concat(autos, unevaluated);
}

/*
 * get autos list
 */
static GList* get_autos (void)
{
	return g_list_copy(autos);
}

/*
 * get watches list
 */
static GList* get_watches (void)
{
	return g_list_copy(watches);
}

/*
 * get files list
 */
static GList* get_files (void)
{
	return g_list_copy(files);
}

/*
 * get list of children
 */
static GList* get_children (gchar* path)
{
	GList *children = NULL;

	gchar command[1000];
	result_class rc;
	struct gdb_mi_record *record = NULL;
	const gchar *numchild;
	int n;

	/* children number */
	g_snprintf(command, sizeof command, "-var-info-num-children \"%s\"", path);
	rc = exec_sync_command(command, TRUE, &record);
	if (RC_DONE != rc || ! record)
	{
		gdb_mi_record_free(record);
		return NULL;
	}
	numchild = gdb_mi_result_var(record->first, "numchild", GDB_MI_VAL_STRING);
	n = numchild ? atoi(numchild) : 0;
	gdb_mi_record_free(record);
	if (!n)
		return NULL;

	/* recursive get children and put into list */
	g_snprintf(command, sizeof command, "-var-list-children \"%s\"", path);
	rc = exec_sync_command(command, TRUE, &record);
	if (RC_DONE == rc && record)
	{
		const struct gdb_mi_result *child_node = gdb_mi_result_var(record->first, "children", GDB_MI_VAL_LIST);

		gdb_mi_result_foreach_matched (child_node, child_node, "child", GDB_MI_VAL_LIST)
		{
			const gchar *internal = gdb_mi_result_var(child_node->val->v.list, "name", GDB_MI_VAL_STRING);
			const gchar *name = gdb_mi_result_var(child_node->val->v.list, "exp", GDB_MI_VAL_STRING);
			variable *var;

			if (! name || ! internal)
				continue;

			var = variable_new2(name, internal, VT_CHILD);
			var->evaluated = TRUE;

			children = g_list_prepend(children, var);
		}
	}
	gdb_mi_record_free(record);

	children = g_list_reverse(children);
	get_variables(children);

	return children;
}

/*
 * add new watch
 */
static variable* add_watch(gchar* expression)
{
	gchar command[1000];
	gchar *escaped;
	struct gdb_mi_record *record = NULL;
	const gchar *name;
	GList *vars = NULL;
	variable *var = variable_new(expression, VT_WATCH);

	watches = g_list_append(watches, var);

	/* try to create a variable */
	escaped = escape_string(var->name->str);
	g_snprintf(command, sizeof command, "-var-create - * \"%s\"", escaped);
	g_free(escaped);

	if (RC_DONE != exec_sync_command(command, TRUE, &record) || !record)
	{
		gdb_mi_record_free(record);
		return var;
	}

	name = gdb_mi_result_var(record->first, "name", GDB_MI_VAL_STRING);
	g_string_assign(var->internal, name ? name : "");
	var->evaluated = name != NULL;

	vars = g_list_append(NULL, var);
	get_variables(vars);

	gdb_mi_record_free(record);
	g_list_free(vars);

	return var;
}

/*
 * remove watch
 */
static void remove_watch(gchar* internal)
{
	GList *iter = watches;
	while (iter)
	{
		variable *var = (variable*)iter->data;
		if (!strcmp(var->internal->str, internal))
		{
			gchar command[1000];
			g_snprintf(command, sizeof command, "-var-delete %s", internal);
			exec_sync_command(command, TRUE, NULL);
			variable_free(var);
			watches = g_list_delete_link(watches, iter);
		}
		iter = iter->next;
	}
}

/*
 * evaluates given expression and returns the result
 */
static gchar *evaluate_expression(gchar *expression)
{
	struct gdb_mi_record *record = NULL;
	gchar *value;
	char command[1000];

	g_snprintf(command, sizeof command, "-data-evaluate-expression \"%s\"", expression);
	if (RC_DONE != exec_sync_command(command, TRUE, &record) || ! record)
	{
		gdb_mi_record_free(record);
		return NULL;
	}

	value = g_strdup(gdb_mi_result_var(record->first, "value", GDB_MI_VAL_STRING));
	gdb_mi_record_free(record);

	return value;
}

/*
 * request GDB interrupt
 */
static gboolean request_interrupt(void)
{
#ifdef DEBUG_OUTPUT
	char msg[1000];
	g_snprintf(msg, sizeof msg, "interrupting pid=%i", target_pid);
	dbg_cbs->send_message(msg, "red");
#endif

	requested_interrupt = TRUE;
	kill(target_pid, SIGINT);

	return TRUE;
}

/*
 * get GDB error messages
 */
static gchar* error_message(void)
{
	return err_message;
}

/*
 * define GDB debug module
 */
DBG_MODULE_DEFINE(gdb);


