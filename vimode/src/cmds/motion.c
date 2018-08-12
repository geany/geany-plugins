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


void cmd_goto_up(CmdContext *c, CmdParams *p)
{
	gint one_above, pos;

	if (p->line == 0)
		return;

	/* Calling SCI_LINEUP/SCI_LINEDOWN in a loop for num lines leads to visible
	 * slow scrolling. On the other hand, SCI_LINEUP preserves the value of
	 * SCI_CHOOSECARETX which we cannot read directly from Scintilla and which
	 * we want to keep - perform jump to previous/following line and add
	 * one final SCI_LINEUP/SCI_LINEDOWN which recovers SCI_CHOOSECARETX for us. */
	one_above = p->line - p->num - 1;
	if (one_above >= 0)
	{
		/* Every case except for the first line - go one line above and perform
		 * SCI_LINEDOWN. This ensures that even with wrapping on, we get the
		 * caret on the first line of the wrapped line */
		pos = SSM(p->sci, SCI_GETLINEENDPOSITION, one_above, 0);
		SET_POS_NOX(p->sci, pos, FALSE);
		SSM(p->sci, SCI_LINEDOWN, 0, 0);
	}
	else
	{
		/* This is the first line and there is no line above - we need to go to
		 * the following line and do SCI_LINEUP. In addition, when wrapping is
		 * on, we need to repeat SCI_LINEUP to get to the first line of wrapping.
		 * This may lead to visible slow scrolling which is why there's the
		 * fast case above for anything else but the first line. */
		gint one_below = p->line - p->num + 1;
		gint wrap_count;

		one_below = one_below > 0 ? one_below : 1;
		pos = SSM(p->sci, SCI_POSITIONFROMLINE, one_below, 0);
		SET_POS_NOX(p->sci, pos, FALSE);
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
	gint one_above, pos;
	gint last_line = p->line_num - 1;

	if (p->line == last_line)
		return;

	/* see cmd_goto_up() for explanation */
	one_above = p->line + num - 1;
	one_above = one_above < last_line ? one_above : last_line - 1;
	pos = SSM(p->sci, SCI_GETLINEENDPOSITION, one_above, 0);
	SET_POS_NOX(p->sci, pos, FALSE);
	SSM(p->sci, SCI_LINEDOWN, 0, 0);
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
	gint new_line = get_line_number_rel(p->sci, -shift);
	goto_nonempty(p->sci, new_line, TRUE);
}


void cmd_goto_page_down(CmdContext *c, CmdParams *p)
{
	gint shift = p->line_visible_num * p->num;
	gint new_line = get_line_number_rel(p->sci, shift);
	goto_nonempty(p->sci, new_line, TRUE);
}


void cmd_goto_halfpage_up(CmdContext *c, CmdParams *p)
{
	gint shift = p->num_present ? p->num : p->line_visible_num / 2;
	gint new_line = get_line_number_rel(p->sci, -shift);
	goto_nonempty(p->sci, new_line, TRUE);
}


void cmd_goto_halfpage_down(CmdContext *c, CmdParams *p)
{
	gint shift = p->num_present ? p->num : p->line_visible_num / 2;
	gint new_line = get_line_number_rel(p->sci, shift);
	goto_nonempty(p->sci, new_line, TRUE);
}


void cmd_goto_line(CmdContext *c, CmdParams *p)
{
	gint num = p->num > p->line_num ? p->line_num : p->num;
	goto_nonempty(p->sci, num - 1, TRUE);
}


void cmd_goto_line_last(CmdContext *c, CmdParams *p)
{
	gint num = p->num > p->line_num ? p->line_num : p->num;
	if (!p->num_present)
		num = p->line_num;
	goto_nonempty(p->sci, num - 1, TRUE);
}


void cmd_goto_screen_top(CmdContext *c, CmdParams *p)
{
	gint top = p->line_visible_first;
	gint count = p->line_visible_num;
	gint line = top + p->num;
	goto_nonempty(p->sci, line > top + count ? top + count : line, FALSE);
}


void cmd_goto_screen_middle(CmdContext *c, CmdParams *p)
{
	goto_nonempty(p->sci, p->line_visible_first + p->line_visible_num/2, FALSE);
}


void cmd_goto_screen_bottom(CmdContext *c, CmdParams *p)
{
	gint top = p->line_visible_first;
	gint count = p->line_visible_num;
	gint line = top + count - p->num;
	goto_nonempty(p->sci, line < top ? top : line, FALSE);
}


void cmd_goto_doc_percentage(CmdContext *c, CmdParams *p)
{
	if (p->num > 100)
		p->num = 100;

	goto_nonempty(p->sci, (p->line_num * p->num) / 100, TRUE);
}


static void goto_word_end(CmdContext *c, CmdParams *p, gboolean forward)
{
	gint i;

	if (VI_IS_COMMAND(vi_get_mode()))
		SSM(p->sci, SCI_CHARRIGHT, 0, 0);

	for (i = 0; i < p->num; i++)
	{
		gint orig_pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
		gint pos;
		gint line_start_pos;

		SSM(p->sci, forward ? SCI_WORDRIGHTEND : SCI_WORDLEFTEND, 0, 0);
		line_start_pos = SSM(p->sci, SCI_POSITIONFROMLINE, GET_CUR_LINE(p->sci), 0);
		pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
		if (pos == orig_pos)
			break;
		/* For some reason, Scintilla treats newlines as word parts and cursor
		 * is left before/after newline. Repeat again in this case. */
		if (pos == line_start_pos)
			SSM(p->sci, forward ? SCI_WORDRIGHTEND : SCI_WORDLEFTEND, 0, 0);
	}

	if (VI_IS_COMMAND(vi_get_mode()))
		SSM(p->sci, SCI_CHARLEFT, 0, 0);
}


static void goto_word_start(CmdContext *c, CmdParams *p, gboolean forward)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		gint orig_pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
		gint pos;
		gint line_end_pos;

		SSM(p->sci, forward ? SCI_WORDRIGHT : SCI_WORDLEFT, 0, 0);
		line_end_pos = SSM(p->sci, SCI_GETLINEENDPOSITION, GET_CUR_LINE(p->sci), 0);
		pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
		if (pos == orig_pos)
			break;
		/* For some reason, Scintilla treats newlines as word parts and cursor
		 * is left before/after newline. Repeat again in this case. */
		if (pos == line_end_pos)
			SSM(p->sci, forward ? SCI_WORDRIGHT : SCI_WORDLEFT, 0, 0);
	}
}


