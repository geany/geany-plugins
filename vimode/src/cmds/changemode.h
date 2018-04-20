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

#ifndef __VIMODE_CMDS_CHANGEMODE_H__
#define __VIMODE_CMDS_CHANGEMODE_H__

#include "context.h"
#include "cmd-params.h"

void cmd_enter_ex(CmdContext *c, CmdParams *p);
void cmd_enter_command(CmdContext *c, CmdParams *p);
void cmd_enter_command_single(CmdContext *c, CmdParams *p);
void cmd_enter_visual(CmdContext *c, CmdParams *p);
void cmd_enter_visual_line(CmdContext *c, CmdParams *p);

void cmd_enter_replace(CmdContext *c, CmdParams *p);

void cmd_enter_insert(CmdContext *c, CmdParams *p);
void cmd_enter_insert_after(CmdContext *c, CmdParams *p);
void cmd_enter_insert_line_start_nonempty(CmdContext *c, CmdParams *p);
void cmd_enter_insert_line_start(CmdContext *c, CmdParams *p);
void cmd_enter_insert_line_end(CmdContext *c, CmdParams *p);
void cmd_enter_insert_clear_right(CmdContext *c, CmdParams *p);
void cmd_enter_insert_delete_char(CmdContext *c, CmdParams *p);
void cmd_enter_insert_next_line(CmdContext *c, CmdParams *p);
void cmd_enter_insert_prev_line(CmdContext *c, CmdParams *p);
void cmd_enter_insert_cut_line(CmdContext *c, CmdParams *p);
void cmd_enter_insert_cut_sel(CmdContext *c, CmdParams *p);
void cmd_enter_insert_cut_line_sel(CmdContext *c, CmdParams *p);

void cmd_enter_command_cut_sel(CmdContext *c, CmdParams *p);
void cmd_enter_command_cut_line_sel(CmdContext *c, CmdParams *p);
void cmd_enter_command_copy_sel(CmdContext *c, CmdParams *p);
void cmd_enter_command_copy_line_sel(CmdContext *c, CmdParams *p);

#endif
