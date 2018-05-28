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

#include "cmds/edit.h"
#include "utils.h"


static void delete_char(CmdContext *c, CmdParams *p, gboolean yank)
{
	gint end_pos = NTH(p->sci, p->pos, p->num);
	end_pos = end_pos > p->line_end_pos ? p->line_end_pos : end_pos;
	if (yank)
	{
		c->line_copy = FALSE;
		SSM(p->sci, SCI_COPYRANGE, p->pos, end_pos);
	}
	SSM(p->sci, SCI_DELETERANGE, p->pos, end_pos - p->pos);
}


void cmd_delete_char(CmdContext *c, CmdParams *p)
{
	delete_char(c, p, FALSE);
}


void cmd_delete_char_copy(CmdContext *c, CmdParams *p)
{
	delete_char(c, p, TRUE);
}


static void delete_char_back(CmdContext *c, CmdParams *p, gboolean yank)
{
	gint start_pos = NTH(p->sci, p->pos, -p->num);
	start_pos = start_pos < p->line_start_pos ? p->line_start_pos : start_pos;
	if (yank)
	{
		c->line_copy = FALSE;
		SSM(p->sci, SCI_COPYRANGE, start_pos, p->pos);
	}
	SSM(p->sci, SCI_DELETERANGE, start_pos, p->pos - start_pos);
}


void cmd_delete_char_back(CmdContext *c, CmdParams *p)
{
	delete_char_back(c, p, FALSE);
}


void cmd_delete_char_back_copy(CmdContext *c, CmdParams *p)
{
	delete_char_back(c, p, TRUE);
}


void cmd_clear_right(CmdContext *c, CmdParams *p)
{
	gint new_line = get_line_number_rel(p->sci, p->num - 1);
	gint pos = SSM(p->sci, SCI_GETLINEENDPOSITION, new_line, 0);

	c->line_copy = FALSE;
	SSM(p->sci, SCI_COPYRANGE, p->pos, pos);
	SSM(p->sci, SCI_DELETERANGE, p->pos, pos - p->pos);
}


void cmd_delete_line(CmdContext *c, CmdParams *p)
{
	gint num = get_line_number_rel(p->sci, p->num);
	gint end = SSM(p->sci, SCI_POSITIONFROMLINE, num, 0);

	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, p->line_start_pos, end);
	SSM(p->sci, SCI_DELETERANGE, p->line_start_pos, end - p->line_start_pos);
}


void cmd_copy_line(CmdContext *c, CmdParams *p)
{
	gint num = get_line_number_rel(p->sci, p->num);
	gint end = SSM(p->sci, SCI_POSITIONFROMLINE, num, 0);

	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, p->line_start_pos, end);
}


void cmd_newline(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_NEWLINE, 0, 0);
}


void cmd_tab(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_TAB, 0, 0);
}


void cmd_del_word_left(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_DELWORDLEFT, 0, 0);
}


void cmd_undo(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		if (!SSM(p->sci, SCI_CANUNDO, 0, 0))
			break;
		SSM(p->sci, SCI_UNDO, 0, 0);
	}
}


void cmd_redo(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		if (!SSM(p->sci, SCI_CANREDO, 0, 0))
			break;
		SSM(p->sci, SCI_REDO, 0, 0);
	}
}


static void paste(CmdContext *c, CmdParams *p, gboolean after)
{
	gint pos;
	gint i;

	if (c->line_copy)
	{
		if (after)
			pos = SSM(p->sci, SCI_POSITIONFROMLINE, p->line+1, 0);
		else
			pos = p->line_start_pos;
	}
	else
	{
		pos = p->pos;
		if (after && pos < p->line_end_pos)
			pos = NEXT(p->sci, pos);
	}

	SET_POS(p->sci, pos, TRUE);
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_PASTE, 0, 0);
	if (c->line_copy)
		SET_POS(p->sci, pos, TRUE);
	else if (!VI_IS_INSERT(vi_get_mode()))
		SSM(p->sci, SCI_CHARLEFT, 0, 0);
}


void cmd_paste_after(CmdContext *c, CmdParams *p)
{
	paste(c, p, TRUE);
}


void cmd_paste_before(CmdContext *c, CmdParams *p)
{
	paste(c, p, FALSE);
}


