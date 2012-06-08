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
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

#include "breakpoint.h"
#include "debug_module.h"

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
	GString *message;
	GString *command;
	GString *error_message;
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
static GSource *gdb_src;

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
and it's nessesary to refresh files list */
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
	
	g_source_destroy(gdb_src);
	
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
		lines = g_list_append (lines, line);
	}
	
	return lines;
}

/*
 * write a command to a gdb channel and flush with a newlinw character 
 */
static void gdb_input_write_line(const gchar *line)
{
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	
	char command[1000];
	sprintf(command, "%s\n", line);
	
	while (strlen(command))
	{
		st = g_io_channel_write_chars(gdb_ch_in, command, strlen(command), &count, &err);
		strcpy(command, command + count);
		if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
		{
#ifdef DEBUG_OUTPUT
			dbg_cbs->send_message(err->message, "red");
#endif
			break;
		}
	}

	st = g_io_channel_flush(gdb_ch_in, &err);
	if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
	{
#ifdef DEBUG_OUTPUT
		dbg_cbs->send_message(err->message, "red");
#endif
	}
}

/*
 * free memory occupied by a queue item 
 */
static void free_queue_item(queue_item *item)
{
	g_string_free(item->message, TRUE);
	g_string_free(item->command, TRUE);
	g_string_free(item->error_message, TRUE);
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

	if (message)
	{
		item->message = g_string_new(message);
	}
	item->command = g_string_new(command);
	if (error_message)
	{
		item->error_message = g_string_new(error_message);
	}
	item->format_error_message = format_error_message;

	return g_list_append(queue, (gpointer)item);
} 

/*
 * asyncronous output reader
 * reads from startup async commands.
 * looks for a command completion (normal or abnormal), if noraml - executes next command
 */
static void exec_async_command(const gchar* command);
static gboolean on_read_async_output(GIOChannel * src, GIOCondition cond, gpointer data)
{
	gchar *line;
	gsize length;
	
	if (G_IO_STATUS_NORMAL != g_io_channel_read_line(src, &line, NULL, &length, NULL))
		return TRUE;		

	*(line + length) = '\0';

	if ('^' == line[0])
	{
		/* got some result */

		GList *lines;
		GList *commands = (GList*)data;
		gchar *coma;

		g_source_remove(gdb_id_out);

		lines = read_until_prompt();
		g_list_foreach(lines, (GFunc)g_free, NULL);
		g_list_free (lines);

		coma = strchr(line, ',');
		if (coma)
		{
			*coma = '\0';
			coma++;
		}
		else
			coma = line + strlen(line);
		
		if (!strcmp(line, "^done"))
		{
			/* command completed succesfully - run next command if exists */
			if (commands->next)
			{
				/* if there are commads left */
				queue_item *item;

				commands = commands->next;
				item = (queue_item*)commands->data;

				/* send message to debugger messages window */
				if (item->message)
				{
					dbg_cbs->send_message(item->message->str, "grey");
				}

				gdb_input_write_line(item->command->str);

				gdb_id_out = g_io_add_watch(gdb_ch_out, G_IO_IN, on_read_async_output, commands);
			}
			else
			{
				/* all commands completed */
				free_commands_queue(commands);

				/* removing read callback */
				g_source_remove(gdb_id_out);

				/* update source files list */
				update_files();

				/* -exec-run */
				exec_async_command("-exec-run &");
			}
		}
		else
		{
			queue_item *item = (queue_item*)commands->data;
			if(item->error_message)
			{
				if (item->format_error_message)
				{
					gchar* gdb_msg = g_strcompress(strstr(coma, "msg=\"") + strlen("msg=\""));

					GString *msg = g_string_new("");
					g_string_printf(msg, item->error_message->str, gdb_msg);
					dbg_cbs->report_error(msg->str);

					g_free(gdb_msg);
					g_string_free(msg, FALSE);
				}
				else
				{
					dbg_cbs->report_error(item->error_message->str);
				}
			}
			
			/* free commands queue */
			free_commands_queue(commands);

			stop();
		}
	}

	g_free(line);

	return TRUE;
}

