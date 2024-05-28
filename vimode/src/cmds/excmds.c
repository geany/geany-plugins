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

#include "cmds/excmds.h"
#include "cmds/edit.h"
#include "cmds/undo.h"
#include "utils.h"

void excmd_save(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_save(p->force);
}


void excmd_save_all(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_save_all(p->force);
}


void excmd_quit(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_quit(p->force);
}


void excmd_save_quit(CmdContext *c, ExCmdParams *p)
{
	if (c->cb->on_save(p->force))
		c->cb->on_quit(p->force);
}


void excmd_save_all_quit(CmdContext *c, ExCmdParams *p)
{
	if (c->cb->on_save_all(p->force))
		c->cb->on_quit(p->force);
}


void excmd_repeat_subst(CmdContext *c, ExCmdParams *p)
{
	const gchar *flags = p->param1;
	if (!flags)
		flags = "g";
	perform_substitute(c->sci, c->substitute_text, p->range_from, p->range_to, flags);
}


void excmd_repeat_subst_orig_flags(CmdContext *c, ExCmdParams *p)
{
	perform_substitute(c->sci, c->substitute_text, p->range_from, p->range_to, NULL);
}


static void prepare_cmd_params(CmdParams *params, CmdContext *c, ExCmdParams *p)
{
	gint start = SSM(c->sci, SCI_POSITIONFROMLINE, p->range_from, 0);
	SET_POS(c->sci, start, TRUE);
	cmd_params_init(params, c->sci, p->range_to - p->range_from + 1, FALSE, NULL, FALSE, 0, 0);
}


void excmd_yank(CmdContext *c, ExCmdParams *p)
{
	CmdParams params;
	prepare_cmd_params(&params, c, p);
	cmd_copy_line(c, &params);
}


void excmd_put(CmdContext *c, ExCmdParams *p)
{
	CmdParams params;
	prepare_cmd_params(&params, c, p);
	cmd_paste_after(c, &params);
}


void excmd_undo(CmdContext *c, ExCmdParams *p)
{
	undo_apply(c, 1);
}


void excmd_redo(CmdContext *c, ExCmdParams *p)
{
	SSM(c->sci, SCI_REDO, 0, 0);
}


void excmd_shift_left(CmdContext *c, ExCmdParams *p)
{
	CmdParams params;
	prepare_cmd_params(&params, c, p);
	cmd_unindent(c, &params);
}


void excmd_shift_right(CmdContext *c, ExCmdParams *p)
{
	CmdParams params;
	prepare_cmd_params(&params, c, p);
	cmd_indent(c, &params);
}


void excmd_delete(CmdContext *c, ExCmdParams *p)
{
	CmdParams params;
	prepare_cmd_params(&params, c, p);
	cmd_delete_line(c, &params);
}

void excmd_join(CmdContext *c, ExCmdParams *p)
{
	CmdParams params;
	prepare_cmd_params(&params, c, p);
	cmd_join_lines(c, &params);
}


void excmd_copy(CmdContext *c, ExCmdParams *p)
{
	CmdParams params;
	gint dest = SSM(c->sci, SCI_POSITIONFROMLINE, p->dest, 0);
	excmd_yank(c, p);
	SET_POS(c->sci, dest, TRUE);
	cmd_params_init(&params, c->sci, 1, FALSE, NULL, FALSE, 0, 0);
	cmd_paste_after(c, &params);
}


void excmd_move(CmdContext *c, ExCmdParams *p)
{
	CmdParams params;
	gint dest;

	if (p->dest >= p->range_from && p->dest <= p->range_to)
		return;

	excmd_delete(c, p);
	if (p->dest > p->range_to)
		p->dest -= p->range_to - p->range_from + 1;
	dest = SSM(c->sci, SCI_POSITIONFROMLINE, p->dest, 0);
	SET_POS(c->sci, dest, TRUE);
	cmd_params_init(&params, c->sci, 1, FALSE, NULL, FALSE, 0, 0);
	cmd_paste_after(c, &params);
}
