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

#include "cmds/motion.h"
#include "utils.h"


void cmd_goto_left(CmdContext *c, CmdParams *p)
{
	gint i;
	gint start_pos = p->line_start_pos;
	gint pos = p->pos;
	for (i = 0; i < p->num && pos > start_pos; i++)
		pos = PREV(p->sci, pos);
	SET_POS(p->sci, pos, TRUE);
}


void cmd_goto_right(CmdContext *c, CmdParams *p)
{
	gint i;
	gint pos = p->pos;
	for (i = 0; i < p->num && pos < p->line_end_pos; i++)
		pos = NEXT(p->sci, pos);
	SET_POS(p->sci, pos, TRUE);
}

static gint doc_line_from_visible_delta(CmdParams *p, gint line, gint delta, gint *previous)
{
	gint step = delta < 0 ? -1 : 1;
	gint new_line = p->line;
	gint left = abs(delta);
	gint prev_line = -1, tmp;

	if ((step > 0 && new_line < p->line_num - 1) || (step < 0 && new_line > 0)) {
		while (left > 0) {
			tmp = new_line;
			if (SSM(p->sci, SCI_GETLINEVISIBLE, new_line, 0)) {
				left--;
				prev_line = tmp;
			}
			new_line += step;
			if (new_line >= p->line_num || new_line <= 0) break;
		}
	}

	if (previous) *previous = prev_line;

	return new_line;
}

void cmd_goto_up(CmdContext *c, CmdParams *p)
{
	gint previous = -1;
	gint new_line;
	gint wrap_count;

	if (p->line == 0)
		return;

	new_line = doc_line_from_visible_delta(p, p->line, -p->num, &previous);

	/* Calling SCI_LINEUP/SCI_LINEDOWN in a loop for num lines leads to visible
	 * slow scrolling. On the other hand, SCI_LINEUP preserves the value of
	 * SCI_CHOOSECARETX which we cannot read directly from Scintilla and which
	 * we want to keep - perform jump to previous/following line and add
	 * one final SCI_LINEUP/SCI_LINEDOWN which recovers SCI_CHOOSECARETX for us. */

	if (previous > -1) {
		guint pos = SSM(p->sci, SCI_POSITIONFROMLINE, previous, 0); 
		SET_POS_NOX(p->sci, pos, FALSE);
	}

	if (new_line < p->line) {
		SSM(p->sci, SCI_LINEUP, 0, 0);

		wrap_count = SSM(p->sci, SCI_WRAPCOUNT, GET_CUR_LINE(p->sci), 0);
		while (wrap_count > 1)
		{
			SSM(p->sci, SCI_LINEUP, 0, 0);
			wrap_count--;
		}
	}
}


void cmd_goto_up_nonempty(CmdContext *c, CmdParams *p)
{
	cmd_goto_up(c, p);
	goto_nonempty(p->sci, GET_CUR_LINE(p->sci), TRUE);
}


static void goto_down(CmdParams *p, gint num)
{
	gint previous = -1;
	gint new_line;

	if (p->line >= p->line_num - 1)
		return;

	new_line = doc_line_from_visible_delta(p, p->line, num, &previous);

	if (previous > -1) {
		guint pos = SSM(p->sci, SCI_GETLINEENDPOSITION, previous, 0);
		SET_POS_NOX(p->sci, pos, FALSE);
	}

	if (new_line > p->line) SSM(p->sci, SCI_LINEDOWN, 0, 0);
}


void cmd_goto_down(CmdContext *c, CmdParams *p)
{
	goto_down(p, p->num);
}


void cmd_goto_down_nonempty(CmdContext *c, CmdParams *p)
{
	goto_down(p, p->num);
	goto_nonempty(p->sci, GET_CUR_LINE(p->sci), TRUE);
}


void cmd_goto_down_one_less_nonempty(CmdContext *c, CmdParams *p)
{
	if (p->num > 1)
		goto_down(p, p->num - 1);
	goto_nonempty(p->sci, GET_CUR_LINE(p->sci), TRUE);
}


