/*
 * Copyright 2019 Jiri Techet <techet@gmail.com>
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

#include "cmds/motion-word.h"

typedef gboolean (*CharacterPredicate)(gchar c);


static void move_left(ScintillaObject *sci, gchar *ch, gint *pos)
{
	*pos = PREV(sci, *pos);
	*ch = SSM(sci, SCI_GETCHARAT, *pos, 0);
}


static void move_right(ScintillaObject *sci, gchar *ch, gint *pos)
{
	*pos = NEXT(sci, *pos);
	*ch = SSM(sci, SCI_GETCHARAT, *pos, 0);
}


static gboolean is_wordchar(gchar c)
{
	return g_ascii_isalnum(c) || c == '_' || (c >= 192 && c <= 255);
}


static gboolean is_space(gchar c)
{
	return g_ascii_isspace(c);
}


static gboolean is_nonspace(gchar c)
{
	return !is_space(c);
}


static gboolean is_nonwordchar(gchar c)
{
	return !is_wordchar(c) && !is_space(c);
}


static gboolean skip_to_left(CharacterPredicate is_in_group, ScintillaObject *sci, gchar *ch, gint *pos)
{
	gboolean moved = FALSE;
	while (is_in_group(*ch) && *pos > 0)
	{
		move_left(sci, ch, pos);
		moved = TRUE;
	}
	return moved;
}


static gboolean skip_to_right(CharacterPredicate is_in_group, ScintillaObject *sci, gchar *ch, gint *pos, gint len)
{
	gboolean moved = FALSE;
	while (is_in_group(*ch) && *pos < len)
	{
		move_right(sci, ch, pos);
		moved = TRUE;
	}
	return moved;
}


static gint find_next_word(ScintillaObject *sci, gint pos, gint num)
{
	gint i;
	gint len = SSM(sci, SCI_GETLENGTH, 0, 0);

	for (i = 0; i < num; i++)
	{
		gchar ch = SSM(sci, SCI_GETCHARAT, pos, 0);

		if (!skip_to_right(is_wordchar, sci, &ch, &pos, len))
			skip_to_right(is_nonwordchar, sci, &ch, &pos, len);
		skip_to_right(is_space, sci, &ch, &pos, len);
	}
	return pos;
}


void cmd_goto_next_word(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
	pos = find_next_word(p->sci, pos, p->num);
	SET_POS(p->sci, pos, TRUE);
}


static gint find_previous_word(ScintillaObject *sci, gint pos, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		gchar ch = SSM(sci, SCI_GETCHARAT, pos, 0);

		move_left(sci, &ch, &pos);

		skip_to_left(is_space, sci, &ch, &pos);
		if (!skip_to_left(is_wordchar, sci, &ch, &pos))
			skip_to_left(is_nonwordchar, sci, &ch, &pos);

		if (pos != 0 || is_space(ch))
			move_right(sci, &ch, &pos);
	}
	return pos;
}


void cmd_goto_previous_word(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
	pos = find_previous_word(p->sci, pos, p->num);
	SET_POS(p->sci, pos, TRUE);
}


static gint find_next_word_end(ScintillaObject *sci, gint pos, gint num, gboolean include_last)
{
	gint i;
	gint len = SSM(sci, SCI_GETLENGTH, 0, 0);

	for (i = 0; i < num; i++)
	{
		gchar ch = SSM(sci, SCI_GETCHARAT, pos, 0);

		move_right(sci, &ch, &pos);

		skip_to_right(is_space, sci, &ch, &pos, len);
		if (!skip_to_right(is_wordchar, sci, &ch, &pos, len))
			skip_to_right(is_nonwordchar, sci, &ch, &pos, len);

		if (!include_last)
		{
			if (pos < len - 1 || is_space(ch))
				move_left(sci, &ch, &pos);
		}
	}
	return pos;
}


void cmd_goto_next_word_end(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
	pos = find_next_word_end(p->sci, pos, p->num, FALSE);
	SET_POS(p->sci, pos, TRUE);
}


static gint find_previous_word_end(ScintillaObject *sci, gint pos, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		gchar ch = SSM(sci, SCI_GETCHARAT, pos, 0);

		if (!skip_to_left(is_wordchar, sci, &ch, &pos))
			skip_to_left(is_nonwordchar, sci, &ch, &pos);
		skip_to_left(is_space, sci, &ch, &pos);
	}
	return pos;
}


void cmd_goto_previous_word_end(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
	pos = find_previous_word_end(p->sci, pos, p->num);
	SET_POS(p->sci, pos, TRUE);
}


static gint find_next_word_space(ScintillaObject *sci, gint pos, gint num)
{
	gint i;
	gint len = SSM(sci, SCI_GETLENGTH, 0, 0);

	for (i = 0; i < num; i++)
	{
		gchar ch = SSM(sci, SCI_GETCHARAT, pos, 0);

		skip_to_right(is_nonspace, sci, &ch, &pos, len);
		skip_to_right(is_space, sci, &ch, &pos, len);
	}
	return pos;
}


void cmd_goto_next_word_space(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
	pos = find_next_word_space(p->sci, pos, p->num);
	SET_POS(p->sci, pos, TRUE);
}


static gint find_previous_word_space(ScintillaObject *sci, gint pos, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		gchar ch = SSM(sci, SCI_GETCHARAT, pos, 0);

		move_left(sci, &ch, &pos);

		skip_to_left(is_space, sci, &ch, &pos);
		skip_to_left(is_nonspace, sci, &ch, &pos);

		if (pos != 0 || is_space(ch))
			move_right(sci, &ch, &pos);
	}
	return pos;
}


void cmd_goto_previous_word_space(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
	pos = find_previous_word_space(p->sci, pos, p->num);
	SET_POS(p->sci, pos, TRUE);
}


static gint find_next_word_end_space(ScintillaObject *sci, gint pos, gint num, gboolean include_last)
{
	gint i;
	gint len = SSM(sci, SCI_GETLENGTH, 0, 0);

	for (i = 0; i < num; i++)
	{
		gchar ch = SSM(sci, SCI_GETCHARAT, pos, 0);

		move_right(sci, &ch, &pos);

		skip_to_right(is_space, sci, &ch, &pos, len);
		skip_to_right(is_nonspace, sci, &ch, &pos, len);

		if (!include_last)
		{
			if (pos < len - 1 || is_space(ch))
				move_left(sci, &ch, &pos);
		}
	}
	return pos;
}


void cmd_goto_next_word_end_space(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
	pos = find_next_word_end_space(p->sci, pos, p->num, FALSE);
	SET_POS(p->sci, pos, TRUE);
}


static gint find_previous_word_end_space(ScintillaObject *sci, gint pos, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		gchar ch = SSM(sci, SCI_GETCHARAT, pos, 0);

		skip_to_left(is_nonspace, sci, &ch, &pos);
		skip_to_left(is_space, sci, &ch, &pos);
	}
	return pos;
}


void cmd_goto_previous_word_end_space(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_GETCURRENTPOS, 0, 0);
	pos = find_previous_word_end_space(p->sci, pos, p->num);
	SET_POS(p->sci, pos, TRUE);
}
	}
}