static void join_lines(CmdContext *c, CmdParams *p, gint line, gint num)
{
	for (int i = 0; i < num; i++)
	{
		gint line_start_pos = SSM(p->sci, SCI_POSITIONFROMLINE, line, 0);
		gint line_end_pos = SSM(p->sci, SCI_GETLINEENDPOSITION, line, 0);
		gint next_line_end_pos = SSM(p->sci, SCI_GETLINEENDPOSITION, line+1, 0);
		gint pos = line_end_pos;
		gint pos_start;

		while (g_ascii_isspace(SSM(p->sci, SCI_GETCHARAT, pos, 0)) && pos > line_start_pos)
			pos = PREV(p->sci, pos);
		if (!g_ascii_isspace(SSM(p->sci, SCI_GETCHARAT, pos, 0)))
			pos = NEXT(p->sci, pos);
		pos_start = pos;

		pos = line_end_pos;
		while (g_ascii_isspace(SSM(p->sci, SCI_GETCHARAT, pos, 0))  && pos < next_line_end_pos)
			pos = NEXT(p->sci, pos);

		SSM(p->sci, SCI_DELETERANGE, pos_start, pos - pos_start);
		SSM(p->sci, SCI_INSERTTEXT, pos_start, (sptr_t)" ");
	}
}


void cmd_join_lines(CmdContext *c, CmdParams *p)
{
	gint num = p->num;
	if (p->num_present && num > 1)
		num--;
	join_lines(c, p, p->line, num);
}


void cmd_join_lines_sel(CmdContext *c, CmdParams *p)
{
	join_lines(c, p, p->sel_first_line, p->sel_last_line - p->sel_first_line);
	vi_set_mode(VI_MODE_COMMAND);
}


static void replace_char(ScintillaObject *sci, gint pos, gint num, gint line,
	gboolean force_upper, gboolean force_lower, gunichar repl_char)
{
	gint i;
	gint max_num;
	gint last_pos;
	struct Sci_TextRange tr;
	gchar *original, *replacement, *repl, *orig;

	max_num = DIFF(sci, pos, SSM(sci, SCI_GETLINEENDPOSITION, line, 0));
	if (line != -1 && num > max_num)
		num = max_num;

	max_num = DIFF(sci, pos, SSM(sci, SCI_GETLENGTH, 0, 0));
	if (num > max_num)
		num = max_num;

	if (num <= 0)
		return;

	last_pos = NTH(sci, pos, num);
	original = g_malloc(last_pos - pos + 1);
	replacement = g_malloc(6 * num + 1);
	repl = replacement;
	orig = original;

	tr.chrg.cpMin = pos;
	tr.chrg.cpMax = last_pos;
	tr.lpstrText = orig;
	SSM(sci, SCI_GETTEXTRANGE, 0, (sptr_t)&tr);

	for (i = 0; i < num; i++)
	{
		gunichar c = g_utf8_get_char(orig);

		if (repl_char == 0)
		{
			if ((force_upper || g_unichar_islower(c)) && !force_lower)
				c = g_unichar_toupper(c);
			else if (force_lower || g_unichar_isupper(c))
				c = g_unichar_tolower(c);
		}
		else
		{
			GUnicodeBreakType t = g_unichar_break_type(c);
			if (t != G_UNICODE_BREAK_CARRIAGE_RETURN  &&
				t != G_UNICODE_BREAK_LINE_FEED)
				c = repl_char;
		}

		repl += g_unichar_to_utf8(c, repl);
		orig = g_utf8_find_next_char(orig, NULL);

	}
	*repl = '\0';

	SSM(sci, SCI_SETTARGETRANGE, pos, last_pos);
	SSM(sci, SCI_REPLACETARGET, -1, (sptr_t)replacement);

	if (line != -1)
		SET_POS(sci, last_pos, TRUE);

	g_free(original);
	g_free(replacement);
}


void cmd_replace_char(CmdContext *c, CmdParams *p)
{
	gunichar repl = gdk_keyval_to_unicode(p->last_kp->key);
	replace_char(p->sci, p->pos, p->num, p->line, FALSE, FALSE, repl);
}


void cmd_replace_char_sel(CmdContext *c, CmdParams *p)
{
	gint num = DIFF(p->sci, p->sel_start, p->sel_start + p->sel_len);
	gunichar repl = gdk_keyval_to_unicode(p->last_kp->key);
	replace_char(p->sci, p->sel_start, num, -1, FALSE, FALSE, repl);
	vi_set_mode(VI_MODE_COMMAND);
}


