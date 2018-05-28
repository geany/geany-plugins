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

#ifndef __VIMODE_CMDS_MOTION_H__
#define __VIMODE_CMDS_MOTION_H__

#include "context.h"
#include "cmd-params.h"

void cmd_goto_left(CmdContext *c, CmdParams *p);
void cmd_goto_right(CmdContext *c, CmdParams *p);
void cmd_goto_up(CmdContext *c, CmdParams *p);
void cmd_goto_up_nonempty(CmdContext *c, CmdParams *p);
void cmd_goto_down(CmdContext *c, CmdParams *p);
void cmd_goto_down_nonempty(CmdContext *c, CmdParams *p);
void cmd_goto_down_one_less_nonempty(CmdContext *c, CmdParams *p);

void cmd_goto_page_up(CmdContext *c, CmdParams *p);
void cmd_goto_page_down(CmdContext *c, CmdParams *p);
void cmd_goto_halfpage_up(CmdContext *c, CmdParams *p);
void cmd_goto_halfpage_down(CmdContext *c, CmdParams *p);
void cmd_goto_line(CmdContext *c, CmdParams *p);
void cmd_goto_line_last(CmdContext *c, CmdParams *p);
void cmd_goto_screen_top(CmdContext *c, CmdParams *p);
void cmd_goto_screen_middle(CmdContext *c, CmdParams *p);
void cmd_goto_screen_bottom(CmdContext *c, CmdParams *p);
void cmd_goto_doc_percentage(CmdContext *c, CmdParams *p);

void cmd_goto_next_word(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word(CmdContext *c, CmdParams *p);
void cmd_goto_next_word_end(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word_end(CmdContext *c, CmdParams *p);
void cmd_goto_next_word_space(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word_space(CmdContext *c, CmdParams *p);
void cmd_goto_next_word_end_space(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word_end_space(CmdContext *c, CmdParams *p);

void cmd_goto_line_start(CmdContext *c, CmdParams *p);
void cmd_goto_line_start_nonempty(CmdContext *c, CmdParams *p);
void cmd_goto_line_end(CmdContext *c, CmdParams *p);
void cmd_goto_column(CmdContext *c, CmdParams *p);

void cmd_goto_matching_brace(CmdContext *c, CmdParams *p);

void cmd_goto_next_char(CmdContext *c, CmdParams *p);
void cmd_goto_prev_char(CmdContext *c, CmdParams *p);
void cmd_goto_next_char_before(CmdContext *c, CmdParams *p);
void cmd_goto_prev_char_before(CmdContext *c, CmdParams *p);
void cmd_goto_char_repeat(CmdContext *c, CmdParams *p);
void cmd_goto_char_repeat_opposite(CmdContext *c, CmdParams *p);

void cmd_scroll_up(CmdContext *c, CmdParams *p);
void cmd_scroll_down(CmdContext *c, CmdParams *p);
void cmd_scroll_center(CmdContext *c, CmdParams *p);
void cmd_scroll_top(CmdContext *c, CmdParams *p);
void cmd_scroll_bottom(CmdContext *c, CmdParams *p);
void cmd_scroll_center_nonempty(CmdContext *c, CmdParams *p);
void cmd_scroll_top_nonempty(CmdContext *c, CmdParams *p);
void cmd_scroll_top_next_nonempty(CmdContext *c, CmdParams *p);
void cmd_scroll_bottom_nonempty(CmdContext *c, CmdParams *p);

#endif