/*
 * asyncronous gdb output reader
 * looks for a stopped event, then notifies "debug" module and removes async handler
 */
enum dbs debug_get_state(void);
static gboolean on_read_from_gdb(GIOChannel * src, GIOCondition cond, gpointer data)
{
	gchar *line;
	gsize length;
	gboolean prompt;
	
	if (G_IO_STATUS_NORMAL != g_io_channel_read_line(src, &line, NULL, &length, NULL))
		return TRUE;		

	prompt = !strcmp(line, GDB_PROMPT);
	
	*(line + length) = '\0';

	if (!prompt)
	{
		if ('~' == line[0])
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
		
	if (!target_pid && g_str_has_prefix(line, "=thread-group-created"))
	{
		*(strrchr(line, '\"')) = '\0';
		target_pid = atoi(line + strlen("=thread-group-created,id=\""));
	}
	else if (g_str_has_prefix(line, "=thread-created"))
	{
		int thread_id;

		*(strrchr(line, ',') - 1) = '\0';
		thread_id = atoi(line + strlen("=thread-created,id=\""));
		dbg_cbs->add_thread(thread_id);
	}
	else if (g_str_has_prefix(line, "=thread-exited"))
	{
		int thread_id;

		*(strrchr(line, ',') - 1) = '\0';
		thread_id = atoi(line + strlen("=thread-exited,id=\""));
		dbg_cbs->remove_thread(thread_id);
	}
	else if (g_str_has_prefix(line, "=library-loaded") || g_str_has_prefix(line, "=library-unloaded"))
	{
		file_refresh_needed = TRUE;
	}
	else if (*line == '*')
	{
		/* asyncronous record found */
		char *record = NULL;
		if ( (record = strchr(line, ',')) )
		{
			*record = '\0';
			record++;
		}
		else
			record = line + strlen(line);
		
		if (!strcmp(line, "*running"))
			dbg_cbs->set_run();
		else if (!strcmp(line, "*stopped"))
		{
			char *reason;

			/* removing read callback (will pulling all output left manually) */
			g_source_remove(gdb_id_out);

			/* looking for a reason to stop */
			reason = strstr(record, "reason=\"");
			if (reason)
			{
				char *next;

				reason += strlen("reason=\"");
				next = strstr(reason, "\"") + 1;
				*(next - 1) = '\0';
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
			}
			else
			{
				/* somehow, sometimes there can be no stop reason */
				stop_reason = SR_EXITED_NORMALLY;
			}
			
			if (SR_BREAKPOINT_HIT == stop_reason || SR_END_STEPPING_RANGE == stop_reason || SR_SIGNAL_RECIEVED == stop_reason)
			{
				gchar *thread_id = strstr(reason + strlen(reason) + 1,"thread-id=\"") + strlen("thread-id=\"");
				*(strchr(thread_id, '\"')) = '\0'; 
				
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

					dbg_cbs->set_stopped(atoi(thread_id));
				}
				else
				{
					if (!requested_interrupt)
						dbg_cbs->report_error(_("Program received a signal"));
					else
						requested_interrupt = FALSE;
						
					dbg_cbs->set_stopped(atoi(thread_id));
				}
			}
			else if (stop_reason == SR_EXITED_NORMALLY || stop_reason == SR_EXITED_SIGNALLED || stop_reason == SR_EXITED_WITH_CODE)
			{
				if (stop_reason == SR_EXITED_WITH_CODE)
				{
					gchar *code;
					gchar *message;

					code = strstr(reason + strlen(reason) + 1,"exit-code=\"") + strlen("exit-code=\"");
					*(strchr(code, '\"')) = '\0';
					message = g_strdup_printf(_("Program exited with code \"%i\""), (int)(char)strtol(code, NULL, 8));
					dbg_cbs->report_error(message);

					g_free(message);
				}

				stop();
			}
		}
	}
	else if (g_str_has_prefix (line, "^error"))
	{
		GList *lines, *iter;
		char *msg;

		/* removing read callback (will pulling all output left manually) */
		g_source_remove(gdb_id_out);

		/* set debugger stopped if is running */
		if (DBS_STOPPED != debug_get_state())
		{
			gchar *thread_id = strstr(line + strlen(line) + 1,"thread-id=\"");
			*(strchr(thread_id, '\"')) = '\0'; 

			dbg_cbs->set_stopped(atoi(thread_id));
		}

		/* get message */
		msg = strstr(line, "msg=\"") + strlen("msg=\"");
		*strrchr(msg, '\"') = '\0';
		msg = g_strcompress(msg);
		
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

		g_free(msg);
	}

	g_free(line);

	return TRUE;
}

