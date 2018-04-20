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

#ifndef __VIMODE_CMDS_EDIT_H__
#define __VIMODE_CMDS_EDIT_H__

#include "context.h"
#include "cmd-params.h"

void cmd_delete_char(CmdContext *c, CmdParams *p);
void cmd_delete_char_copy(CmdContext *c, CmdParams *p);
void cmd_delete_char_back(CmdContext *c, CmdParams *p);
void cmd_delete_char_back_copy(CmdContext *c, CmdParams *p);

void cmd_clear_right(CmdContext *c, CmdParams *p);
void cmd_delete_line(CmdContext *c, CmdParams *p);
void cmd_copy_line(CmdContext *c, CmdParams *p);

void cmd_newline(CmdContext *c, CmdParams *p);
void cmd_tab(CmdContext *c, CmdParams *p);
void cmd_del_word_left(CmdContext *c, CmdParams *p);

void cmd_undo(CmdContext *c, CmdParams *p);
void cmd_redo(CmdContext *c, CmdParams *p);

void cmd_paste_after(CmdContext *c, CmdParams *p);
void cmd_paste_before(CmdContext *c, CmdParams *p);

void cmd_join_lines(CmdContext *c, CmdParams *p);
void cmd_join_lines_sel(CmdContext *c, CmdParams *p);

void cmd_replace_char(CmdContext *c, CmdParams *p);
void cmd_replace_char_sel(CmdContext *c, CmdParams *p);

void cmd_switch_case(CmdContext *c, CmdParams *p);
void cmd_lower_case(CmdContext *c, CmdParams *p);
void cmd_upper_case(CmdContext *c, CmdParams *p);

void cmd_indent(CmdContext *c, CmdParams *p);
void cmd_unindent(CmdContext *c, CmdParams *p);
void cmd_indent_sel(CmdContext *c, CmdParams *p);
void cmd_unindent_sel(CmdContext *c, CmdParams *p);
void cmd_indent_ins(CmdContext *c, CmdParams *p);
void cmd_unindent_ins(CmdContext *c, CmdParams *p);

void cmd_copy_char_from_above(CmdContext *c, CmdParams *p);
void cmd_copy_char_from_below(CmdContext *c, CmdParams *p);

void cmd_repeat_subst(CmdContext *c, CmdParams *p);

#endif
