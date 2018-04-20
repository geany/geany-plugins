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

#ifndef __VIMODE_CMDS_TXTOBJS_H__
#define __VIMODE_CMDS_TXTOBJS_H__

#include "context.h"
#include "cmd-params.h"

void cmd_select_quotedbl(CmdContext *c, CmdParams *p);
void cmd_select_quoteleft(CmdContext *c, CmdParams *p);
void cmd_select_apostrophe(CmdContext *c, CmdParams *p);
void cmd_select_brace(CmdContext *c, CmdParams *p);
void cmd_select_paren(CmdContext *c, CmdParams *p);
void cmd_select_less(CmdContext *c, CmdParams *p);
void cmd_select_bracket(CmdContext *c, CmdParams *p);

void cmd_select_quotedbl_inner(CmdContext *c, CmdParams *p);
void cmd_select_quoteleft_inner(CmdContext *c, CmdParams *p);
void cmd_select_apostrophe_inner(CmdContext *c, CmdParams *p);
void cmd_select_brace_inner(CmdContext *c, CmdParams *p);
void cmd_select_paren_inner(CmdContext *c, CmdParams *p);
void cmd_select_less_inner(CmdContext *c, CmdParams *p);
void cmd_select_bracket_inner(CmdContext *c, CmdParams *p);

#endif