/*
 * execute "command" asyncronously
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

	/* connect read callback to the output chanel */
	gdb_id_out = g_io_add_watch(gdb_ch_out, G_IO_IN, on_read_from_gdb, NULL);
}

/*
 * execute "command" syncronously
 * i.e. reading output right
 * after execution
 */ 
static result_class exec_sync_command(const gchar* command, gboolean wait4prompt, gchar** command_record)
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

		if ('^' == line[0])
		{
			gchar* coma = strchr(line, ',');
			if (coma)
			{
				*coma = '\0';
				coma++;
			}
			else
				coma = line + strlen(line);
			
			if (command_record)
			{
				*command_record = (gchar*)g_malloc(strlen(coma) + 1);
				strcpy(*command_record, coma);
			}
			
			if (!strcmp(line, "^done"))
				rc = RC_DONE;
			else if (!strcmp(line, "^error"))
			{
				/* save error message */
				gchar* msg = g_strcompress(strstr(coma, "msg=\"") + strlen("msg=\""));
				strcpy(err_message, msg);
				g_free(msg);
				
				rc = RC_ERROR;
			}
			else if (!strcmp(line, "^exit"))
				rc = RC_EXIT;
		}
		else if ('&' != line[0])
		{
			colorize_message (line);
		}
	}
	
	g_list_foreach(lines, (GFunc)g_free, NULL);
	g_list_free(lines);
	
	return rc;
}

/*
 * starts gdb, collects commands and start the first one
 */