void cmd_goto_page_up(CmdContext *c, CmdParams *p)
{
	gint shift = p->line_visible_num * p->num;
	gint new_line = doc_line_from_visible_delta(p, p->line, -shift, NULL);
	goto_nonempty(p->sci, new_line, TRUE);
}


void cmd_goto_page_down(CmdContext *c, CmdParams *p)
{
	gint shift = p->line_visible_num * p->num;
	gint new_line = doc_line_from_visible_delta(p, p->line, shift, NULL);
	goto_nonempty(p->sci, new_line, TRUE);
}


void cmd_goto_halfpage_up(CmdContext *c, CmdParams *p)
{
	gint shift = p->num_present ? p->num : p->line_visible_num / 2;
	gint new_line = doc_line_from_visible_delta(p, p->line, -shift, NULL);
	goto_nonempty(p->sci, new_line, TRUE);
}


void cmd_goto_halfpage_down(CmdContext *c, CmdParams *p)
{
	gint shift = p->num_present ? p->num : p->line_visible_num / 2;
	gint new_line = doc_line_from_visible_delta(p, p->line, shift, NULL);
	goto_nonempty(p->sci, new_line, TRUE);
}


void cmd_goto_line(CmdContext *c, CmdParams *p)
{
	gint num = p->num > p->line_num ? p->line_num : p->num;
	num = doc_line_from_visible_delta(p, num, -1, NULL);
	goto_nonempty(p->sci, num, TRUE);
}


void cmd_goto_line_last(CmdContext *c, CmdParams *p)
{
	gint num = p->num > p->line_num ? p->line_num : p->num;
	if (!p->num_present)
		num = p->line_num;
	num = doc_line_from_visible_delta(p, num, -1, NULL);
	goto_nonempty(p->sci, num, TRUE);
}


void cmd_goto_screen_top(CmdContext *c, CmdParams *p)
{
	gint line;
	gint top = p->line_visible_first;
	gint count = p->line_visible_num;
	gint max = doc_line_from_visible_delta(p, top, count, NULL);
	gint num = p->num;

	if (!p->num_present)
		num = 0;

	line = doc_line_from_visible_delta(p, top, num, NULL);
	goto_nonempty(p->sci, line > max ? max : line, FALSE);
}


void cmd_goto_screen_middle(CmdContext *c, CmdParams *p)
{
	gint num = doc_line_from_visible_delta(p, p->line_visible_first, p->line_visible_num / 2, NULL);
	goto_nonempty(p->sci, num, FALSE);
}


void cmd_goto_screen_bottom(CmdContext *c, CmdParams *p)
{
	gint top = p->line_visible_first;
	gint count = p->line_visible_num;
	gint line = doc_line_from_visible_delta(p, top, count - p->num, NULL);
	goto_nonempty(p->sci, line < top ? top : line, FALSE);
}


void cmd_goto_doc_percentage(CmdContext *c, CmdParams *p)
{
	if (p->num > 100)
		p->num = 100;

	goto_nonempty(p->sci, (p->line_num * p->num) / 100, TRUE);
}


void cmd_goto_line_start(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_HOME, 0, 0);
}


void cmd_goto_line_start_nonempty(CmdContext *c, CmdParams *p)
{
	goto_nonempty(p->sci, p->line, TRUE);
}


void cmd_goto_line_end(CmdContext *c, CmdParams *p)
{
	if (p->num > 1)
		goto_down(p, p->num - 1);
	SSM(p->sci, SCI_LINEEND, 0, 0);
}


void cmd_goto_column(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_FINDCOLUMN, p->line, p->num - 1);
	SET_POS(p->sci, pos, TRUE);
}


void cmd_goto_matching_brace(CmdContext *c, CmdParams *p)
{
	gint pos = p->pos;
	while (pos < p->line_end_pos)
	{
		gint matching_pos = SSM(p->sci, SCI_BRACEMATCH, pos, 0);
		if (matching_pos != -1)
		{
			SET_POS(p->sci, matching_pos, TRUE);
			return;
		}
		pos++;
	}
}