void cmd_goto_next_word(CmdContext *c, CmdParams *p)
{
	goto_word_start(c, p, TRUE);
}


void cmd_goto_previous_word(CmdContext *c, CmdParams *p)
{
	goto_word_start(c, p, FALSE);
}


void cmd_goto_next_word_end(CmdContext *c, CmdParams *p)
{
	goto_word_end(c, p, TRUE);
}


void cmd_goto_previous_word_end(CmdContext *c, CmdParams *p)
{
	goto_word_end(c, p, FALSE);
}


static void use_all_wordchars(ScintillaObject *sci)
{
	guchar wc[512];
	gint i;
	gint j = 0;
	for (i = 0; i < 256; i++)
	{
		if (g_ascii_isprint(i) && !g_ascii_isspace(i))
			wc[j++] = i;
	}
	wc[j] = '\0';
	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t)wc);
}


void cmd_goto_next_word_space(CmdContext *c, CmdParams *p)
{
	guchar wc[512];
	SSM(p->sci, SCI_GETWORDCHARS, 0, (sptr_t)wc);
	use_all_wordchars(p->sci);

	goto_word_start(c, p, TRUE);

	SSM(p->sci, SCI_SETWORDCHARS, 0, (sptr_t)wc);
}


void cmd_goto_previous_word_space(CmdContext *c, CmdParams *p)
{
	guchar wc[512];
	SSM(p->sci, SCI_GETWORDCHARS, 0, (sptr_t)wc);
	use_all_wordchars(p->sci);

	goto_word_start(c, p, FALSE);

	SSM(p->sci, SCI_SETWORDCHARS, 0, (sptr_t)wc);
}


void cmd_goto_next_word_end_space(CmdContext *c, CmdParams *p)
{
	guchar wc[512];
	SSM(p->sci, SCI_GETWORDCHARS, 0, (sptr_t)wc);
	use_all_wordchars(p->sci);

	goto_word_end(c, p, TRUE);

	SSM(p->sci, SCI_SETWORDCHARS, 0, (sptr_t)wc);
}


void cmd_goto_previous_word_end_space(CmdContext *c, CmdParams *p)
{
	guchar wc[512];
	SSM(p->sci, SCI_GETWORDCHARS, 0, (sptr_t)wc);
	use_all_wordchars(p->sci);

	goto_word_end(c, p, FALSE);

	SSM(p->sci, SCI_SETWORDCHARS, 0, (sptr_t)wc);
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
