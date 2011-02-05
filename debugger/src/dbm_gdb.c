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
 * 		Implementation of struct _dbg_module for GDB
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

/* enumeration for stop reason */
enum sr {
	SR_BREAKPOINT_HIT,
	SR_END_STEPPING_RANGE,
	SR_EXITED_NORMALLY,
	SR_SIGNAL_RECIEVED,
	SR_EXITED_SIGNALLED,
} stop_reason;

/* callbacks to use for messaging, error reporting and state change alerting */
dbg_callbacks* dbg_cbs;

/* GDB command line arguments*/
static gchar *gdb_args[] = { "gdb", "-i=mi", NULL };

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

/* locals list */
static GList *locals = NULL;

/* watches list */
static GList *watches = NULL;

/* loaded files list */
static GList *files = NULL;

/* set to true if library was loaded/unloaded
and it's nessesary to refresh files list */
static gboolean file_refresh_needed = FALSE;

/* forward declarations */
void stop();
variable* add_watch(gchar* expression);
void update_watches();
void update_locals();
void update_files();

/* list of start messages, to show them in init if initialization is successfull */
GList *start_messages = NULL;

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
 * print message using color, based on message type
 */
void colorize_message(gchar *message)
{
	gchar *color;
	if ('=' == *message)
		color = "rose";
	else if ('^' == *message)
		color = "brown";
	else if ('*' == *message)
		color = "blue";
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
	
	/* delete locals */
	g_list_foreach(locals, (GFunc)g_free, NULL);
	g_list_free(locals);
	locals = NULL;
	
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
 * reads gdb_out until "(gdb)" string met
 */
GList* read_until_prompt()
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
 * reads gdb_out until have data (by lines)
 */
GList* read_until_end()
{
	GList *lines = NULL;

	struct pollfd pfd;
	pfd.fd = gdb_out;
	pfd.events = POLLIN;
	pfd.revents = 0;

	while(poll(&pfd, 1, 100))
	{
		gchar *line = NULL;
		GIOStatus st;
		gsize terminator;
		GError *err = NULL;

		if(G_IO_STATUS_NORMAL == g_io_channel_read_line(gdb_ch_out, &line, NULL, &terminator, &err))
		{
			line[terminator] = '\0';
			lines = g_list_append(lines, line);
		}
		else
			dbg_cbs->report_error(err->message);
	}

	return lines;
}

/*
 * asyncronous gdb output reader
 * looks for a stopped event, then notifies "debug" module and removes async handler
 */
static gboolean on_read_from_gdb(GIOChannel * src, GIOCondition cond, gpointer data)
{
	gchar *line;
	gint length;
	
	if (G_IO_STATUS_NORMAL != g_io_channel_read_line(src, &line, NULL, &length, NULL))
		return TRUE;		

	gboolean prompt = !strcmp(line, GDB_PROMPT);
	
	*(line + length) = '\0';

	if (!prompt)
	{
		gchar *compressed = g_strcompress(line);
		colorize_message(compressed);
		g_free(compressed);
	}
		
	if (!target_pid && g_str_has_prefix(line, "=thread-group-created"))
	{
		*(strrchr(line, '\"')) = '\0';
		target_pid = atoi(line + strlen("=thread-group-created,id=\""));
	}
	else if (g_str_has_prefix(line, "=library-loaded") || g_str_has_prefix(line, "=library-unloaded"))
		file_refresh_needed = TRUE;
	else if (*line == '*')
	{
		/* asyncronous record found */
		char *record = NULL;
		if (record = strchr(line, ','))
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
			/* removing read callback (will pulling all output left manually) */
			g_source_remove(gdb_id_out);

			/* update locals */
			update_locals();

			/* update watches */
			update_watches();

			/* update files */
			if (file_refresh_needed)
			{
				update_files();
				file_refresh_needed = FALSE;
			}

			/* looking for a reason to stop */
			char *next = NULL;
			char *reason = strstr(record, "reason=\"") + strlen("reason=\"");
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
			 	
			if (SR_BREAKPOINT_HIT == stop_reason || SR_END_STEPPING_RANGE == stop_reason)
			{
				dbg_cbs->set_stopped();
			}
			else if (stop_reason == SR_SIGNAL_RECIEVED)
			{
				if (!requested_interrupt)
					dbg_cbs->report_error(_("Program received a signal"));
				else
					requested_interrupt = FALSE;
					
				dbg_cbs->set_stopped();
			}
			else if (stop_reason == SR_EXITED_NORMALLY || stop_reason == SR_EXITED_SIGNALLED)
				stop();
		}
	}
	else if (g_str_has_prefix (line, "^error"))
	{
		/* removing read callback (will pulling all output left manually) */
		g_source_remove(gdb_id_out);

		/* get message */
		char *msg = strstr(line, "msg=\"") + strlen("msg=\"");
		*strrchr(msg, '\"') = '\0';
		msg = g_strcompress(msg);
		
		/* reading until prompt */
		GList *lines = read_until_prompt();
		GList *iter = lines;
		while(iter)
		{
			gchar *l = (gchar*)iter->data;
			if (strcmp(l, GDB_PROMPT))
				colorize_message(l);
			g_free(l);
			
			iter = iter->next;
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
void exec_async_command(gchar* command)
{
#ifdef DEBUG_OUTPUT
	dbg_cbs->send_message(command, "red");
#endif
	
	/* write command to gdb input channel */
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	
	char gdb_command[1000];
	sprintf(gdb_command, "%s\n", command);
	
	while (strlen(gdb_command))
	{
		st = g_io_channel_write_chars(gdb_ch_in, gdb_command, strlen(gdb_command), &count, &err);
		strcpy(gdb_command, gdb_command + count);
		if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
		{
#ifdef DEBUG_OUTPUT
			dbg_cbs->send_message("Error sending command", "red");
#endif
			break;
		}
	}

	/* flush the chanel */
	st = g_io_channel_flush(gdb_ch_in, &err);

	/* connect read callback to the output chanel */
	gdb_id_out = g_io_add_watch(gdb_ch_out, G_IO_IN, on_read_from_gdb, NULL);
}

/*
 * execute "command" syncronously
 * i.e. reading output right
 * after execution
 */ 
result_class exec_sync_command(gchar* command, gboolean wait4prompt, gchar** command_record)
{

#ifdef DEBUG_OUTPUT
	dbg_cbs->send_message(command, "red");
#endif

	/* write command to gdb input channel */
	GIOStatus st;
	GError *err = NULL;
	gsize count;
	
	char gdb_command[1000];
	sprintf(gdb_command, "%s\n", command);
	while (strlen(gdb_command))
	{
		st = g_io_channel_write_chars(gdb_ch_in, gdb_command, strlen(gdb_command), &count, &err);
		strcpy(gdb_command, gdb_command + count);
		if (err || (st == G_IO_STATUS_ERROR) || (st == G_IO_STATUS_EOF))
		{
#ifdef DEBUG_OUTPUT
			dbg_cbs->send_message("Error sending command", "red");
#endif
			break;
		}
	}
	st = g_io_channel_flush(gdb_ch_in, &err);
	
	if (!wait4prompt)
		return RC_DONE;
	
	GList *lines = read_until_prompt();

#ifdef DEBUG_OUTPUT
	GList *line = lines;
	while (line)
	{
		dbg_cbs->send_message((gchar*)line->data, "red");
		line = line->next;
	}
#endif

	result_class rc;
	GList *iter = lines;

	while (iter)
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
		else
			colorize_message (line);

		iter = iter->next;
	}	
	
	g_list_foreach(lines, (GFunc)g_free, NULL);
	g_list_free(lines);
	
	return rc;
}

/*
 * starts gdb and sets its parameners
 */
static gboolean init(dbg_callbacks* callbacks)
{
	dbg_cbs = callbacks;

	GError *err = NULL;

	/* spawn GDB */
	const gchar *exclude[] = { "LANG", NULL };
	gchar **gdb_env = utils_copy_environment(exclude, "LANG", "C", NULL);
	const gchar *env_lang = g_getenv("LANG");
	if (!g_spawn_async_with_pipes(NULL, gdb_args, gdb_env,
				     GDB_SPAWN_FLAGS, NULL,
				     NULL, &gdb_pid, &gdb_in, &gdb_out, NULL, &err))
	{
		dbg_cbs->report_error(_("Failed to spawn gdb process"));
		return FALSE;
	}
	
	/* set handler for gdb process exit event */ 
	g_child_watch_add(gdb_pid, on_gdb_exit, NULL);
	gdb_src = g_child_watch_source_new(gdb_pid);

	/* create GIOChanel for reading from gdb */
	gdb_ch_in = g_io_channel_unix_new(gdb_in);

	/* create GIOChanel for writing to gdb */
	gdb_ch_out = g_io_channel_unix_new(gdb_out);

	/* reading starting gdb messages */
	GList *lines = read_until_prompt();

	GList *line = lines;
	while (line)
	{
		gchar *unescaped = g_strcompress((gchar*)line->data);
		if (strlen(unescaped))
			start_messages = g_list_append(start_messages, g_strdup((gchar*)line->data));
		g_free(unescaped);
		line = line->next;
	}

	g_list_foreach(lines, (GFunc)g_free, NULL);
	g_list_free(lines);
	
	/* setting asyncronous mode */
	if (RC_DONE != exec_sync_command("-gdb-set target-async 1", TRUE, NULL) ||
	/* setting null-stop array printing */
		RC_DONE != exec_sync_command("-interpreter-exec console \"set print null-stop\"", TRUE, NULL) ||
	/* enable pretty printing */
		RC_DONE != exec_sync_command("-enable-pretty-printing", TRUE, NULL))
	{
		dbg_cbs->report_error(_("Error configuring GDB"));
		free_start_messages();
		return FALSE;
	}
	
	g_strfreev(gdb_env);
	
	return TRUE;
}

/*
 * load file, set command line end environment variables
 */
gboolean load(char* file, char* commandline, GList* env, GList *witer)
{
	/* loading file */
	GString *command = g_string_new("");
	g_string_printf(command, "-file-exec-and-symbols %s", file);
	result_class rc = exec_sync_command(command->str, TRUE, NULL);
	g_string_free(command, TRUE);
	if (RC_DONE != rc)
	{
		dbg_cbs->report_error(_("Error loading file"));
		free_start_messages();
		return FALSE;
	}

	/* set arguments */
	command = g_string_new("");
	g_string_printf(command, "-exec-arguments %s", commandline);
	exec_sync_command(command->str, TRUE, NULL);
	g_string_free(command, TRUE);
	
	/* set locale */
	command = g_string_new("");
	g_string_printf(command, "-gdb-set environment LANG=%s", g_getenv("LANG"));
	exec_sync_command(command->str, TRUE, NULL);
	g_string_free(command, TRUE);
	
	/* set passed evironment */
	command = g_string_new("");
	GList *iter = env;
	while (iter)
	{
		gchar *name = (gchar*)iter->data;
		iter = iter->next;
		
		gchar *value = (gchar*)iter->data;
	
		g_string_printf(command, "-gdb-set environment %s=%s", name, value);
		exec_sync_command(command->str, TRUE, NULL);

		iter = iter->next;
	}
	g_string_free(command, TRUE);

	/* add initial watches to the list */
	while (witer)
	{
		gchar *name = (gchar*)witer->data;

		variable *var = variable_new(name);
		watches = g_list_append(watches, var);
		
		witer = witer->next;
	}

	/* update source files list */
	update_files();

	return TRUE;
}

/*
 * starts debugging
 */
void run(char* terminal_device)
{
	/* set debugging terminal */
	GString *command = g_string_new("-inferior-tty-set ");
	g_string_append(command, terminal_device);
	gchar *record = NULL;
	exec_sync_command(command->str, TRUE, &record);
	g_string_free(command, TRUE);
	g_free(record);

	/* print start messages */
	GList *iter = start_messages;
	while (iter)
	{
		dbg_cbs->send_message((gchar*)iter->data, "grey");
		iter = iter->next;
	}
	free_start_messages();

	exec_async_command("-exec-run &");
}

/*
 * stops GDB
 */
void stop()
{
	exec_sync_command("-gdb-exit", FALSE, NULL);
}

/*
 * resumes GDB
 */
void resume()
{
	exec_async_command("-exec-continue");
}

/*
 * step over
 */
void step_over()
{
	exec_async_command("-exec-next");
}

/*
 * step into
 */
void step_into()
{
	exec_async_command("-exec-step");
}

/*
 * step out
 */
void step_out()
{
	exec_async_command("-exec-finish");
}

/*
 * execute until
 */
void execute_until(gchar *file, int line)
{
	gchar command[1000];
	sprintf(command, "-exec-until %s:%i", file, line);
	exec_async_command(command);
}

/*
 * set breakpoint
 */
gboolean set_break(breakpoint* bp, break_set_activity bsa)
{
	char command[1000];
	if (BSA_NEW_BREAK == bsa)
	{
		/* new breakpoint */
		gchar *record = NULL;
		
		/* 1. insert breakpoint */
		sprintf (command, "-break-insert %s:%i", bp->file, bp->line);
		if (RC_DONE != exec_sync_command(command, TRUE, &record))
		{
			g_free(record);
			sprintf (command, "-break-insert -f %s:%i", bp->file, bp->line);
			if (RC_DONE != exec_sync_command(command, TRUE, &record))
			{
				g_free(record);
				return FALSE;
			}
		}
		/* lookup break-number */
		char *pos = strstr(record, "number=\"") + strlen("number=\"");
		*strchr(pos, '\"') = '\0';
		int number = atoi(pos);
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
 * gets breakpoint number by file and line
 */
int get_break_number(char* file, int line)
{
	gchar* record;
	result_class rc = exec_sync_command("-break-list", TRUE, &record);
	
	gchar* bstart = record;
	while (bstart = strstr(bstart, "bkpt="))
	{
		bstart += strlen("bkpt={number=\"");
		*strchr(bstart, '\"') = '\0';
		int num = atoi(bstart);
		
		bstart += strlen(bstart) + 1;
		bstart = strstr(bstart, "fullname=\"") + strlen("fullname=\"");
		*strchr(bstart, '\"') = '\0';
		gchar *fname = bstart;
		
		bstart += strlen(bstart) + 1;
		bstart = strstr(bstart, "line=\"") + strlen("line=\"");
		*strchr(bstart, '\"') = '\0';
		int bline = atoi(bstart);
		
		if (!strcmp(fname, file) && bline == line)
			return num;
		
		bstart += strlen(bstart) + 1;
	} 
	
	free(record);
	
	return -1;
}

/*
 * removes breakpoint
 */
gboolean remove_break(breakpoint* bp)
{
	/* find break number */
	int number = get_break_number(bp->file, bp->line);
	if (-1 != number)
	{
		gchar command[100];
		sprintf(command, "-break-delete %i", number);
		result_class rc = exec_sync_command(command, TRUE, NULL);
		
		return RC_DONE == rc;
	}
	return FALSE;
}

/*
 * gets stack
 */
GList* get_stack()
{
	gchar* record = NULL;
	result_class rc = exec_sync_command("-stack-list-frames", TRUE, &record);
	if (RC_DONE != rc)
		return NULL;
	
	GList *stack = NULL;

	gchar **frames = g_strsplit(record, "frame=", 0);
	gchar **next = frames + 1;
	while (*next)
	{
		frame *f = malloc(sizeof(frame));
		
		/* adresss */
		gchar* pos = strstr(*next, "addr=\"") + strlen("addr=\"");
		*strchr(pos, '\"') = '\0';
		strcpy(f->address, pos);
		pos += strlen(pos) + 1;

		/* function */
		pos = strstr(pos, "func=\"") + strlen("func=\"");
		*strchr(pos, '\"') = '\0';
		strcpy(f->function, pos);
		pos += strlen(pos) + 1;

		/* file: fullname | file | from */
		char* fullname = strstr(pos, "fullname=\"");
		char* file = strstr(pos, "file=\"");
		char* from = strstr(pos, "from=\"");
		
		if (fullname)
		{
			fullname += strlen("fullname=\"");
			pos = fullname;
			*strchr(pos, '\"') = '\0';
			strcpy(f->file, pos);
			pos += strlen(pos) + 1;
		}
		else if (file)
		{
			file += strlen("file=\"");
			pos = file;
			*strchr(pos, '\"') = '\0';
			strcpy(f->file, pos);
			pos += strlen(pos) + 1;
		}
		else if (from)
		{
			from += strlen("from=\"");
			pos = from;
			*strchr(pos, '\"') = '\0';
			strcpy(f->file, pos);
			pos += strlen(pos) + 1;
		}
		else
			strcpy(f->file, "");
		
		/* whether source is available */
		f->have_source = (gboolean)fullname;

		/* line */
		int line = 0;
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
gchar* unescape_hex_values(gchar *src)
{
	GString *dest = g_string_new("");
	
	gchar *slash;
	while (slash = strstr(src, "\\x"))
	{
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

		char hex[4] = { 0, 0, 0, '\0' };
		strncpy(hex, slash + 2, 3);
		wchar_t wc = (wchar_t)strtol(hex, NULL, 16);
	
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
gboolean isvalidcharacter(gchar *pc, gboolean utf8)
{
	if (utf8)
		return -1 != g_utf8_get_char_validated(pc, -1);
	else
		return isprint(*pc);
}

/*
 * unescapes string, handles octal characters representations
 */
gchar* unescape_octal_values(gchar *text)
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
gchar *unescape(gchar *text)
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
void get_variables (GList *vars)
{
	GList *variables;
	
	while (vars)
	{
		gchar command[1000];
		result_class rc;
		
		variable *var = (variable*)vars->data;

		gchar *varname = var->internal->str;
		gchar *record = NULL;
		gchar *pos;

		/* path expression */
		sprintf(command, "-var-info-path-expression \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);
		pos = strstr(record, "path_expr=\"") + strlen("path_expr=\"");
		*(strrchr(pos, '\"')) = '\0';
		gchar *expression = unescape(pos);
		g_string_assign(var->expression, expression);
		g_free(expression);
		
		/* children number */
		sprintf(command, "-var-info-num-children \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);

		pos = strstr(record, "numchild=\"") + strlen("numchild=\"");
		*(strchr(pos, '\"')) = '\0';
		int numchild = atoi(pos);
		var->has_children = numchild > 0;

		/* value */
		sprintf(command, "-var-evaluate-expression \"%s\"", varname);
		exec_sync_command(command, TRUE, &record);
		pos = strstr(record, "value=\"") + strlen("value=\"");
		*(strrchr(pos, '\"')) = '\0';
		gchar *value = unescape(pos);
		g_string_assign(var->value, value);
		g_free(value);

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
void update_files()
{
	if (files)
	{
		/* free previous list */
		g_list_foreach(files, (GFunc)g_free, NULL);
		g_list_free(files);
		files = NULL;
	}

	GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	gchar *record = NULL;
	exec_sync_command("-file-list-exec-source-files", TRUE, &record);
	gchar *pos = record;
	while (pos = strstr(pos, "fullname=\""))
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
void update_watches()
{
	gchar command[1000];
	result_class rc;

	/* delete all GDB variables */
	GList *iter = watches;
	while (iter)
	{
		variable *var = (variable*)iter->data;
		
		if (var->internal->len)
		{
			sprintf(command, "-var-delete %s", var->internal->str);
			exec_sync_command(command, TRUE, NULL);
		}
		
		/* reset all variables fields */
		variable_reset(var);
		
		iter = iter->next;
	}
	
	/* create GDB variables, adding successfully created
	variables to the list then passed for updatind */
	GList *updating = NULL;
	iter = watches;
	while (iter)
	{
		variable *var = (variable*)iter->data;
		
		/* try to create variable */
		gchar  *record = NULL;

		gchar *escaped = g_strescape(var->name->str, NULL);
		sprintf(command, "-var-create - * \"%s\"", escaped);
		g_free(escaped);

		if (RC_DONE != exec_sync_command(command, TRUE, &record))
		{
			/* do not include to updating list, move to next watch */
			var->evaluated = FALSE;
			g_string_assign(var->internal, "");
			g_free(record);			
			iter = iter->next;
			
			continue;
		}
		
		/* find and assign internal name */
		gchar *pos = strstr(record, "name=\"") + strlen("name=\"");
		*strchr(pos, '\"') = '\0'; 
		g_string_assign(var->internal, pos);
		g_free(record);			
		
		var->evaluated = TRUE;

		/* add to updating list */
		updating = g_list_append(updating, var);

		iter = iter->next;
	}
	
	/* update watches */
	get_variables(updating);

	/* free updating list */
	g_list_free(updating);
}

/*
 * updates locals list 
 */
void update_locals()
{
	gchar command[1000];

	/* remove all previous GDB variables for locals */
	GList *iter = locals;
	while (iter)
	{
		variable *var = (variable*)iter->data;
		
		sprintf(command, "-var-delete %s", var->internal->str);
		exec_sync_command(command, TRUE, NULL);
		
		iter = iter->next;
	}

	g_list_foreach(locals, (GFunc)variable_free, NULL);
	g_list_free(locals);
	locals = NULL;
	
	/* add current locals to the list */
	gchar *record = NULL;
	result_class rc = exec_sync_command("-stack-list-locals 0", TRUE, &record);
	if (RC_DONE != rc)
		return;

	GList *unevaluated = NULL;

	gchar *pos = record;
	while ((pos = strstr(pos, "name=\"")))
	{
		pos += strlen("name=\"");
		*(strchr(pos, '\"')) = '\0';

		variable *var = variable_new(pos);

		/* create new gdb variable */
		gchar *create_record = NULL;
		
		gchar *escaped = g_strescape(pos, NULL);
		sprintf(command, "-var-create - * \"%s\"", escaped);
		g_free(escaped);

		/* form new variable */
		if (RC_DONE == exec_sync_command(command, TRUE, &create_record))
		{
			gchar *intname = strstr(create_record, "name=\"") + strlen ("name=\"");
			*strchr(intname, '\"') = '\0';
			var->evaluated = TRUE;
			g_string_assign(var->internal, intname);
			locals = g_list_append(locals, var);

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

	/* get values for the locals (without incorrect variables) */
	get_variables(locals);
	
	/* add incorrect variables */
	locals = g_list_concat(locals, unevaluated);
}

/*
 * get locals list 
 */
GList* get_locals ()
{
	return g_list_copy(locals);
}

/*
 * get watches list 
 */
GList* get_watches ()
{
	return g_list_copy(watches);
}

/*
 * get files list 
 */
GList* get_files ()
{
	return g_list_copy(files);
}

/*
 * get list of children 
 */
GList* get_children (gchar* path)
{
	GList *children = NULL;
	
	gchar command[1000];
	result_class rc;
	gchar *record = NULL;
	gchar *pos = NULL;

	/* children number */
	sprintf(command, "-var-info-num-children \"%s\"", path);
	rc = exec_sync_command(command, TRUE, &record);
	if (RC_DONE != rc)
		return NULL;
	pos = strstr(record, "numchild=\"") + strlen("numchild=\"");
	*(strchr(pos, '\"')) = '\0';
	int numchild = atoi(pos);
	g_free(record);
	if (!numchild)
		return NULL;
	
	/* recursive get children and put into list */
	sprintf(command, "-var-list-children \"%s\"", path);
	rc = exec_sync_command(command, TRUE, &record);
	if (RC_DONE == rc)
	{
		pos = record;
		while (pos = strstr(pos, "child={"))
		{
			gchar *name, *internal;
			
			/* name */
			pos = strstr(pos, "name=\"") + strlen("name=\"");
			*(strstr(pos, "\",exp=\"")) = '\0';
			internal = pos;
			pos += strlen(pos) + 1;

			/* exp */
			pos = strstr(pos, "exp=\"") + strlen("exp=\"");
			*(strstr(pos, "\",numchild=\"")) = '\0';
			
			name = g_strcompress(pos);
			
			variable *var = variable_new2(name, internal);
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
variable* add_watch(gchar* expression)
{
	gchar command[1000];
	result_class rc;

	variable *var = variable_new(expression);
	watches = g_list_append(watches, var);

	/* try to create a variable */
	gchar *record = NULL;
	
	gchar *escaped = g_strescape(expression, NULL);
	sprintf(command, "-var-create - * \"%s\"", escaped);
	g_free(escaped);

	if (RC_DONE != exec_sync_command(command, TRUE, &record))
	{
		g_free(record);
		return var;
	}
	
	gchar *pos = strstr(record, "name=\"") + strlen("name=\"");
	*strchr(pos, '\"') = '\0'; 
	g_string_assign(var->internal, pos);
	var->evaluated = TRUE;

	GList *vars = g_list_append(NULL, var);
	get_variables(vars);

	g_free(record);
	g_list_free(vars);

	return var;	
}

/*
 * remove watch 
 */
void remove_watch(gchar* path)
{
	GList *iter = watches;
	while (iter)
	{
		variable *var = (variable*)iter->data;
		if (!strcmp(var->name->str, path))
		{
			gchar command[1000];
			sprintf(command, "-var-delete %s", var->internal->str);
			exec_sync_command(command, TRUE, NULL);
			watches = g_list_delete_link(watches, iter);
		}
		iter = iter->next;
	}
}

/*
 * evaluates given expression and returns the result
 */
gchar *evaluate_expression(gchar *expression)
{
	gchar *record = NULL;

	char command[1000];
	sprintf (command, "-data-evaluate-expression \"%s\"", expression);
	result_class rc = exec_sync_command(command, TRUE, &record);
	
	if (RC_DONE != rc)
	{
		g_free(record);
		return NULL;
	}

	gchar *pos = strstr(record, "value=\"") + strlen("value=\"");
	*(strrchr(pos, '\"')) = '\0';
	gchar *retval = unescape(pos);
	
	return retval;
}

/*
 * request GDB interrupt 
 */
gboolean request_interrupt()
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
gchar* error_message()
{
	return err_message;
}

/*
 * define GDB debug module 
 */
DBG_MODULE_DEFINE(gdb);


