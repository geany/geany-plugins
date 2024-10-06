/*
 * Copyright 2018 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "cmds/special.h"
#include "utils.h"

#include <gdk/gdkkeysyms.h>


void cmd_swap_anchor(CmdContext *c, CmdParams *p)
{
	gint anchor = c->sel_anchor;
	c->sel_anchor = p->pos;
	SET_POS(p->sci, anchor, TRUE);
}


void cmd_nop(CmdContext *c, CmdParams *p)
{
	// do nothing
}


void cmd_repeat_last_command(CmdContext *c, CmdParams *p)
{
	// handled in a special way - has to be separate from cmd_nop()
}


void cmd_paste_inserted_text(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_ADDTEXT, c->insert_buf_len, (sptr_t) c->insert_buf);
}


void cmd_paste_inserted_text_leave_ins(CmdContext *c, CmdParams *p)
{
	cmd_paste_inserted_text(c, p);
	vi_set_mode(VI_MODE_COMMAND);
}


void cmd_write_exit(CmdContext *c, CmdParams *p)
{
	if (c->cb->on_save(FALSE))
		c->cb->on_quit(FALSE);
}


void cmd_force_exit(CmdContext *c, CmdParams *p)
{
	c->cb->on_quit(TRUE);
}


void cmd_search_next(CmdContext *c, CmdParams *p)
{
	gboolean invert = FALSE;
	gint pos;

	if (p->last_kp->key == GDK_KEY_N)
		invert = TRUE;

	pos = perform_search(p->sci, c->search_text, p->num, invert);
	if (pos >= 0)
		SET_POS(c->sci, pos, TRUE);

}


static void search_current(CmdContext *c, CmdParams *p, gboolean next)
{
	gchar *word = get_current_word(p->sci);
	gint pos;

	g_free(c->search_text);
	if (!word)
		c->search_text = NULL;
	else
	{
		const gchar *prefix = next ? "/" : "?";
		c->search_text = g_strconcat(prefix, "\\<", word, "\\>", NULL);
	}
	g_free(word);

	pos = perform_search(p->sci, c->search_text, p->num, FALSE);
	if (pos >= 0)
		SET_POS(c->sci, pos, TRUE);

}


void cmd_search_current_next(CmdContext *c, CmdParams *p)
{
	search_current(c, p, TRUE);
}


void cmd_search_current_prev(CmdContext *c, CmdParams *p)
{
	search_current(c, p, FALSE);
}
