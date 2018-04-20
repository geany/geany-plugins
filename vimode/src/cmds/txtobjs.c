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

#include "cmds/txtobjs.h"


static gint find_upper_level_brace(ScintillaObject *sci, gint pos, gint open_brace, gint close_brace)
{
	while (pos > 0)
	{
		pos = PREV(sci, pos);
		gchar c = SSM(sci, SCI_GETCHARAT, pos, 0);

		if (c == open_brace)
			return pos;
		else if (c == close_brace) 
		{
			pos = SSM(sci, SCI_BRACEMATCH, pos, 0);
			if (pos < 0)
				break;
		}
	}
	return -1;
}


static gint find_char(ScintillaObject *sci, gint pos, gint ch, gboolean forward)
{
	while (pos > 0)
	{
		gint last_pos = pos;
		gchar c;

		pos = forward ? NEXT(sci, pos) : PREV(sci, pos);
		c = SSM(sci, SCI_GETCHARAT, pos, 0);

		if (c == ch)
			return pos;
		if (pos == last_pos)
			break;
	}
	return -1;
}


static void select_brace(CmdContext *c, CmdParams *p, gint open_brace, gint close_brace, gboolean inner)
{
	gint pos = p->pos;
	gint start_pos = 0;
	gint end_pos = 0;
	gint i;

	for (i = 0; i < p->num; i++)
	{
		if (open_brace == close_brace)
		{
			start_pos = find_char(p->sci, pos, open_brace, FALSE);
			if (start_pos < 0)
				return;
			end_pos = find_char(p->sci, pos, close_brace, TRUE);
		}
		else
		{
			start_pos = find_upper_level_brace(p->sci, pos, open_brace, close_brace);
			if (start_pos < 0)
				return;
			end_pos = SSM(p->sci, SCI_BRACEMATCH, start_pos, 0);
		}

		if (end_pos < 0)
			return;

		pos = start_pos;
	}

	if (inner)
		start_pos = NEXT(p->sci, start_pos);
	else
		end_pos = NEXT(p->sci, end_pos);
	
	if (VI_IS_VISUAL(vi_get_mode()))
	{
		c->sel_anchor = start_pos;
		SET_POS(p->sci, end_pos, TRUE);
	}
	else
	{
		p->sel_start = start_pos;
		p->sel_len = end_pos - start_pos;
	}
}


void cmd_select_quotedbl(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '\"', '\"', FALSE);
}


void cmd_select_quoteleft(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '`', '`', FALSE);
}


void cmd_select_apostrophe(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '\'', '\'', FALSE);
}


void cmd_select_brace(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '{', '}', FALSE);
}


void cmd_select_paren(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '(', ')', FALSE);
}


void cmd_select_less(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '<', '>', FALSE);
}


void cmd_select_bracket(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '[', ']', FALSE);
}


void cmd_select_quotedbl_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '\"', '\"', TRUE);
}


void cmd_select_quoteleft_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '`', '`', TRUE);
}


void cmd_select_apostrophe_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '\'', '\'', TRUE);
}


void cmd_select_brace_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '{', '}', TRUE);
}


void cmd_select_paren_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '(', ')', TRUE);
}


void cmd_select_less_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '<', '>', TRUE);
}


void cmd_select_bracket_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '[', ']', TRUE);
}
