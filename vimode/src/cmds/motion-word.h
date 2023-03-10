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

#ifndef __VIMODE_CMDS_MOTION_WORD_H__
#define __VIMODE_CMDS_MOTION_WORD_H__

#include "context.h"
#include "cmd-params.h"

void cmd_goto_next_word(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word(CmdContext *c, CmdParams *p);
void cmd_goto_next_word_end(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word_end(CmdContext *c, CmdParams *p);
void cmd_goto_next_word_space(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word_space(CmdContext *c, CmdParams *p);
void cmd_goto_next_word_end_space(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word_end_space(CmdContext *c, CmdParams *p);

void get_word_range(ScintillaObject *sci, gboolean word_space, gboolean inner,
	gint pos, gint num, gint *sel_start, gint *sel_len);

#endif
