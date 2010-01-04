/*
 * gdb-io-frame.c - Stack frame information functions for GDB wrapper library.
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

#include "geanyplugin.h"
#include "gdb-io-priv.h"


static GdbFrameFunc gdbio_locals_func = NULL;

static GSList *locals_list = NULL;
static GSList **which_list = NULL;

static gint locals_index = 0;
static gint args_index = 0;
static gint *which_index = NULL;

static void var_created(gint seq, gchar ** list, gchar * resp);
static void got_varlist(gint seq, gchar ** list, gchar * resp);

typedef enum _FrameProcState
{
	fpBegin,
	fpGotLocals,
	fpGotArgs
} FrameProcState;


static FrameProcState state = fpBegin;
static GdbFrameInfo current_frame;



static void
gdbio_free_var(GdbVar * v)
{
	if (v)
	{
		g_free(v->type);
		g_free(v->name);
		g_free(v->value);
		g_free(v->numchild);
		g_free(v);
	}
}


void
gdbio_free_var_list(GSList * args)
{
	GSList *p;
	for (p = args; p; p = p->next)
	{
		gdbio_free_var((GdbVar *) p->data);
	}
	g_slist_free(args);
}



static void
free_lists()
{
	gdbio_free_var_list(locals_list);
	locals_list = NULL;
	locals_index = 0;
	args_index = 0;
	which_list = &locals_list;
	which_index = &locals_index;
	state = fpBegin;
	g_free(current_frame.func);
	g_free(current_frame.filename);
	gdbio_free_var_list(current_frame.args);
	memset(&current_frame, 0, sizeof(current_frame));
}

static void
get_arglist()
{
	which_list = &current_frame.args;
	which_index = &args_index;
	gdbio_send_seq_cmd(got_varlist, "-stack-list-arguments 1 %s %s\n",
			   current_frame.level, current_frame.level);
}


static void
create_var(gchar * varname)
{
	gdbio_send_seq_cmd(var_created, "-var-create x%s * %s\n", varname, varname);
}


static void
var_deleted(gint seq, gchar ** list, gchar * resp)
{
	GdbVar *lv;
	gdbio_pop_seq(seq);
	(*which_index)++;
	lv = g_slist_nth_data(*which_list, *which_index);
	if (lv)
	{
		create_var(lv->name);
	}
	else
	{
		if (state == fpBegin)
		{
			state = fpGotLocals;
			get_arglist();
		}
		else
		{
			if (gdbio_locals_func)
			{
				gdbio_locals_func(&current_frame, locals_list);
			}
			free_lists();
		}
	}
}



static void
delete_var(gchar * varname)
{
	gdbio_send_seq_cmd(var_deleted, "-var-delete x%s\n", varname);
}



static gchar *
fmt_val(gchar * value)
{
	gchar buf[256];
	if (!value)
		return g_strdup("0");
	if (strlen(value) < sizeof(buf))
	{
		return g_strdup(value);
	}
	strncpy(buf, value, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	return g_strdup_printf("%s...%s", buf, strchr(buf, '"') ? "\"" : "");
}



static void
var_created(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	HSTR(h, type);
	HSTR(h, value);
	HSTR(h, numchild);
	gdbio_pop_seq(seq);
	if (type)
	{
		GdbVar *lv = g_slist_nth_data(*which_list, *which_index);
		if (lv)
		{
			lv->type = g_strdup(type ? type : "int");
			lv->value = fmt_val(value);
			lv->numchild = g_strdup(numchild ? numchild : "0");
		}
		delete_var(lv->name);
	}
	if (h)
		g_hash_table_destroy(h);
}



void
got_varlist(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	GSList *hlist = NULL;
	HLST(h, locals);
	HLST(h, stack_args);
	gdbio_pop_seq(seq);
	if (state == fpBegin)
	{
		hlist = locals;
	}
	else
	{
		GdbLxValue *v = stack_args->data;
		if (v && (v->type == vt_HASH))
		{
			HLST(v->hash, args);
			if (args)
			{
				hlist = args;
			}
		}
	}
	if (hlist)
	{
		GSList *p;
		GdbVar *lv;
		for (p = hlist; p; p = p->next)
		{
			GdbLxValue *v = p->data;
			if (v && (v->type == vt_HASH) && v->hash)
			{
				HSTR(v->hash, name);
				if (name)
				{
					lv = g_new0(GdbVar, 1);
					lv->name = g_strdup(name);
					*which_list = g_slist_append(*which_list, lv);
				}
			}
		}
		lv = g_slist_nth_data(*which_list, *which_index);
		if (lv)
		{
			create_var(lv->name);
		}
	}
	else
	{
		if (state == fpBegin)
		{
			state = fpGotLocals;
			get_arglist();
		}
		else
		{
			if (gdbio_locals_func)
			{
				gdbio_locals_func(&current_frame, locals_list);
			}
		}
	}
	if (h)
		g_hash_table_destroy(h);
}





static void
got_current_level(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	HTAB(h, frame);
	gdbio_pop_seq(seq);
	if (frame)
	{
		HSTR(frame, level);
		if (level)
		{
			HSTR(frame, addr);
			HSTR(frame, func);
			HSTR(frame, file);
			HSTR(frame, fullname);
			HSTR(frame, line);
			strncpy(current_frame.level, level, sizeof(current_frame.level) - 1);
			strncpy(current_frame.addr, addr ? addr : "",
				sizeof(current_frame.addr) - 1);
			strncpy(current_frame.line, line ? line : "",
				sizeof(current_frame.line) - 1);
			current_frame.filename = g_strdup(fullname ? fullname : file ? file : "");
			current_frame.func = g_strdup(func ? func : "");
		}
	}
	if (h)
		g_hash_table_destroy(h);
	gdbio_send_seq_cmd(got_varlist, "-stack-list-locals 1\n");
}


static void
set_current_level(gint seq, gchar ** list, gchar * resp)
{
	gdbio_pop_seq(seq);
	gdbio_send_seq_cmd(got_current_level, "-stack-info-frame\n");
}

void
gdbio_show_locals(GdbFrameFunc func, gchar * level)
{
	free_lists();
	gdbio_locals_func = func;
	gdbio_send_seq_cmd(set_current_level, "-stack-select-frame %s\n", level);
}




static gpointer
qpop(GQueue ** q)
{
	gpointer p = NULL;
	if (*q)
	{
		p = g_queue_pop_head(*q);
		if (g_queue_get_length(*q) == 0)
		{
			g_queue_free(*q);
			*q = NULL;
		}
	}
	return p;
}

static void
qpush(GQueue ** q, gpointer p)
{
	if (p)
	{
		if (!*q)
		{
			*q = g_queue_new();
		}
		g_queue_push_head(*q, p);
	}
}

static gpointer
qtop(GQueue * q)
{
	return q ? g_queue_peek_head(q) : NULL;
}



/*
static gpointer qnth(GQueue*q, gint n)
{
  return q?g_queue_peek_nth(q, n):NULL;
}

static gint qlen(GQueue*q)
{
  return q?g_queue_get_length(q):0;
}
*/


