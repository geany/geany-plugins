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

#ifndef __CMD_PARAMS_H__
#define __CMD_PARAMS_H__

#include "sci.h"
#include "keypress.h"
#include "context.h"

typedef struct
{
	/* current Scintilla object - the same one as in CmdContext, added here
	 * for convenience */
	ScintillaObject *sci;

	/* number preceding command - defaults to 1 when not present */
	gint num;
	/* determines if the command was preceded by number */
	gboolean num_present;
	/* last key press */
	KeyPress *last_kp;
	/* if running as an "operator" command */
	gboolean is_operator_cmd;

	/* selection start or selection made by movement command */
	gint sel_start;
	/* selection length or selection made by movement command */
	gint sel_len;
	/* first line of selection */
	gint sel_first_line;
	/* the beginning of the first line of selection */
	gint sel_first_line_begin_pos;
	/* last line of selection */
	gint sel_last_line;
	/* the end of the last line of selection */
	gint sel_last_line_end_pos;

	/* current position in scintilla */
	gint pos;
	/* current line in scintilla */
	gint line;
	/* position of the end of the current line */
	gint line_end_pos;
	/* position of the start of the current line */
	gint line_start_pos;
	/* number of lines in document */
	gint line_num;
	/* first visible line */
	gint line_visible_first;
	/* number of displayed lines */
	gint line_visible_num;
} CmdParams;

typedef void (*Cmd)(CmdContext *c, CmdParams *p);

void cmd_params_init(CmdParams *param, ScintillaObject *sci,
	gint num, gboolean num_present, GSList *kpl, gboolean is_operator_cmd,
	gint sel_start, gint sel_len);

#endif