static gboolean run(const gchar* file, const gchar* commandline, GList* env, GList *witer, GList *biter, const gchar* terminal_device, dbg_callbacks* callbacks)
{
	GError *err = NULL;
	const gchar *exclude[] = { "LANG", NULL };
	gchar **gdb_env = utils_copy_environment(exclude, "LANG", "C", NULL);
	gchar *working_directory = g_path_get_dirname(file);
	GList *lines, *iter;
	GList *commands = NULL;
	GString *command;
	int bp_index;
	queue_item *item;

	dbg_cbs = callbacks;

	/* spawn GDB */
	if (!g_spawn_async_with_pipes(working_directory, (gchar**)gdb_args, gdb_env,
				     GDB_SPAWN_FLAGS, NULL,
				     NULL, &gdb_pid, &gdb_in, &gdb_out, NULL, &err))
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
	g_child_watch_add(gdb_pid, on_gdb_exit, NULL);
	gdb_src = g_child_watch_source_new(gdb_pid);

	/* create GDB GIO chanels */
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
	command = g_string_new("");
	g_string_printf(command, "-file-exec-and-symbols \"%s\"", file);
	commands = add_to_queue(commands, _("~\"Loading target file.\\n\""), command->str, _("Error loading file"), FALSE);
	g_string_free(command, TRUE);

	/* setting asyncronous mode */
	commands = add_to_queue(commands, NULL, "-gdb-set target-async 1", _("Error configuring GDB"), FALSE);

	/* setting null-stop array printing */
	commands = add_to_queue(commands, NULL, "-interpreter-exec console \"set print null-stop\"", _("Error configuring GDB"), FALSE);

	/* enable pretty printing */
	commands = add_to_queue(commands, NULL, "-enable-pretty-printing", _("Error configuring GDB"), FALSE);

	/* set locale */
	command = g_string_new("");
	g_string_printf(command, "-gdb-set environment LANG=%s", g_getenv("LANG"));
	commands = add_to_queue(commands, NULL, command->str, NULL, FALSE);
	g_string_free(command, TRUE);

	/* set arguments */
	command = g_string_new("");
	g_string_printf(command, "-exec-arguments %s", commandline);
	commands = add_to_queue(commands, NULL, command->str, NULL, FALSE);
	g_string_free(command, TRUE);

	/* set passed evironment */
	iter = env;
	while (iter)
	{
		gchar *name, *value;

		name = (gchar*)iter->data;
		iter = iter->next;
		value = (gchar*)iter->data;

		command = g_string_new("");
		g_string_printf(command, "-gdb-set environment %s=%s", name, value);

		commands = add_to_queue(commands, NULL, command->str, NULL, FALSE);
		g_string_free(command, TRUE);

		iter = iter->next;
	}

	/* set breaks */
	bp_index = 1;
	while (biter)
	{
		breakpoint *bp = (breakpoint*)biter->data;
		GString *error_message = g_string_new("");

		command = g_string_new("");
		g_string_printf(command, "-break-insert -f \"\\\"%s\\\":%i\"", bp->file, bp->line);

		g_string_printf(error_message, _("Breakpoint at %s:%i cannot be set\nDebugger message: %s"), bp->file, bp->line, "%s");
		
		commands = add_to_queue(commands, NULL, command->str, error_message->str, TRUE);

		g_string_free(command, TRUE);

		if (bp->hitscount)
		{
			command = g_string_new("");
			g_string_printf(command, "-break-after %i %i", bp_index, bp->hitscount);
			commands = add_to_queue(commands, NULL, command->str, error_message->str, TRUE);
			g_string_free(command, TRUE);
		}
		if (strlen(bp->condition))
		{
			command = g_string_new("");
			g_string_printf (command, "-break-condition %i %s", bp_index, bp->condition);
			commands = add_to_queue(commands, NULL, command->str, error_message->str, TRUE);
			g_string_free(command, TRUE);
		}
		if (!bp->enabled)
		{
			command = g_string_new("");
			g_string_printf (command, "-break-disable %i", bp_index);
			commands = add_to_queue(commands, NULL, command->str, error_message->str, TRUE);
			g_string_free(command, TRUE);
		}

		g_string_free(error_message, TRUE);

		bp_index++;
		biter = biter->next;
	}

	/* set debugging terminal */
	command = g_string_new("-inferior-tty-set ");
	g_string_append(command, terminal_device);
	commands = add_to_queue(commands, NULL, command->str, NULL, FALSE);
	g_string_free(command, TRUE);

	/* connect read callback to the output chanel */
	gdb_id_out = g_io_add_watch(gdb_ch_out, G_IO_IN, on_read_async_output, commands);

	item = (queue_item*)commands->data;

	/* send message to debugger messages window */
	if (item->message)
	{
		dbg_cbs->send_message(item->message->str, "grey");
	}

	/* send first command */
	gdb_input_write_line(item->command->str);

	return TRUE;
}

/*
 * starts debugging
 */
static void restart(char* terminal_device)
{
	dbg_cbs->clear_messages();
	exec_async_command("-exec-run &");
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
	sprintf(command, "-exec-until %s:%i", file, line);
	exec_async_command(command);
}

/*
 * gets breakpoint number by file and line
 */
