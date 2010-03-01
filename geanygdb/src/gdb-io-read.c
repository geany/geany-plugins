/*
 * gdb-io-read.c - Output reading functions for GDB wrapper library.
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

#include <stdlib.h>
#include <sys/time.h>

#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "geanyplugin.h"
#include "gdb-io-priv.h"

static GSList *source_files = NULL;
static gboolean starting = FALSE;



static void
free_string_list(GSList ** list)
{
	GSList *p;
	for (p = *list; p; p = p->next)
	{
		if (p->data)
		{
			g_free(p->data);
		}
	}
	*list = NULL;
}



static void
free_source_list()
{
	free_string_list(&source_files);
}



static gint
find_file_and_fullname(gconstpointer data, gconstpointer user_data)
{
	GdbLxValue *v = (GdbLxValue *) data;
	gchar *ref = (gchar *) user_data;
	HSTR(v->hash, fullname);
	HSTR(v->hash, file);
	return (fullname && file
		&& (g_str_equal(ref, file) || g_str_equal(ref, fullname))) ? 0 : -1;
}



static void
parse_file_list_cb(gpointer data, gpointer user_data)
{
	GdbLxValue *v = (GdbLxValue *) data;
	if (v && (v->type == vt_HASH))
	{
		HSTR(v->hash, fullname);
		HSTR(v->hash, file);
		if (file && !fullname)
		{
			if (g_slist_find_custom((GSList *) user_data, file, find_file_and_fullname))
			{
				return;
			}
		}
		if (fullname)
		{
			file = fullname;
		}
		if (file)
		{
			if (!g_slist_find_custom(source_files, file, (GCompareFunc) strcmp))
			{
				source_files = g_slist_append(source_files, g_strdup(file));
			}
		}
	}
}



static void handle_response_line(gchar * str, gchar ** list);


static void
handle_response_lines(gchar ** list)
{
	if (list)
	{
		gint i;
		for (i = 0; list[i]; i++)
		{
			handle_response_line(list[i], list);
		}
	}
}



static gboolean
response_is_error(gchar * resp, gchar ** list)
{
	if (strncmp(resp, "^error", 6) == 0)
	{
		handle_response_line(resp, list);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

#define CHK_RESP_ERR(resp, list) if (response_is_error(resp,list)) { return; }

#define IsDigit g_ascii_isdigit

static void
parse_process_info(gint seq, gchar ** list, gchar * resp)
{
	CHK_RESP_ERR(resp, list);
	gdbio_pop_seq(seq);
	if (g_str_equal(resp, "^done"))
	{
		gchar *pidstr = strchr(list[0], ' ');
		if (pidstr)
		{
			GPid pid = -1;
			while (g_ascii_isspace(*pidstr))
			{
				pidstr++;
			}
			if (IsDigit(*pidstr))
			{
				gchar *end = pidstr;
				while (IsDigit(*end))
				{
					end++;
				}
				*end = '\0';
				pid = gdbio_atoi(pidstr);
				if ((pid > 0) && (!gdbio_get_target_pid()))
				{
					gdbio_set_target_pid(pid);
					gdbio_send_cmd("-exec-continue\n");
				}
			}
		}
	}
}


GHashTable *
gdbio_get_results(gchar * resp, gchar ** list)
{
	if (strncmp(resp, "^error", 6) == 0)
	{
		if (resp[6] == ',')
		{
			GHashTable *h = gdblx_parse_results(resp + 7);
			HSTR(h, msg);
			gchar *tmp = NULL;
			if (msg)
			{
				if (g_str_equal(msg, "unknown error"))
				{
					gint len = g_strv_length(list);
					if ((len > 1) && list[len - 2] && *list[len - 2])
					{
						tmp = list[len - 2];
						if (tmp[0] == '&')
						{
							tmp++;
						}
						tmp = g_strcompress(tmp);
						g_strstrip(tmp);
						msg = tmp;
					}
				}
				gdbio_error_func(msg);
				if (tmp)
				{
					g_free(tmp);
				}
			}
			if (h)
			{
				g_hash_table_destroy(h);
			}
		}
		return NULL;
	}
	if (strncmp(resp, "^done,", 6) == 0)
		return gdblx_parse_results(resp + 6);
	if (strncmp(resp, "*stopped,", 9) == 0)
	{
		gdbio_do_status(GdbStopped);
		return gdblx_parse_results(resp + 9);
	}
	return NULL;
}



static void handle_response_line(gchar * str, gchar ** list);


void
gdbio_set_starting(gboolean s)
{
	starting = s;
}

void
gdbio_target_started(gint seq, gchar ** list, gchar * resp)
{
	if ((strncmp(resp, "^error", 6) == 0) && (!gdbio_get_target_pid()))
	{
		gdbio_error_func(_("Error starting target process!\n"));
		gdbio_do_status(GdbFinished);
	}
	else
	{
		handle_response_lines(list);
	}
}



static void
set_main_break(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	HTAB(h, bkpt);
	gdbio_pop_seq(seq);
	if (bkpt)
	{
		if (gdblx_check_keyval(bkpt, "number", "1"))
		{
			gdbio_do_status(GdbLoaded);
		}
	}
	if (h)
		g_hash_table_destroy(h);
}


void
gdbio_parse_file_list(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	HLST(h, files);
	gdbio_pop_seq(seq);
	if (files)
	{
		free_source_list();
		g_slist_foreach(files, parse_file_list_cb, files);
		free_source_list();
		gdbio_send_seq_cmd(set_main_break, "-break-insert _start\n");
	}
	else
	{
		gdbio_error_func
			(_("This executable does not appear to contain the required debugging information."));
	}
	if (h)
		g_hash_table_destroy(h);
}





static gboolean
do_step_func(GHashTable * h, gchar * reason)
{
	HTAB(h, frame);
	HSTR(frame, fullname);
	HSTR(frame, line);
	if (fullname && line)
	{
		if (gdbio_setup.step_func)
		{
			gchar *p;
			for (p = reason; *p; p++)
			{
				if (*p == '-')
				{
					*p = ' ';
				}
			}
			gdbio_setup.step_func(fullname, line, reason);
		}
		else
		{
			gdbio_info_func("%s:%s", fullname, line);
		}
		return TRUE;
	}
	else
	{
		HSTR(frame, func);
		if (func)
		{
			return TRUE;
		}
	}
	return FALSE;
}



#define reason_is(r) (r && reason && g_str_equal(reason, r))

static void
finish_function(gint seq, gchar ** list, gchar * resp)
{
	if (strncmp(resp, "^running", 8) == 0)
	{
		gdbio_set_running(TRUE);
		gdbio_do_status(GdbRunning);
	}
	else
	{
		GHashTable *h = gdbio_get_results(resp, list);
		HSTR(h, reason);
		gdbio_pop_seq(seq);
		if (reason_is("function-finished"))
		{
			gdbio_do_status(GdbStopped);
			do_step_func(h, reason);
		}
		else
		{
			handle_response_lines(list);
		}
		if (h)
			g_hash_table_destroy(h);
	}
}


static void
return_function(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	gdbio_pop_seq(seq);
	if (h)
	{
		do_step_func(h, "returned");
	}
	else
	{
		handle_response_lines(list);
	}
}




static void
watchpoint_trigger(GHashTable * h, GHashTable * wp, gchar * reason)
{
	HTAB(h, value);
	HSTR(wp, exp);
	HSTR(wp, number);
	HSTR(value, new);
	HSTR(value, old);
	gchar *readval = gdblx_lookup_string(value, "value");
	if (new && old)
	{
		gdbio_info_func("%s #%s  expression:%s  old-value:%s  new-value:%s\n",
				reason, number ? number : "?", exp ? exp : "?", old, new);
	}
	else
	{
		if (old)
		{
			gdbio_info_func("%s #%s  expression:%s  value:%s", reason,
					number ? number : "?", exp ? exp : "?", old);
		}
		else
		{
			if (new)
			{
				gdbio_info_func("%s #%s  expression:%s  value:%s", reason,
						number ? number : "?", exp ? exp : "?", new);
			}
			else
			{
				if (readval)
				{
					gdbio_info_func("%s #%s  expression:%s  value:%s", reason,
							number ? number : "?", exp ? exp : "?",
							readval);
				}
				else
				{
					gdbio_info_func("%s #%s  expression:%s", reason,
							number ? number : "?", exp ? exp : "?");
				}
			}
		}
	}
}

static gboolean
handle_results_hash(GHashTable * h, gchar * rectype, gchar ** list)
{
	if (g_str_equal(rectype, "^error"))
	{
		HSTR(h, msg);
		gchar *tmp = NULL;
		if (msg)
		{
			if (g_str_equal(msg, "unknown error"))
			{
				gint len = g_strv_length(list);
				if ((len > 1) && list[len - 2] && *list[len - 2])
				{
					tmp = list[len - 2];
					if (tmp[0] == '&')
					{
						tmp++;
					}
					tmp = g_strcompress(tmp);
					g_strstrip(tmp);
					msg = tmp;
				}
			}
			gdbio_error_func(msg);
			if (tmp)
			{
				g_free(tmp);
			}
			return TRUE;
		}
		else
			return FALSE;
	}
	if (g_str_equal(rectype, "^done"))
	{

		HTAB(h, frame);
		if (frame)
		{
			HSTR(frame, fullname);
			HSTR(frame, line);
			if (fullname && line)
			{
				return do_step_func(h, "done");
			}
		}
		return FALSE;
	}
	if (g_str_equal(rectype, "*stopped"))
	{
		HSTR(h, reason);
		if (!reason)
		{
			return FALSE;
		}
		if (reason_is("breakpoint-hit"))
		{
			if (gdblx_check_keyval(h, "bkptno", "1"))
			{
				gdbio_send_seq_cmd(parse_process_info,
						   "-interpreter-exec console \"info proc\"\n");
				return TRUE;
			}
			else
			{
				return (do_step_func(h, reason));
			}
			return FALSE;
		}
		gdbio_set_running(FALSE);
		if (reason_is("signal-received"))
		{
			HSTR(h, signal_name);
			HSTR(h, signal_meaning);
			HSTR(h, thread_id);
			HTAB(h, frame);
			HSTR(frame, func);
			HSTR(frame, file);
			HSTR(frame, fullname);
			HSTR(frame, line);
			HSTR(frame, addr);
			HSTR(frame, from);
			HLST(frame, args);
			if (!fullname)
			{
				fullname = "??";
			}
			if (!file)
			{
				file = "??";
			}
			if (!line)
			{
				line = "??";
			}
			if (!args)
			{
				args = NULL;
			}
			if (signal_name && signal_meaning && thread_id && frame &&
			    addr && func && file && fullname)
			{
				if (gdbio_setup.signal_func)
				{
					GdbSignalInfo si;
					si.signal_name = signal_name;
					si.signal_meaning = signal_meaning;
					si.addr = addr;
					si.func = func;
					si.file = file;
					si.fullname = fullname;
					si.line = line;
					si.from = from;
					gdbio_setup.signal_func(&si);
				}
				else
				{
					gdbio_info_func
						(_("Program received signal %s (%s) at %s in function %s() at %s:%s"),
						 signal_name, signal_meaning, addr, func, file,
						 line);
				}
				return TRUE;
			}
		}


		if (reason_is("end-stepping-range"))
		{
			return do_step_func(h, reason);
		}
		if (reason_is("function-finished"))
		{
			return do_step_func(h, reason);
		}
		if (reason_is("location-reached"))
		{
			return do_step_func(h, reason);
		}
		if (reason_is("watchpoint-trigger"))
		{
			HTAB(h, wpt);
			if (wpt)
			{
				watchpoint_trigger(h, wpt, reason);
			}
			return do_step_func(h, reason);
		}
		if (reason_is("access-watchpoint-trigger"))
		{
			HTAB(h, hw_awpt);
			if (hw_awpt)
			{
				watchpoint_trigger(h, hw_awpt, reason);
			}
			return do_step_func(h, reason);
		}
		if (reason_is("read-watchpoint-trigger"))
		{
			HTAB(h, hw_rwpt);
			if (hw_rwpt)
			{
				watchpoint_trigger(h, hw_rwpt, reason);
			}
			return do_step_func(h, reason);
		}
		if (reason_is("watchpoint-scope"))
		{
			HSTR(h, wpnum);
			gdbio_info_func(_("Watchpoint #%s out of scope"), wpnum ? wpnum : "?");
			gdbio_send_cmd("-exec-continue\n");
			return do_step_func(h, reason);
		}

		if (reason_is("exited-signalled"))
		{
			HSTR(h, signal_name);
			HSTR(h, signal_meaning);
			gdbio_info_func(_("Program exited on signal %s (%s).\n"),
					signal_name ? signal_name : "UNKNOWN",
					signal_meaning ? signal_meaning : _("Unknown signal"));
			gdbio_target_exited(signal_name);
			return TRUE;
		}
		if (reason_is("exited"))
		{
			HSTR(h, exit_code);
			gchar *tail = NULL;
			gint ec = -1;
			if (exit_code)
			{
				ec = strtoull(exit_code, &tail, 8);
				if ((!tail) || (*tail))
				{
					ec = -1;
				}
			}
			gdbio_info_func(_("Program exited with code %d [%s]\n"), ec,
					exit_code ? exit_code : _("(unknown)"));
			gdbio_target_exited(exit_code);
			return TRUE;
		}
		if (g_str_equal(reason, "exited-normally"))
		{
			gdbio_info_func(_("Program exited normally.\n"));
			gdbio_target_exited("0");
			return TRUE;
		}
	}
	return FALSE;
}



static void
handle_response_line(gchar * str, gchar ** list)
{
	gchar *rv = str;
	if (!rv)
	{
		return;
	}
	switch (rv[0])
	{
		case '~':
		case '@':
		case '&':
			{
				rv++;
				if (rv[0] == '"')
				{
					gint len = strlen(rv);
					memmove(rv, rv + 1, len);
					if (rv[len - 2] == '"')
					{
						rv[len - 2] = '\0';
					}
				}
				rv = g_strcompress(rv);
				gdbio_info_func(rv);
				g_free(rv);
				break;
			}
		case '^':
		case '*':
			{
				gchar *comma = strchr(rv, ',');
				if (comma)
				{
					GHashTable *h = gdblx_parse_results(comma + 1);
					*comma = '\0';
					if (g_str_equal(rv, "*stopped"))
					{
						gdbio_do_status(GdbStopped);
					}
					if (!handle_results_hash(h, rv, list))
					{
						gdblx_dump_table(h);
					}
					g_hash_table_destroy(h);
				}
				else
				{
					if (g_str_equal(rv, "^running"))
					{
						if (starting)
						{
							starting = FALSE;
						}
                        gdbio_do_status(GdbRunning);
						gdbio_set_running(TRUE);
					}
				}
				break;
			}
		default:
			{
				break;
			}
	}
}




#define prompt "\n(gdb) \n"
#define prlen 8


#define starts_with_token(resp) \
  ( IsDigit(resp[0]) && IsDigit(resp[1]) && \
    IsDigit(resp[2]) && IsDigit(resp[3]) && \
    IsDigit(resp[4]) && IsDigit(resp[5]) && \
    strchr("^*=+", resp[6]) )


void
gdbio_consume_response(GString * recv_buf)
{
	gchar *eos = NULL;
	do
	{
		if (recv_buf->len)
		{
			eos = strstr(recv_buf->str, prompt);
		}
		else
		{
			eos = NULL;
		}
		if (eos)
		{
			gint seq = -1;
			gchar seqbuf[SEQ_LEN + 2];
			ResponseHandler handler = NULL;
			gchar **lines;
			gint len;
			*eos = '\0';
			lines = g_strsplit(recv_buf->str, "\n", 0);
			*eos = '\n';
			len = g_strv_length(lines);
			g_string_erase(recv_buf, 0, (eos - recv_buf->str) + 8);
			if (len)
			{
				gchar *resp = lines[len - 1];
				if (starts_with_token(resp))
				{
					strncpy(seqbuf, resp, SEQ_LEN);
					seqbuf[SEQ_LEN] = '\0';
					seq = gdbio_atoi(seqbuf);
					if (seq >= 0)
					{
						handler = gdbio_seq_lookup(seq);
						if (handler)
						{
							memmove(resp, resp + SEQ_LEN,
								strlen(resp + SEQ_LEN) + 1);
							g_strstrip(resp);
							handler(seq, lines, resp);
							g_strfreev(lines);
							do_loop();
							continue;
						}
						else
						{
							g_printerr
								("***Error: Could not find handler for token #%s\n",
								 seqbuf);
						}
					}
				}
			}
			if (lines)
			{
				handle_response_lines(lines);
				g_strfreev(lines);
			}
		}
		do_loop();
	}
	while (eos);
}





void
gdbio_continue()
{
	gdbio_send_cmd("-exec-continue\n");
}




void
gdbio_return()
{
	gdbio_send_seq_cmd(return_function, "-exec-return\n");
}


void
gdbio_finish()
{
	gdbio_send_seq_cmd(finish_function, "-exec-finish\n");
}
