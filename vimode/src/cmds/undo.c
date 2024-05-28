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

#include "undo.h"
#include "utils.h"

void undo_update(CmdContext *c, gint pos)
{
	c->undo_pos = pos;
}


static gboolean is_start_of_line(ScintillaObject *sci, gint pos)
{
	gint line = SSM(sci, SCI_LINEFROMPOSITION, pos, 0);
	gint line_pos = SSM(sci, SCI_POSITIONFROMLINE, line, 0);

	return pos == line_pos;
}


void undo_apply(CmdContext *c, gint num)
{
	ScintillaObject *sci = c->sci;
	gint i;

	c->undo_pos = -1;

	for (i = 0; i < num; i++)
	{
		if (!SSM(sci, SCI_CANUNDO, 0, 0))
			break;
		SSM(sci, SCI_UNDO, 0, 0);
	}

	/* exit when no undo has been applied */
	if (c->undo_pos == -1)
		return;

	if (is_start_of_line(sci, c->undo_pos))
		goto_nonempty(sci, SSM(sci, SCI_LINEFROMPOSITION, c->undo_pos, 0), FALSE);
	else
		SET_POS(sci, c->undo_pos, FALSE);
}
