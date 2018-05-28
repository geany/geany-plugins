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

#ifndef __VIMODE_CMDS_SPECIAL_H__
#define __VIMODE_CMDS_SPECIAL_H__

#include "context.h"
#include "cmd-params.h"

void cmd_swap_anchor(CmdContext *c, CmdParams *p);
void cmd_nop(CmdContext *c, CmdParams *p);
void cmd_repeat_last_command(CmdContext *c, CmdParams *p);
void cmd_paste_inserted_text(CmdContext *c, CmdParams *p);
void cmd_paste_inserted_text_leave_ins(CmdContext *c, CmdParams *p);

void cmd_write_exit(CmdContext *c, CmdParams *p);
void cmd_force_exit(CmdContext *c, CmdParams *p);

void cmd_search_next(CmdContext *c, CmdParams *p);
void cmd_search_current_next(CmdContext *c, CmdParams *p);
void cmd_search_current_prev(CmdContext *c, CmdParams *p);

#endif
