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

#include "cmds/changemode.h"
#include "utils.h"


void cmd_enter_ex(CmdContext *c, CmdParams *p)
{
	c->num = p->num;
	vi_enter_ex_mode();
}


void cmd_enter_command(CmdContext *c, CmdParams *p)
{
	if (SSM(p->sci, SCI_AUTOCACTIVE, 0, 0) || SSM(p->sci, SCI_CALLTIPACTIVE, 0, 0))
		SSM(p->sci, SCI_CANCEL, 0, 0);
	else
		vi_set_mode(VI_MODE_COMMAND);
}


void cmd_enter_command_single(CmdContext *c, CmdParams *p)
{
	vi_set_mode(VI_MODE_COMMAND_SINGLE);
}


void cmd_enter_visual(CmdContext *c, CmdParams *p)
{
	if (vi_get_mode() == VI_MODE_VISUAL)
	{
		SSM(p->sci, SCI_SETEMPTYSELECTION, p->pos, 0);
		vi_set_mode(VI_MODE_COMMAND);
	}
	else
		vi_set_mode(VI_MODE_VISUAL);
}


void cmd_enter_visual_line(CmdContext *c, CmdParams *p)
{
	if (vi_get_mode() == VI_MODE_VISUAL_LINE)
	{
		SSM(p->sci, SCI_SETEMPTYSELECTION, p->pos, 0);
		vi_set_mode(VI_MODE_COMMAND);
	}
	else
	{
		vi_set_mode(VI_MODE_VISUAL_LINE);
		/* just to force the scintilla notification callback to be called so we can
		 * select the current line */
		SSM(p->sci, SCI_LINEEND, 0, 0);
	}
}


void cmd_enter_insert(CmdContext *c, CmdParams *p)
{
	c->num = p->num;
	c->newline_insert = FALSE;
	vi_set_mode(VI_MODE_INSERT);
}


void cmd_enter_replace(CmdContext *c, CmdParams *p)
{
	c->num = p->num;
	c->newline_insert = FALSE;
	vi_set_mode(VI_MODE_REPLACE);
}


void cmd_enter_insert_after(CmdContext *c, CmdParams *p)
{
	cmd_enter_insert(c, p);
	if (p->pos < p->line_end_pos)
		SSM(p->sci, SCI_CHARRIGHT, 0, 0);
}


void cmd_enter_insert_line_start_nonempty(CmdContext *c, CmdParams *p)
{
	goto_nonempty(p->sci, p->line, TRUE);
	cmd_enter_insert(c, p);
}


void cmd_enter_insert_line_start(CmdContext *c, CmdParams *p)
{
	SET_POS(p->sci, p->line_start_pos, TRUE);
	cmd_enter_insert(c, p);
}


void cmd_enter_insert_line_end(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_LINEEND, 0, 0);
	cmd_enter_insert(c, p);
}


void cmd_enter_insert_next_line(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_LINEEND, 0, 0);
	SSM(p->sci, SCI_NEWLINE, 0, 0);
	SSM(p->sci, SCI_DELLINELEFT, 0, 0);
	c->num = p->num;
	c->newline_insert = TRUE;
	vi_set_mode(VI_MODE_INSERT);
}


void cmd_enter_insert_prev_line(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_HOME, 0, 0);
	SSM(p->sci, SCI_NEWLINE, 0, 0);
	SSM(p->sci, SCI_LINEUP, 0, 0);
	c->num = p->num;
	c->newline_insert = TRUE;
	vi_set_mode(VI_MODE_INSERT);
}


static void cut_range_change_mode(CmdContext *c, CmdParams *p,
	gint start, gint end, gboolean line_copy, ViMode mode)
{
	c->line_copy = line_copy;
	SSM(p->sci, SCI_COPYRANGE, start, end);
	SSM(p->sci, SCI_DELETERANGE, start, end - start);
	if (mode == VI_MODE_INSERT)
		cmd_enter_insert(c, p);
	else
		vi_set_mode(mode);
}


void cmd_enter_insert_clear_right(CmdContext *c, CmdParams *p)
{
	gint new_line = get_line_number_rel(p->sci, p->num - 1);
	gint pos = SSM(p->sci, SCI_GETLINEENDPOSITION, new_line, 0);

	cut_range_change_mode(c, p, p->pos, pos, FALSE, VI_MODE_INSERT);
}


void cmd_enter_insert_delete_char(CmdContext *c, CmdParams *p)
{
	gint end = NTH(p->sci, p->pos, p->num);
	end = end > p->line_end_pos ? p->line_end_pos : end;

	cut_range_change_mode(c, p, p->pos, end, FALSE, VI_MODE_INSERT);
}


void cmd_enter_insert_cut_line(CmdContext *c, CmdParams *p)
{
	gint new_line = get_line_number_rel(p->sci, p->num - 1);
	gint pos_end = SSM(p->sci, SCI_GETLINEENDPOSITION, new_line, 0);

	cut_range_change_mode(c, p, p->line_start_pos, pos_end, TRUE, VI_MODE_INSERT);
}


void cmd_enter_insert_cut_sel(CmdContext *c, CmdParams *p)
{
	gint sel_end_pos = p->sel_start + p->sel_len;
	if (p->is_operator_cmd && p->line_end_pos < sel_end_pos)
		sel_end_pos = p->line_end_pos;

	cut_range_change_mode(c, p, p->sel_start, sel_end_pos, FALSE, VI_MODE_INSERT);
}


void cmd_enter_insert_cut_line_sel(CmdContext *c, CmdParams *p)
{
	gint begin = p->sel_first_line_begin_pos;
	gint end = p->sel_last_line_end_pos;

	cut_range_change_mode(c, p, begin, end, TRUE, VI_MODE_INSERT);
}


void cmd_enter_command_cut_sel(CmdContext *c, CmdParams *p)
{
	gint sel_end_pos = p->sel_start + p->sel_len;
	if (p->is_operator_cmd && p->line_end_pos < sel_end_pos)
		sel_end_pos = p->line_end_pos;

	cut_range_change_mode(c, p, p->sel_start, sel_end_pos, FALSE, VI_MODE_COMMAND);
}


void cmd_enter_command_cut_line_sel(CmdContext *c, CmdParams *p)
{
	gint begin = p->sel_first_line_begin_pos;
	gint end = p->sel_last_line_end_pos;

	cut_range_change_mode(c, p, begin, end, TRUE, VI_MODE_COMMAND);
	SET_POS(p->sci, begin, TRUE);
}


void cmd_enter_command_copy_sel(CmdContext *c, CmdParams *p)
{
	gint sel_end_pos = p->sel_start + p->sel_len;
	if (p->is_operator_cmd && p->line_end_pos < sel_end_pos)
		sel_end_pos = p->line_end_pos;

	c->line_copy = FALSE;
	SSM(p->sci, SCI_COPYRANGE, p->sel_start, sel_end_pos);
	vi_set_mode(VI_MODE_COMMAND);
	SET_POS(p->sci, p->sel_start, TRUE);
}


void cmd_enter_command_copy_line_sel(CmdContext *c, CmdParams *p)
{
	gint begin = p->sel_first_line_begin_pos;
	gint end = p->sel_last_line_end_pos;

	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, begin, end);
	vi_set_mode(VI_MODE_COMMAND);
	SET_POS(p->sci, begin, TRUE);
}