static GQueue *obj_list_queue = NULL;
static void
push_list(GSList * p)
{
	qpush(&obj_list_queue, p);
}
static void
pop_list()
{
	gdbio_free_var_list(qpop(&obj_list_queue));
}
static GSList *
top_list()
{
	return qtop(obj_list_queue);
}


static GQueue *obj_var_queue = NULL;
static void
push_var(GdbVar * p)
{
	qpush(&obj_var_queue, p);
}
static void
pop_var()
{
	gdbio_free_var(qpop(&obj_var_queue));
}
static GdbVar *
top_var()
{
	return qtop(obj_var_queue);
}




//static GdbObjectFunc gdbio_object_list_func=NULL;

static GQueue *obj_func_queue = NULL;
static void
push_func(GdbObjectFunc p)
{
	qpush(&obj_func_queue, p);
}
static void
pop_func()
{
	qpop(&obj_func_queue);
}
static GdbObjectFunc
top_func()
{
	return qtop(obj_func_queue);
}




static void
done_top()
{
	pop_var();
	pop_list();
//  pop_name();
	pop_func();
}



static void
object_deleted(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	gdbio_pop_seq(seq);
	if (h)
	{
		if (top_func() && top_var() && top_list())
		{
			top_func()(top_var(), top_list());
		}
		done_top();
		g_hash_table_destroy(h);
	}
}



static GdbVar *
hash_val_to_var(GHashTable * h)
{
	HSTR(h, name);
	if (name)
	{
		GdbVar *var = g_new0(GdbVar, 1);
		HSTR(h, type);
		HSTR(h, value);
		HSTR(h, numchild);
		var->name = g_strdup(name + 1);
		var->type = g_strdup(type ? type : "int");
		var->value = fmt_val(value);
		var->numchild = g_strdup(numchild ? numchild : "0");
		return var;
	}
	return NULL;
}


#define MAX_ITEMS 1024

static GSList *
hash_list_to_var_list(GSList * hlist)
{
	GSList *vlist = NULL;
	GSList *p;
	gint i;
	for (p = hlist, i = 0; p; p = p->next, i++)
	{
		GdbLxValue *hv = p->data;
		if (hv && (hv->type == vt_HASH) && hv->hash)
		{
			GdbVar *var = hash_val_to_var(hv->hash);
			if (var)
			{
				vlist = g_slist_append(vlist, var);
			}
		}
		if (i >= MAX_ITEMS)
		{
			GdbVar *var = g_new0(GdbVar, 1);
			var->type = g_strdup(" ");
			var->name = g_strdup_printf("* LIST TRUNCATED AT ITEM #%d *", i + 1);
			var->value = g_strdup(" ");
			var->numchild = g_strdup("0");
			vlist = g_slist_append(vlist, var);
			gdbio_error_func(_("Field list too long, not all items can be displayed.\n"));
			break;
		}
	}
	return vlist;
}



static void
object_listed(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	gdbio_pop_seq(seq);
	if (h)
	{
		HLST(h, children);
		if (children)
		{
			push_list(hash_list_to_var_list(children));
		}
		gdbio_send_seq_cmd(object_deleted, "-var-delete x%s\n", top_var()->name);
		g_hash_table_destroy(h);
	}
}



static void
object_created(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	gdbio_pop_seq(seq);
	if (h)
	{
		HSTR(h, name);
		if (name)
		{
			push_var(hash_val_to_var(h));
			gdbio_send_seq_cmd(object_listed, "-var-list-children 1 %s\n", name);
		}
		g_hash_table_destroy(h);
	}
}


void
gdbio_show_object(GdbObjectFunc func, const gchar * varname)
{

	if (func)
	{
		push_func(func);
		gdbio_send_seq_cmd(object_created, "-var-create x%s * %s\n", varname, varname);
	}
}
