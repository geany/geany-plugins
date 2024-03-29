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

#ifndef __VIMODE_CMDS_EXCMDS_H__
#define __VIMODE_CMDS_EXCMDS_H__

#include "excmd-params.h"
#include "context.h"

void excmd_save(CmdContext *c, ExCmdParams *p);
void excmd_save_all(CmdContext *c, ExCmdParams *p);
void excmd_quit(CmdContext *c, ExCmdParams *p);
void excmd_save_quit(CmdContext *c, ExCmdParams *p);
void excmd_save_all_quit(CmdContext *c, ExCmdParams *p);
void excmd_repeat_subst(CmdContext *c, ExCmdParams *p);
void excmd_repeat_subst_orig_flags(CmdContext *c, ExCmdParams *p);
void excmd_yank(CmdContext *c, ExCmdParams *p);
void excmd_put(CmdContext *c, ExCmdParams *p);
void excmd_undo(CmdContext *c, ExCmdParams *p);
void excmd_redo(CmdContext *c, ExCmdParams *p);
void excmd_shift_left(CmdContext *c, ExCmdParams *p);
void excmd_shift_right(CmdContext *c, ExCmdParams *p);
void excmd_delete(CmdContext *c, ExCmdParams *p);
void excmd_join(CmdContext *c, ExCmdParams *p);
void excmd_copy(CmdContext *c, ExCmdParams *p);
void excmd_move(CmdContext *c, ExCmdParams *p);

#endif
