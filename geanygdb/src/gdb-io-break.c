
/*
 * gdb-io-break.c - Breakpoint management functions for GDB wrapper library.
 *
 * See the file "gdb-io.h" for license information.
 *
 */


#include <string.h>
#include <glib.h>
#include "gdb-io-priv.h"
#include "support.h"


static GdbListFunc gdbio_break_list_func = NULL;

static GSList *breakpoint_list = NULL;


static void
free_breakpoint_list()
{
	GSList *p;
	for (p = breakpoint_list; p; p = p->next)
	{
		GdbBreakPointInfo *bpi = p->data;
		if (bpi)
		{
			g_free(bpi->addr);
			g_free(bpi->disp);
			g_free(bpi->enabled);
			g_free(bpi->file);
			g_free(bpi->fullname);
			g_free(bpi->func);
			g_free(bpi->line);
			g_free(bpi->number);
			g_free(bpi->times);
			g_free(bpi->type);
			g_free(bpi->what);
			g_free(bpi->cond);
			g_free(bpi->ignore);
			g_free(bpi);
		}
	}
	g_slist_free(breakpoint_list);
	breakpoint_list = NULL;
}



#define populate(rec, hash, key) \
  rec->key=gdblx_lookup_string(hash, #key""); \
  if (rec->key) {rec->key=g_strdup(rec->key);}



static void
breakpoint_cb(gpointer data, gpointer user_data)
{
	GdbLxValue *v = (GdbLxValue *) data;
	if (v && (v->type == vt_HASH) && (v->hash))
	{
		GHashTable *bkpt = v->hash;
		if (bkpt)
		{
			GdbBreakPointInfo *bpi = g_new0(GdbBreakPointInfo, 1);
			populate(bpi, bkpt, addr);
			populate(bpi, bkpt, disp);
			populate(bpi, bkpt, enabled);
			populate(bpi, bkpt, file);
			populate(bpi, bkpt, fullname);
			populate(bpi, bkpt, func);
			populate(bpi, bkpt, line);
			populate(bpi, bkpt, number);
			populate(bpi, bkpt, times);
			populate(bpi, bkpt, type);
			populate(bpi, bkpt, what);
			populate(bpi, bkpt, cond);
			populate(bpi, bkpt, ignore);
			breakpoint_list = g_slist_append(breakpoint_list, bpi);
		}
	}
}


static void
parse_break_list(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	HTAB(h, BreakpointTable);
	gdbio_pop_seq(seq);
	if (BreakpointTable && gdbio_break_list_func)
	{
		HLST(BreakpointTable, body);
		if (body)
		{
			free_breakpoint_list();
			g_slist_foreach(body, breakpoint_cb, NULL);
			gdbio_break_list_func(breakpoint_list);
			free_breakpoint_list();
		}
		else
		{
			gdbio_break_list_func(NULL);
		}
	}
	if (h)
		g_hash_table_destroy(h);
}



void
gdbio_show_breaks(GdbListFunc func)
{
	gdbio_break_list_func = func;
	if (func)
	{
		gdbio_send_seq_cmd(parse_break_list, "-break-list\n");
	}
}



static void
added_break(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	gdbio_pop_seq(seq);
	if (h)
	{
		HTAB(h, bkpt);
		if (bkpt)
		{
			HSTR(bkpt, file);
			HSTR(bkpt, line);
			HSTR(bkpt, func);
			HSTR(bkpt, number);
			if (func)
			{
				gdbio_info_func(_("Added breakpoint #%s in %s() at %s:%s\n"), number,
						func, file, line);
			}
			else
			{
				gdbio_info_func(_("Added breakpoint #%s at %s:%s\n"), number, file,
						line);
			}

		}
		else
		{
			HTAB(h, wpt);
			if (wpt)
			{
				HSTR(wpt, exp);
				HSTR(wpt, number);
				gdbio_info_func(_("Added write watchpoint #%s for %s\n"), number, exp);
			}
			else
			{
				HTAB(h, hw_awpt);
				if (hw_awpt)
				{
					HSTR(hw_awpt, exp);
					HSTR(hw_awpt, number);
					gdbio_info_func(_("Added read/write watchpoint #%s for %s\n"),
							number, exp);
				}
				else
				{
					HTAB(h, hw_rwpt);
					if (hw_rwpt)
					{
						HSTR(hw_rwpt, exp);
						HSTR(hw_rwpt, number);
						gdbio_info_func
							(_("Added read watchpoint #%s for %s\n"),
							 number, exp);
					}
				}
			}
		}
		g_hash_table_destroy(h);
	}
	if (gdbio_break_list_func)
	{
		gdbio_show_breaks(gdbio_break_list_func);
	}
}

/* opt is "-r" (read) or "-a" (r/w) or NULL or empty (write) */
void
gdbio_add_watch(GdbListFunc func, const gchar * option, const gchar * varname)
{
	gdbio_break_list_func = func;
	gdbio_send_seq_cmd(added_break, "-break-watch %s %s\n", option ? option : "", varname);
}



void
gdbio_add_break(GdbListFunc func, const gchar * filename, const gchar * locn)
{
	gdbio_break_list_func = func;
	if (filename && *filename)
	{
		gdbio_send_seq_cmd(added_break, "-break-insert %s:%s\n", filename, locn);
	}
	else
	{
		gdbio_send_seq_cmd(added_break, "-break-insert %s\n", locn);
	}
}


static void
deleted_break(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	gdbio_pop_seq(seq);
	if (h)
	{
		g_hash_table_destroy(h);
		gdbio_info_func(_("Watch/breakpoint deleted.\n"));
	}
	if (gdbio_break_list_func)
	{
		gdbio_show_breaks(gdbio_break_list_func);
	}
}



static void
toggled_break(gint seq, gchar ** list, gchar * resp)
{
	gdbio_pop_seq(seq);
	if (strncmp(resp, "^error", 6) == 0)
	{
		if (resp[6] == ',')
		{
			GHashTable *h = gdblx_parse_results(resp + 7);
			HSTR(h, msg);

			if (msg)
			{
				gchar *tmp =
					g_strconcat(_("Failed to toggle breakpoint -\n"), msg, NULL);
				gdbio_error_func(tmp);
				if (tmp)
				{
					g_free(tmp);
				}
			}
			else
			{
			}
			if (h)
			{
				g_hash_table_destroy(h);
			}
		}
	}
	else
	{
		gdbio_info_func(_("Watch/breakpoint toggled.\n"));
	}
}



static void
edited_break(gint seq, gchar ** list, gchar * resp)
{
	GHashTable *h = gdbio_get_results(resp, list);
	gdbio_pop_seq(seq);
	if (h)
	{
		g_hash_table_destroy(h);
		gdbio_info_func(_("Watch/breakpoint modified.\n"));
	}
}


void
gdbio_delete_break(GdbListFunc func, const gchar * number)
{
	gdbio_break_list_func = func;
	gdbio_send_seq_cmd(deleted_break, "-break-delete %s\n", number);
}


void
gdbio_enable_break(const gchar * number, gboolean enabled)
{
	gdbio_send_seq_cmd(toggled_break, "-break-%s %s\n", enabled ? "enable" : "disable", number);
}


void
gdbio_ignore_break(const gchar * number, const gchar * times)
{
	gdbio_send_seq_cmd(edited_break, "-break-after %s %s\n", number, times);
}


void
gdbio_break_cond(const gchar * number, const gchar * expr)
{
	gdbio_send_seq_cmd(edited_break, "-break-condition %s %s\n", number, expr);
}