static int get_break_number(char* file, int line)
{
	gchar *record, *bstart;

	exec_sync_command("-break-list", TRUE, &record);
	bstart = record;

	while ( (bstart = strstr(bstart, "bkpt=")) )
	{
		gchar *fname, *file_quoted;
		int num, bline;
		gboolean break_found;

		bstart += strlen("bkpt={number=\"");
		*strchr(bstart, '\"') = '\0';
		num = atoi(bstart);
		
		bstart += strlen(bstart) + 1;
		bstart = strstr(bstart, "original-location=\"") + strlen("original-location=\"");
		*strchr(bstart, ':') = '\0';
		fname = bstart;
		
		bstart += strlen(bstart) + 1;
		*strchr(bstart, '\"') = '\0';
		bline = atoi(bstart);
		
		file_quoted = g_strdup_printf("\\\"%s\\\"", file);
		break_found = !strcmp(fname, file_quoted) && bline == line;
		g_free(file_quoted);

		if (break_found)
		{
			return num;
		}
		
		bstart += strlen(bstart) + 1;
	} 
	
	free(record);
	
	return -1;
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

		char *pos;
		int number;
		gchar *record = NULL;

		/* 1. insert breakpoint */
		sprintf (command, "-break-insert \"\\\"%s\\\":%i\"", bp->file, bp->line);
		if (RC_DONE != exec_sync_command(command, TRUE, &record))
		{
			g_free(record);
			sprintf (command, "-break-insert -f \"\\\"%s\\\":%i\"", bp->file, bp->line);
			if (RC_DONE != exec_sync_command(command, TRUE, &record))
			{
				g_free(record);
				return FALSE;
			}
		}
		/* lookup break-number */
		pos = strstr(record, "number=\"") + strlen("number=\"");
		*strchr(pos, '\"') = '\0';
		number = atoi(pos);
		g_free(record);
		/* 2. set hits count if differs from 0 */
		if (bp->hitscount)
		{
			sprintf (command, "-break-after %i %i", number, bp->hitscount);
			exec_sync_command(command, TRUE, NULL);
		}
		/* 3. set condition if exists */
		if (strlen(bp->condition))
		{
			sprintf (command, "-break-condition %i %s", number, bp->condition);
			if (RC_DONE != exec_sync_command(command, TRUE, NULL))
				return FALSE;
		}
		/* 4. disable if disabled */
		if (!bp->enabled)
		{
			sprintf (command, "-break-disable %i", number);
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
			sprintf (command, bp->enabled ? "-break-enable %i" : "-break-disable %i", bnumber);
		else if (BSA_UPDATE_HITS_COUNT == bsa)
			sprintf (command, "-break-after %i %i", bnumber, bp->hitscount);
		else if (BSA_UPDATE_CONDITION == bsa)
			sprintf (command, "-break-condition %i %s", bnumber, bp->condition);

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

		sprintf(command, "-break-delete %i", number);
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

/*
 * gets stack
 */
static GList* get_stack(void)
{
	gchar* record = NULL;
	GList *stack = NULL;
	gchar **frames, **next;
	result_class rc;

	rc = exec_sync_command("-stack-list-frames", TRUE, &record);
	if (RC_DONE != rc)
		return NULL;

	frames = g_strsplit(record, "frame=", 0);
	next = frames + 1;
	while (*next)
	{
		frame *f = frame_new();
		int line;
		gchar *pos, *fullname, *file, *from;
		
		/* adresss */
		pos = strstr(*next, "addr=\"") + strlen("addr=\"");
		*strchr(pos, '\"') = '\0';
		f->address = g_strdup(pos);
		pos += strlen(pos) + 1;

		/* function */
		pos = strstr(pos, "func=\"") + strlen("func=\"");
		*strchr(pos, '\"') = '\0';
		f->function = g_strdup(pos);
		pos += strlen(pos) + 1;

		/* file: fullname | file | from */
		fullname = strstr(pos, "fullname=\"");
		file = strstr(pos, "file=\"");
		from = strstr(pos, "from=\"");
		
		if (fullname)
		{
			fullname += strlen("fullname=\"");
			pos = fullname;
			*strchr(pos, '\"') = '\0';
			f->file = g_strdup(pos);
			pos += strlen(pos) + 1;
		}
		else if (file)
		{
			file += strlen("file=\"");
			pos = file;
			*strchr(pos, '\"') = '\0';
			f->file = g_strdup(pos);
			pos += strlen(pos) + 1;
		}
		else if (from)
		{
			from += strlen("from=\"");
			pos = from;
			*strchr(pos, '\"') = '\0';
			f->file = g_strdup(pos);
			pos += strlen(pos) + 1;
		}
		else
		{
			f->file = g_strdup("");
		}
		
		/* whether source is available */
		f->have_source = fullname ? TRUE : FALSE;

		/* line */
		line = 0;
		pos = strstr(pos, "line=\"");
		if (pos)
		{
			pos += strlen("line=\"");
			*strchr(pos, '\"') = '\0';
			line = atoi(pos);
			pos += strlen(pos) + 1;
		}
		f->line = line;

		stack = g_list_append(stack, f);

		next++;
	}
	g_strfreev(frames);	
	
	free(record);
	
	return stack;
}

/*
 * unescapes hex values (\0xXXX) to readable chars
 * converting it from wide character value to char
 */
static gchar* unescape_hex_values(gchar *src)
{
	GString *dest = g_string_new("");
	
	gchar *slash;
	while ( (slash = strstr(src, "\\x")) )
	{
		char hex[4] = { 0, 0, 0, '\0' };
		wchar_t wc;

		/* append what has been missed
		unescaping it in advance */
		if (slash - src)
		{
			gchar *missed = g_strndup(src, slash - src);
			gchar *unescaped = g_strcompress(missed);
			g_string_append(dest, unescaped);
			g_free(missed);
			g_free(unescaped);
		}

		strncpy(hex, slash + 2, 3);
		wc = (wchar_t)strtol(hex, NULL, 16);

		if (iswalpha(wc))
		{
			gchar mb[5];
			int len = wctomb(mb, wc);
			mb[len] = '\0';
			g_string_append(dest, mb);
		}
		else
			g_string_append_len(dest, slash, 5);
		
		src = slash + 5;
	}
	
	if (strlen(src))
	{
		gchar *unescaped = g_strcompress(src);
		g_string_append(dest, unescaped);
		g_free(unescaped);
	}

	return g_string_free(dest, FALSE);
}

/*
 * checks if pc pointer points to the 
 * valid printable charater
 */
static gboolean isvalidcharacter(gchar *pc, gboolean utf8)
{
	if (utf8)
		return -1 != g_utf8_get_char_validated(pc, -1);
	else
		return isprint(*pc);
}

/*
 * unescapes string, handles octal characters representations
 */
static gchar* unescape_octal_values(gchar *text)
{
	GString *value = g_string_new("");
	
	gboolean utf8 = g_str_has_suffix(getenv("LANG"), "UTF-8");

	gchar *tmp = g_strdup(text);
	gchar *unescaped = g_strcompress(tmp);

	gchar *pos = unescaped;
	while (*pos)
	{
		if (isvalidcharacter(pos, utf8))
		{
			if (utf8)
			{
				/* valid utf8 character, copy to output
				 and move to the next character */
				gchar *next = g_utf8_next_char(pos);
				g_string_append_len(value, pos, next - pos);
				pos = next;
			}
			else
			{
				g_string_append_len(value, pos++, 1);
			}
		}
		else
		{
			/* not a valid character, convert it to its octal representation
			 and append to the result string */
			gchar *invalid = g_strndup(pos, 1);
			gchar *escaped = g_strescape(invalid, NULL);

			g_string_append(value, escaped);

			g_free(escaped);
			g_free(invalid);

			pos += 1;
		}
	}

	g_free(tmp);

	return g_string_free (value, FALSE);
}

/*
 * unescapes value string, handles hexidecimal and octal characters representations
 */
static gchar *unescape(gchar *text)
{
	gchar *retval = NULL;

	/* create string copy */
	gchar *value = g_strdup(text);

	/* make first unescaping */
	gchar *tmp = g_strcompress(value);

	/* make first unescaping */
	if (strstr(tmp, "\\x"))
		retval = unescape_hex_values(tmp);
	else
		retval = unescape_octal_values(tmp);

	g_free(tmp);
	g_free(value);

	return retval;
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
		gchar *record = NULL;
		gchar *pos;
		gchar *expression;
		int numchild;
		gchar *value;

		/* path expression */
		sprintf(command, "-var-info-path-expression \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);
		pos = strstr(record, "path_expr=\"") + strlen("path_expr=\"");
		*(strrchr(pos, '\"')) = '\0';
		expression = unescape(pos);
		g_string_assign(var->expression, expression);
		g_free(expression);
		g_free(record);
		
		/* children number */
		sprintf(command, "-var-info-num-children \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);
		pos = strstr(record, "numchild=\"") + strlen("numchild=\"");
		*(strchr(pos, '\"')) = '\0';
		numchild = atoi(pos);
		var->has_children = numchild > 0;
		g_free(record);

		/* value */
		sprintf(command, "-data-evaluate-expression \"%s\"", var->expression->str);
		exec_sync_command(command, TRUE, &record);
		pos = strstr(record, "value=\"");
		if (!pos)
		{
			g_free(record);
			sprintf(command, "-var-evaluate-expression \"%s\"", varname);
			exec_sync_command(command, TRUE, &record);
			pos = strstr(record, "value=\"");
		}
		pos +=  + strlen("value=\"");
		*(strrchr(pos, '\"')) = '\0';
		value = unescape(pos);
		g_string_assign(var->value, value);
		g_free(value);
		g_free(record);

		/* type */
		sprintf(command, "-var-info-type \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);
		pos = strstr(record, "type=\"") + strlen("type=\"");
		*(strchr(pos, '\"')) = '\0';
		g_string_assign(var->type, pos);
		g_free(record);

		vars = vars->next;
	}
}

/*
 * updates files list
 */
static void update_files(void)
{
	GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	gchar *record = NULL;
	gchar *pos;

	if (files)
	{
		/* free previous list */
		g_list_foreach(files, (GFunc)g_free, NULL);
		g_list_free(files);
		files = NULL;
	}

	exec_sync_command("-file-list-exec-source-files", TRUE, &record);
	pos = record;
	while ( (pos = strstr(pos, "fullname=\"")) )
	{
		pos += strlen("fullname=\"");
		*(strchr(pos, '\"')) = '\0';
		if (!g_hash_table_lookup(ht, pos))
		{
			g_hash_table_insert(ht, (gpointer)pos, (gpointer)1);
			files = g_list_append(files, g_strdup(pos));
		}
			
		pos += strlen(pos) + 1;
	}

	g_hash_table_destroy(ht);
	g_free(record);
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
			sprintf(command, "-var-delete %s", var->internal->str);
			exec_sync_command(command, TRUE, NULL);
		}
		
		/* reset all variables fields */
		variable_reset(var);
	}
	
	/* create GDB variables, adding successfully created
	variables to the list then passed for updatind */
	for (iter = watches; iter; iter = iter->next)
	{
		variable *var = (variable*)iter->data;
		gchar *record = NULL;
		gchar *pos;
		gchar *escaped;

		/* try to create variable */
		escaped = g_strescape(var->name->str, NULL);
		sprintf(command, "-var-create - * \"%s\"", escaped);
		g_free(escaped);

		if (RC_DONE != exec_sync_command(command, TRUE, &record))
		{
			/* do not include to updating list, move to next watch */
			var->evaluated = FALSE;
			g_string_assign(var->internal, "");
			g_free(record);
			
			continue;
		}
		
		/* find and assign internal name */
		pos = strstr(record, "name=\"") + strlen("name=\"");
		*strchr(pos, '\"') = '\0'; 
		g_string_assign(var->internal, pos);
		g_free(record);			
		
		var->evaluated = TRUE;

		/* add to updating list */
		updating = g_list_append(updating, var);
	}
	
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
	GList *unevaluated = NULL, *iter;
	const char *gdb_commands[2];
	int i;

	/* remove all previous GDB variables for autos */
	for (iter = autos; iter; iter = iter->next)
	{
		variable *var = (variable*)iter->data;
		
		sprintf(command, "-var-delete %s", var->internal->str);
		exec_sync_command(command, TRUE, NULL);
	}

	g_list_foreach(autos, (GFunc)variable_free, NULL);
	g_list_free(autos);
	autos = NULL;
	
	/* add current autos to the list */
	
	gdb_commands[0] = g_strdup_printf("-stack-list-arguments 0 %i %i", active_frame, active_frame);
	gdb_commands[1] = "-stack-list-locals 0";
	for (i = 0; i < sizeof (gdb_commands) / sizeof(*gdb_commands); i++)
	{
		gchar *record = NULL, *pos;

		result_class rc = exec_sync_command(gdb_commands[i], TRUE, &record);
		if (RC_DONE != rc)
			break;

		pos = record;
		while ((pos = strstr(pos, "name=\"")))
		{
			variable *var;
			gchar *create_record = NULL, *escaped;

			pos += strlen("name=\"");
			*(strchr(pos, '\"')) = '\0';

			var = variable_new(pos, i ? VT_LOCAL : VT_ARGUMENT);

			/* create new gdb variable */
			escaped = g_strescape(pos, NULL);
			sprintf(command, "-var-create - * \"%s\"", escaped);
			g_free(escaped);

			/* form new variable */
			if (RC_DONE == exec_sync_command(command, TRUE, &create_record))
			{
				gchar *intname = strstr(create_record, "name=\"") + strlen ("name=\"");
				*strchr(intname, '\"') = '\0';
				var->evaluated = TRUE;
				g_string_assign(var->internal, intname);
				autos = g_list_append(autos, var);

				g_free(create_record);
			}
			else
			{
				var->evaluated = FALSE;
				g_string_assign(var->internal, "");
				unevaluated = g_list_append(unevaluated, var);
			}
			
			pos += strlen(pos) + 1;
		}
		g_free(record);
	}
	g_free((void*)gdb_commands[0]);
	
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
	gchar *record = NULL;
	gchar *pos = NULL;
	int numchild;

	/* children number */
	sprintf(command, "-var-info-num-children \"%s\"", path);
	rc = exec_sync_command(command, TRUE, &record);
	if (RC_DONE != rc)
		return NULL;
	pos = strstr(record, "numchild=\"") + strlen("numchild=\"");
	*(strchr(pos, '\"')) = '\0';
	numchild = atoi(pos);
	g_free(record);
	if (!numchild)
		return NULL;
	
	/* recursive get children and put into list */
	sprintf(command, "-var-list-children \"%s\"", path);
	rc = exec_sync_command(command, TRUE, &record);
	if (RC_DONE == rc)
	{
		pos = record;
		while ( (pos = strstr(pos, "child={")) )
		{
			gchar *name, *internal;
			variable *var;
			
			/* name */
			pos = strstr(pos, "name=\"") + strlen("name=\"");
			*(strstr(pos, "\",exp=\"")) = '\0';
			internal = pos;
			pos += strlen(pos) + 1;

			/* exp */
			pos = strstr(pos, "exp=\"") + strlen("exp=\"");
			*(strstr(pos, "\",numchild=\"")) = '\0';
			
			name = g_strcompress(pos);
			
			var = variable_new2(name, internal, VT_CHILD);
			var->evaluated = TRUE;
			
			pos += strlen(pos) + 1;

			children = g_list_prepend(children, var);
		
			g_free(name);
		}
	}
	g_free(record);
	
	get_variables(children);

	return children;
}

/*
 * add new watch 
 */
static variable* add_watch(gchar* expression)
{
	gchar command[1000];
	gchar *record = NULL, *escaped, *pos;
	GList *vars = NULL;
	variable *var = variable_new(expression, VT_WATCH);

	watches = g_list_append(watches, var);

	/* try to create a variable */
	escaped = g_strescape(expression, NULL);
	sprintf(command, "-var-create - * \"%s\"", escaped);
	g_free(escaped);

	if (RC_DONE != exec_sync_command(command, TRUE, &record))
	{
		g_free(record);
		return var;
	}
	
	pos = strstr(record, "name=\"") + strlen("name=\"");
	*strchr(pos, '\"') = '\0'; 
	g_string_assign(var->internal, pos);
	var->evaluated = TRUE;

	vars = g_list_append(NULL, var);
	get_variables(vars);

	g_free(record);
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
			sprintf(command, "-var-delete %s", internal);
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
	gchar *record = NULL, *pos;
	char command[1000];
	result_class rc;

	sprintf (command, "-data-evaluate-expression \"%s\"", expression);
	rc = exec_sync_command(command, TRUE, &record);
	
	if (RC_DONE != rc)
	{
		g_free(record);
		return NULL;
	}

	pos = strstr(record, "value=\"") + strlen("value=\"");
	*(strrchr(pos, '\"')) = '\0';

	return unescape(pos);
}

/*
 * request GDB interrupt 
 */
static gboolean request_interrupt(void)
{
#ifdef DEBUG_OUTPUT
	char msg[1000];
	sprintf(msg, "interrupting pid=%i", target_pid);
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