static void find_char(CmdContext *c, CmdParams *p, gboolean invert)
{
	struct Sci_TextToFind ttf;
	gboolean forward;
	gint pos = p->pos;
	gint i;

	if (!c->search_char)
		return;

	forward = c->search_char[0] == 'f' || c->search_char[0] == 't';
	forward = !forward != !invert;
	ttf.lpstrText = c->search_char + 1;

	for (i = 0; i < p->num; i++)
	{
		gint new_pos;

		if (forward)
		{
			ttf.chrg.cpMin = NEXT(p->sci, pos);
			ttf.chrg.cpMax = p->line_end_pos;
		}
		else
		{
			ttf.chrg.cpMin = pos;
			ttf.chrg.cpMax = p->line_start_pos;
		}

		new_pos = SSM(p->sci, SCI_FINDTEXT, 0, (sptr_t)&ttf);
		if (new_pos < 0)
			break;
		pos = new_pos;
	}

	if (pos >= 0)
	{
		if (c->search_char[0] == 't')
			pos = PREV(p->sci, pos);
		else if (c->search_char[0] == 'T')
			pos = NEXT(p->sci, pos);
		SET_POS(p->sci, pos, TRUE);
	}
}


void cmd_goto_next_char(CmdContext *c, CmdParams *p)
{
	g_free(c->search_char);
	c->search_char = g_strconcat("f", kp_to_str(p->last_kp), NULL);
	find_char(c, p, FALSE);
}


void cmd_goto_prev_char(CmdContext *c, CmdParams *p)
{
	g_free(c->search_char);
	c->search_char = g_strconcat("F", kp_to_str(p->last_kp), NULL);
	find_char(c, p, FALSE);
}


void cmd_goto_next_char_before(CmdContext *c, CmdParams *p)
{
	g_free(c->search_char);
	c->search_char = g_strconcat("t", kp_to_str(p->last_kp), NULL);
	find_char(c, p, FALSE);
}


void cmd_goto_prev_char_before(CmdContext *c, CmdParams *p)
{
	g_free(c->search_char);
	c->search_char = g_strconcat("T", kp_to_str(p->last_kp), NULL);
	find_char(c, p, FALSE);
}


void cmd_goto_char_repeat(CmdContext *c, CmdParams *p)
{
	find_char(c, p, FALSE);
}


void cmd_goto_char_repeat_opposite(CmdContext *c, CmdParams *p)
{
	find_char(c, p, TRUE);
}


void cmd_scroll_up(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_LINESCROLL, 0, -p->num);
}


void cmd_scroll_down(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_LINESCROLL, 0, p->num);
}


static void scroll_to_line(CmdParams *p, gint offset, gboolean nonempty)
{
	gint column = SSM(p->sci, SCI_GETCOLUMN, p->pos, 0);
	gint line = p->line;

	if (p->num_present)
		line = p->num - 1;
	if (nonempty)
		goto_nonempty(p->sci, line, FALSE);
	else
	{
		gint pos = SSM(p->sci, SCI_FINDCOLUMN, line, column);
		SET_POS_NOX(p->sci, pos, FALSE);
	}
	SSM(p->sci, SCI_SETFIRSTVISIBLELINE, line + offset, 0);
}


void cmd_scroll_center(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, - p->line_visible_num / 2, FALSE);
}


void cmd_scroll_top(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, 0, FALSE);
}


void cmd_scroll_bottom(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, - p->line_visible_num + 1, FALSE);
}


void cmd_scroll_center_nonempty(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, - p->line_visible_num / 2, TRUE);
}


void cmd_scroll_top_nonempty(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, 0, TRUE);
}


void cmd_scroll_top_next_nonempty(CmdContext *c, CmdParams *p)
{
	if (p->num_present)
		cmd_scroll_top_nonempty(c, p);
	else
	{
		gint line = p->line_visible_first + p->line_visible_num;
		goto_nonempty(p->sci, line, FALSE);
		SSM(p->sci, SCI_SETFIRSTVISIBLELINE, line, 0);
	}
}


void cmd_scroll_bottom_nonempty(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, - p->line_visible_num + 1, TRUE);
}
