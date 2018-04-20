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

#include "utils.h"

#include <string.h>


void clamp_cursor_pos(ScintillaObject *sci)
{
	gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	gint line = GET_CUR_LINE(sci);
	gint start_pos = SSM(sci, SCI_POSITIONFROMLINE, line, 0);
	gint end_pos = SSM(sci, SCI_GETLINEENDPOSITION, line, 0);
	if (pos == end_pos && pos != start_pos)
		SET_POS_NOX(sci, pos-1, FALSE);
}


static gchar *get_contents_range(ScintillaObject *sci, gint start, gint end)
{
	struct Sci_TextRange tr;
	gchar *text = g_malloc(end - start + 1);

	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = text;

	SSM(sci, SCI_GETTEXTRANGE, 0, (sptr_t)&tr);
	return text;
}


gchar *get_current_word(ScintillaObject *sci)
{
	gint start, end, pos;

	if (!sci)
		return NULL;

	pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	start = SSM(sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	end = SSM(sci, SCI_WORDENDPOSITION, pos, TRUE);

	if (start == end)
		return NULL;

	return get_contents_range(sci, start, end);
}


gint perform_search(ScintillaObject *sci, const gchar *search_text,
	gint num, gboolean invert)
{
	struct Sci_TextToFind ttf;
	gint flags = SCFIND_REGEXP | SCFIND_MATCHCASE;
	gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	gint len = SSM(sci, SCI_GETLENGTH, 0, 0);
	gboolean forward;
	GString *s;
	gint i;

	if (!search_text)
		return -1;

	s = g_string_new(search_text);
	while (TRUE)
	{
		gchar *p = strstr(s->str, "\\c");
		if (!p)
			break;
		g_string_erase(s, p - s->str, 2);
		flags &= ~SCFIND_MATCHCASE;
	}

	forward = s->str[0] == '/';
	forward = !forward != !invert;
	ttf.lpstrText = s->str + 1;

	for (i = 0; i < num; i++)
	{
		gint new_pos;

		if (forward)
		{
			ttf.chrg.cpMin = pos + 1;
			ttf.chrg.cpMax = len;
		}
		else
		{
			ttf.chrg.cpMin = pos;
			ttf.chrg.cpMax = 0;
		}

		new_pos = SSM(sci, SCI_FINDTEXT, flags, (sptr_t)&ttf);
		if (new_pos < 0)
		{
			/* wrap */
			if (forward)
			{
				ttf.chrg.cpMin = 0;
				ttf.chrg.cpMax = pos;
			}
			else
			{
				ttf.chrg.cpMin = len;
				ttf.chrg.cpMax = pos;
			}

			new_pos = SSM(sci, SCI_FINDTEXT, flags, (sptr_t)&ttf);
		}

		if (new_pos < 0)
			break;
		pos = new_pos;
	}

	g_string_free(s, TRUE);
	return pos;
}


void perform_substitute(ScintillaObject *sci, const gchar *cmd, gint from, gint to,
	const gchar *flag_override)
{
	gchar *copy = g_strdup(cmd);
	gchar *p = copy;
	gchar *pattern = NULL;
	gchar *repl = NULL;
	gchar *flags = NULL;

	if (!cmd)
		return;

	while (*p)
	{
		if (*p == '/' && *(p-1) != '\\')
		{
			if (!pattern)
				pattern = p+1;
			else if (!repl)
				repl = p+1;
			else if (!flags)
				flags = p+1;
			*p = '\0';
		}
		p++;
	}

	if (flag_override)
		flags = (gchar *)flag_override;

	if (pattern && repl)
	{
		struct Sci_TextToFind ttf;
		gint find_flags = SCFIND_REGEXP | SCFIND_MATCHCASE;
		GString *s = g_string_new(pattern);
		gboolean all = flags && strstr(flags, "g") != NULL;

		while (TRUE)
		{
			p = strstr(s->str, "\\c");
			if (!p)
				break;
			g_string_erase(s, p - s->str, 2);
			find_flags &= ~SCFIND_MATCHCASE;
		}

		ttf.lpstrText = s->str;
		ttf.chrg.cpMin = SSM(sci, SCI_POSITIONFROMLINE, from, 0);
		ttf.chrg.cpMax = SSM(sci, SCI_GETLINEENDPOSITION, to, 0);
		while (SSM(sci, SCI_FINDTEXT, find_flags, (sptr_t)&ttf) != -1)
		{
			SSM(sci, SCI_SETTARGETSTART, ttf.chrgText.cpMin, 0);
			SSM(sci, SCI_SETTARGETEND, ttf.chrgText.cpMax, 0);
			SSM(sci, SCI_REPLACETARGET, -1, (sptr_t)repl);

			if (!all)
				break;
		}

		g_string_free(s, TRUE);
	}

	g_free(copy);
}


gint get_line_number_rel(ScintillaObject *sci, gint shift)
{
	gint new_line = GET_CUR_LINE(sci) + shift;
	gint lines = SSM(sci, SCI_GETLINECOUNT, 0, 0);
	new_line = new_line < 0 ? 0 : new_line;
	new_line = new_line > lines ? lines : new_line;
	return new_line;
}

void goto_nonempty(ScintillaObject *sci, gint line, gboolean scroll)
{
	gint line_end_pos = SSM(sci, SCI_GETLINEENDPOSITION, line, 0);
	gint pos = SSM(sci, SCI_POSITIONFROMLINE, line, 0);

	while (g_ascii_isspace(SSM(sci, SCI_GETCHARAT, pos, 0)) && pos < line_end_pos)
		pos = NEXT(sci, pos);
	SET_POS(sci, pos, scroll);
}
