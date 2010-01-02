/*
 * gdb-io-stack.c - Stack information functions for GDB wrapper library.
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

#include <string.h>
#include <glib.h>

#include "gdb-io-priv.h"
#include "support.h"

static GdbListFunc gdbio_stack_list_func = NULL;
static GSList *frame_list = NULL;

/*
  Max number of frames to return in stack list -
  you can increase if you want, but too large
  value can be *very* slow!
*/
#define MAX_STACK_RETURN 1024



static void
free_frame_list()
{
	GSList *p;
	for (p = frame_list; p; p = p->next)
	{
		if (p->data)
		{
			GdbFrameInfo *f = p->data;
			if (f->func)
			{
				g_free(f->func);
			}
			if (f->filename)
			{
				g_free(f->filename);
			}
			if (f->args)
			{
				gdbio_free_var_list(f->args);
			}
			g_free(f);
			p->data = NULL;
		}
	}
	g_slist_free(frame_list);
	frame_list = NULL;
}



static void
stack_cb(gpointer data, gpointer user_data)
{
	GdbLxValue *v = (GdbLxValue *) data;
	if (v && (v->type == vt_HASH))
	{
		GHashTable *frame = v->hash;
		HSTR(frame, level);
		HSTR(frame, addr);
		HSTR(frame, func);
		HSTR(frame, file);
		HSTR(frame, fullname);
		HSTR(frame, line);
		if (!fullname)
			fullname = file;
		if (level && addr && func && fullname && line)
		{
			GdbFrameInfo *frame_info = g_new0(GdbFrameInfo, 1);
			strncpy(frame_info->level, level, sizeof(frame_info->level) - 1);
			strncpy(frame_info->addr, addr, sizeof(frame_info->addr) - 1);
			strncpy(frame_info->line, line, sizeof(frame_info->line) - 1);
			frame_info->func = g_strdup(func);
			frame_info->filename = g_strdup(fullname);
			frame_list = g_slist_append(frame_list, frame_info);
		}
	}
}



static void
merge_stack_args_cb(gpointer data, gpointer user_data)
{
	GdbLxValue *v = (GdbLxValue *) data;
	if (v && (v->type = vt_HASH))
	{
		GHashTable *hash = v->hash;
		HSTR(hash, level);
		HLST(hash, args);
		if (level && args)
		{
			gchar *tail;
			gint n = strtoull(level, &tail, 10);
			GdbFrameInfo *frame = NULL;
			GSList *p;
			for (p = frame_list; p; p = p->next)
			{
				if (p->data)
				{
					GdbFrameInfo *f = p->data;
					if (gdbio_atoi(f->level) == n)
					{
						frame = f;
						break;
					}
				}
			}
			if (frame)
			{
				for (p = args; p; p = p->next)
				{
					v = p->data;
					if (v && (v->type = vt_HASH))
					{
						HSTR(v->hash, name);
						HSTR(v->hash, value);
						if (name && value)
						{
							GdbVar *arg = g_new0(GdbVar, 1);
							arg->name = g_strdup(name);
							arg->value = g_strdup(value);
							frame->args =
								g_slist_append(frame->args, arg);
						}
					}
				}
			}
		}
	}
}



static void
parse_stack_args(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	HLST(h, stack_args);
	gdbio_pop_seq(seq);
	if (stack_args)
	{
		if (frame_list)
		{
			g_slist_foreach(stack_args, merge_stack_args_cb, NULL);
			gdbio_stack_list_func(frame_list);
			free_frame_list();
		}
	}
	if (h)
		g_hash_table_destroy(h);
}



static void
parse_stack_list(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	HLST(h, stack);
	gdbio_pop_seq(seq);
	if (stack)
	{
		g_slist_foreach(stack, stack_cb, h);
		if (frame_list)
		{
			gint len = g_slist_length(frame_list);
			if (len >= MAX_STACK_RETURN)
			{
				gdbio_error_func
					(ngettext(
						"Stack too deep to display!\n(Showing only %d frame)",
						"Stack too deep to display!\n(Showing only %d frames)",
						 len), len);
			}
			gdbio_send_seq_cmd(parse_stack_args, "-stack-list-arguments 1 0 %d\n",
					   len - 1);
		}
	}
	if (h)
		g_hash_table_destroy(h);
}



void
gdbio_show_stack(GdbListFunc func)
{
	gdbio_stack_list_func = func;
	if (func)
	{
		gdbio_send_seq_cmd(parse_stack_list, "-stack-list-frames 0 %d\n",
				   MAX_STACK_RETURN - 1);
	}
}
