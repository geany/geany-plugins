/*
 * Copyright 2024 Sylvain Cresto <scresto@gmail.com>
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

#include "cmds/fold.h"
#include "utils.h"

#define GOTO_NEAREST_PARENT		0
#define GOTO_TOPMOST_PARENT		1
#define GOTO_CONTRACTED_PARENT		2

static gint goto_above_fold(CmdParams *p, gint type)
{
	/* foldparent of the next line */
	gint line = SSM(p->sci, SCI_GETFOLDPARENT, p->line + 1, 0);

	if (p->line == line)
		;  /* we are already on the fold point line */
	else
	{
		/* foldparent of the current line */
		line = SSM(p->sci, SCI_GETFOLDPARENT, p->line, 0);
	}

	/* retreive first parent when type != GOTO_NEAREST_PARENT
	   when type == GOTO_CONTRACTED_PARENT we stop on first contracted parent if exist
	*/
	if (type == GOTO_CONTRACTED_PARENT && line != -1 && ! SSM(p->sci, SCI_GETFOLDEXPANDED, line, 0))
		;	/* this fold point is contracted and type == GOTO_CONTRACTED_PARENT */
	else if (type != GOTO_NEAREST_PARENT)
	{
		gint prev_line = line;
		while (prev_line != -1)
		{
			prev_line = SSM(p->sci, SCI_GETFOLDPARENT, prev_line, 0);
			if (prev_line != -1)
			{
				line = prev_line;
				if (type == GOTO_CONTRACTED_PARENT && ! SSM(p->sci, SCI_GETFOLDEXPANDED, line, 0))
					break;
			}
		}
	}

	if (line != -1)
	{
		/* move the cursor on the visible line before the fold */
		gint pos = SSM(p->sci, SCI_POSITIONFROMLINE, line, 0);
		SET_POS_NOX(p->sci, pos, TRUE);
	}

	return line;
}


void cmd_toggle_fold(CmdContext *c, CmdParams *p)
{
	gint line = goto_above_fold(p, GOTO_NEAREST_PARENT);
	if (line != -1)
		SSM(p->sci, SCI_FOLDLINE, (uptr_t) line, SC_FOLDACTION_TOGGLE);
}


void cmd_open_fold(CmdContext *c, CmdParams *p)
{
	gint line = goto_above_fold(p, GOTO_NEAREST_PARENT);
	if (line != -1)
		SSM(p->sci, SCI_FOLDLINE, (uptr_t) line, SC_FOLDACTION_EXPAND);
}


void cmd_close_fold(CmdContext *c, CmdParams *p)
{
	gint line = goto_above_fold(p, GOTO_NEAREST_PARENT);
	if (line != -1)
		SSM(p->sci, SCI_FOLDLINE, (uptr_t) line, SC_FOLDACTION_CONTRACT);
}


void cmd_toggle_fold_child(CmdContext *c, CmdParams *p)
{
	gint line = goto_above_fold(p, GOTO_CONTRACTED_PARENT);
	if (line != -1)
		SSM(p->sci, SCI_FOLDCHILDREN, (uptr_t) line, SC_FOLDACTION_TOGGLE);
}


void cmd_open_fold_child(CmdContext *c, CmdParams *p)
{
	gint line = goto_above_fold(p, GOTO_NEAREST_PARENT);
	SSM(p->sci, SCI_FOLDCHILDREN, (uptr_t) line, SC_FOLDACTION_EXPAND);
}


void cmd_close_fold_child(CmdContext *c, CmdParams *p)
{
	gint line = goto_above_fold(p, GOTO_TOPMOST_PARENT);
	if (line != -1)
		SSM(p->sci, SCI_FOLDCHILDREN, (uptr_t) line, SC_FOLDACTION_CONTRACT);
}


void cmd_open_fold_all(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_FOLDALL, SC_FOLDACTION_EXPAND | SC_FOLDACTION_CONTRACT_EVERY_LEVEL, 0);
}


void cmd_close_fold_all(CmdContext *c, CmdParams *p)
{
	goto_above_fold(p, GOTO_TOPMOST_PARENT);
	SSM(p->sci, SCI_FOLDALL, SC_FOLDACTION_CONTRACT | SC_FOLDACTION_CONTRACT_EVERY_LEVEL, 0);
}