static void switch_case(CmdContext *c, CmdParams *p,
	gboolean force_upper, gboolean force_lower)
{
	if (VI_IS_VISUAL(vi_get_mode()) || p->sel_len > 0)
	{
		gint num = DIFF(p->sci, p->sel_start, p->sel_start + p->sel_len);
		replace_char(p->sci, p->sel_start, num, -1, force_upper, force_lower, 0);
		vi_set_mode(VI_MODE_COMMAND);
	}
	else
		replace_char(p->sci, p->pos, p->num, p->line, force_upper, force_lower, 0);
}


void cmd_switch_case(CmdContext *c, CmdParams *p)
{
	switch_case(c, p, FALSE, FALSE);
}


void cmd_lower_case(CmdContext *c, CmdParams *p)
{
	switch_case(c, p, FALSE, TRUE);
}


void cmd_upper_case(CmdContext *c, CmdParams *p)
{
	switch_case(c, p, TRUE, FALSE);
}


static void indent(ScintillaObject *sci, gboolean unindent, gint pos, gint num, gint indent_num)
{
	gint i;
	gint line_start = SSM(sci, SCI_LINEFROMPOSITION, pos, 0);
	gint line_count = SSM(sci, SCI_GETLINECOUNT, 0, 0);
	gint line_end = line_start + num > line_count ? line_count : line_start + num;
	gint end_pos = SSM(sci, SCI_POSITIONFROMLINE, line_end, 0);

	SSM(sci, SCI_HOME, 0, 0);
	SSM(sci, SCI_SETSEL, end_pos, pos);
	for (i = 0; i < indent_num; i++)
		SSM(sci, unindent ? SCI_BACKTAB : SCI_TAB, 0, 0);
	goto_nonempty(sci, line_start, TRUE);
}


void cmd_indent(CmdContext *c, CmdParams *p)
{
	indent(p->sci, FALSE, p->pos, p->num, 1);
}


void cmd_unindent(CmdContext *c, CmdParams *p)
{
	indent(p->sci, TRUE, p->pos, p->num, 1);
}


void cmd_indent_sel(CmdContext *c, CmdParams *p)
{
	indent(p->sci, FALSE, p->sel_start, p->sel_last_line - p->sel_first_line + 1, p->num);
	vi_set_mode(VI_MODE_COMMAND);
}


void cmd_unindent_sel(CmdContext *c, CmdParams *p)
{
	indent(p->sci, TRUE, p->sel_start, p->sel_last_line - p->sel_first_line + 1, p->num);
	vi_set_mode(VI_MODE_COMMAND);
}


static void indent_ins(CmdContext *c, CmdParams *p, gboolean indent)
{
	gint delta;
	SSM(p->sci, SCI_HOME, 0, 0);
	SSM(p->sci, indent ? SCI_TAB : SCI_BACKTAB, 0, 0);
	delta = SSM(p->sci, SCI_GETLINEENDPOSITION, p->line, 0) - p->line_end_pos;
	SET_POS(p->sci, p->pos + delta, TRUE);
}


void cmd_indent_ins(CmdContext *c, CmdParams *p)
{
	indent_ins(c, p, TRUE);
}

void cmd_unindent_ins(CmdContext *c, CmdParams *p)
{
	indent_ins(c, p, FALSE);
}


static void copy_char(CmdContext *c, CmdParams *p, gboolean above)
{
	if ((above && p->line > 0) || (!above && p->line < p->line_num - 1))
	{
		gint line = above ? p->line - 1 : p->line + 1;
		gint col = SSM(p->sci, SCI_GETCOLUMN, p->pos, 0);
		gint pos = SSM(p->sci, SCI_FINDCOLUMN, line, col);
		gint line_end = SSM(p->sci, SCI_GETLINEENDPOSITION, line, 0);

		if (pos < line_end)
		{
			gchar txt[MAX_CHAR_SIZE];
			struct Sci_TextRange tr;

			tr.chrg.cpMin = pos;
			tr.chrg.cpMax = NEXT(p->sci, pos);
			tr.lpstrText = txt;

			SSM(p->sci, SCI_GETTEXTRANGE, 0, (sptr_t)&tr);
			SSM(p->sci, SCI_INSERTTEXT, p->pos, (sptr_t)txt);
			SET_POS(p->sci, NEXT(p->sci, p->pos), TRUE);
		}
	}
}


void cmd_copy_char_from_above(CmdContext *c, CmdParams *p)
{
	copy_char(c, p, TRUE);
}


void cmd_copy_char_from_below(CmdContext *c, CmdParams *p)
{
	copy_char(c, p, FALSE);
}


void cmd_repeat_subst(CmdContext *c, CmdParams *p)
{
	perform_substitute(p->sci, c->substitute_text, p->line, p->line, "");
}
