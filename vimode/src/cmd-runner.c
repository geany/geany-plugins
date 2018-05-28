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

#include "cmd-runner.h"
#include "utils.h"

#include "cmds/motion.h"
#include "cmds/txtobjs.h"
#include "cmds/changemode.h"
#include "cmds/edit.h"
#include "cmds/special.h"

#include <gdk/gdkkeysyms.h>

typedef struct {
	Cmd cmd;
	guint key1;
	guint key2;
	guint modif1;
	guint modif2;
	gboolean param;
	gboolean needs_selection;
} CmdDef;


#define ARROW_MOTIONS \
	/* left */ \
	{cmd_goto_left, GDK_KEY_Left, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_KP_Left, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_leftarrow, 0, 0, 0, FALSE, FALSE}, \
	/* right */ \
	{cmd_goto_right, GDK_KEY_Right, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_right, GDK_KEY_KP_Right, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_right, GDK_KEY_rightarrow, 0, 0, 0, FALSE, FALSE}, \
	/* up */ \
	{cmd_goto_up, GDK_KEY_Up, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up, GDK_KEY_KP_Up, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up, GDK_KEY_uparrow, 0, 0, 0, FALSE, FALSE}, \
	/* down */ \
	{cmd_goto_down, GDK_KEY_Down, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_KP_Down, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_downarrow, 0, 0, 0, FALSE, FALSE}, \
	/* goto next word */ \
	{cmd_goto_next_word, GDK_KEY_Right, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_Right, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_KP_Right, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_KP_Right, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_rightarrow, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_rightarrow, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	/* goto prev word */ \
	{cmd_goto_previous_word, GDK_KEY_Left, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_Left, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_KP_Left, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_KP_Left, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_leftarrow, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_leftarrow, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	/* page up/down */ \
	{cmd_goto_page_up, GDK_KEY_Up, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_KP_Up, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_uparrow, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_Down, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_KP_Down, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_downarrow, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	/* END */


#define MOVEMENT_CMDS \
	ARROW_MOTIONS \
	/* left */ \
	{cmd_goto_left, GDK_KEY_h, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_h, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_BackSpace, 0, 0, 0, FALSE, FALSE}, \
	/* right */ \
	{cmd_goto_right, GDK_KEY_l, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_right, GDK_KEY_space, 0, 0, 0, FALSE, FALSE}, \
	/* up */ \
	{cmd_goto_up, GDK_KEY_k, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up, GDK_KEY_p, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_up_nonempty, GDK_KEY_minus, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up_nonempty, GDK_KEY_KP_Subtract, 0, 0, 0, FALSE, FALSE}, \
	/* down */ \
	{cmd_goto_down, GDK_KEY_j, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_j, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_n, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_plus, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_KP_Add, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_m, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_Return, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_KP_Enter, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_ISO_Enter, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_one_less_nonempty, GDK_KEY_underscore, 0, 0, 0, FALSE, FALSE}, \
	/* line beginning */ \
	{cmd_goto_line_start, GDK_KEY_0, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line_start, GDK_KEY_Home, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line_start_nonempty, GDK_KEY_asciicircum, 0, 0, 0, FALSE, FALSE}, \
	/* line end */ \
	{cmd_goto_line_end, GDK_KEY_dollar, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line_end, GDK_KEY_End, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_column, GDK_KEY_bar, 0, 0, 0, FALSE, FALSE}, \
	/* find character */ \
	{cmd_goto_next_char, GDK_KEY_f, 0, 0, 0, TRUE, FALSE}, \
	{cmd_goto_prev_char, GDK_KEY_F, 0, 0, 0, TRUE, FALSE}, \
	{cmd_goto_next_char_before, GDK_KEY_t, 0, 0, 0, TRUE, FALSE}, \
	{cmd_goto_prev_char_before, GDK_KEY_T, 0, 0, 0, TRUE, FALSE}, \
	{cmd_goto_char_repeat, GDK_KEY_semicolon, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_char_repeat_opposite, GDK_KEY_comma, 0, 0, 0, FALSE, FALSE}, \
	/* goto line */ \
	{cmd_goto_line_last, GDK_KEY_G, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line_last, GDK_KEY_End, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_line_last, GDK_KEY_KP_End, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_line, GDK_KEY_g, GDK_KEY_g, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line, GDK_KEY_Home, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_line, GDK_KEY_KP_Home, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_doc_percentage, GDK_KEY_percent, 0, 0, 0, FALSE, FALSE}, \
	/* goto next word */ \
	{cmd_goto_next_word, GDK_KEY_w, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_next_word_space, GDK_KEY_W, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_next_word_end, GDK_KEY_e, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_next_word_end_space, GDK_KEY_E, 0, 0, 0, FALSE, FALSE}, \
	/* goto prev word */ \
	{cmd_goto_previous_word, GDK_KEY_b, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word_space, GDK_KEY_B, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word_end, GDK_KEY_g, GDK_KEY_e, 0, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word_end_space, GDK_KEY_g, GDK_KEY_E, 0, 0, FALSE, FALSE}, \
	/* various motions */ \
	{cmd_goto_matching_brace, GDK_KEY_percent, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_screen_top, GDK_KEY_H, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_screen_middle, GDK_KEY_M, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_screen_bottom, GDK_KEY_L, 0, 0, 0, FALSE, FALSE}, \
	/* page up/down */ \
	{cmd_goto_page_down, GDK_KEY_f, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_Page_Down, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_KP_Page_Down, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_b, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_Page_Up, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_KP_Page_Up, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_halfpage_down, GDK_KEY_d, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_halfpage_up, GDK_KEY_u, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	/* scrolling */ \
	{cmd_scroll_down, GDK_KEY_e, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_scroll_up, GDK_KEY_y, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_scroll_center, GDK_KEY_z, GDK_KEY_z, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top, GDK_KEY_z, GDK_KEY_t, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_bottom, GDK_KEY_z, GDK_KEY_b, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_center_nonempty, GDK_KEY_z, GDK_KEY_period, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top_nonempty, GDK_KEY_z, GDK_KEY_Return, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top_nonempty, GDK_KEY_z, GDK_KEY_KP_Enter, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top_nonempty, GDK_KEY_z, GDK_KEY_ISO_Enter, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top_next_nonempty, GDK_KEY_z, GDK_KEY_plus, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_bottom_nonempty, GDK_KEY_z, GDK_KEY_minus, 0, 0, FALSE, FALSE}, \
	/* END */


CmdDef movement_cmds[] = {
	MOVEMENT_CMDS
	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


#define OPERATOR_CMDS \
	{cmd_enter_command_cut_sel, GDK_KEY_d, 0, 0, 0, FALSE, TRUE}, \
	{cmd_enter_command_copy_sel, GDK_KEY_y, 0, 0, 0, FALSE, TRUE}, \
	{cmd_enter_insert_cut_sel, GDK_KEY_c, 0, 0, 0, FALSE, TRUE}, \
	{cmd_unindent_sel, GDK_KEY_less, 0, 0, 0, FALSE, TRUE}, \
	{cmd_indent_sel, GDK_KEY_greater, 0, 0, 0, FALSE, TRUE}, \
	{cmd_switch_case, GDK_KEY_g, GDK_KEY_asciitilde, 0, 0, FALSE, TRUE}, \
	{cmd_switch_case, GDK_KEY_asciitilde, 0, 0, 0, FALSE, TRUE}, \
	{cmd_upper_case, GDK_KEY_g, GDK_KEY_U, 0, 0, FALSE, TRUE}, \
	{cmd_lower_case, GDK_KEY_g, GDK_KEY_u, 0, 0, FALSE, TRUE}, \
	/* END */


CmdDef operator_cmds[] = {
	OPERATOR_CMDS
	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


#define TEXT_OBJECT_CMDS \
	/* inclusive */ \
	{cmd_select_quotedbl, GDK_KEY_a, GDK_KEY_quotedbl, 0, 0, FALSE, FALSE}, \
	{cmd_select_quoteleft, GDK_KEY_a, GDK_KEY_quoteleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_apostrophe, GDK_KEY_a, GDK_KEY_apostrophe, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace, GDK_KEY_a, GDK_KEY_braceleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace, GDK_KEY_a, GDK_KEY_braceright, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace, GDK_KEY_a, GDK_KEY_B, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren, GDK_KEY_a, GDK_KEY_parenleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren, GDK_KEY_a, GDK_KEY_parenright, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren, GDK_KEY_a, GDK_KEY_b, 0, 0, FALSE, FALSE}, \
	{cmd_select_less, GDK_KEY_a, GDK_KEY_less, 0, 0, FALSE, FALSE}, \
	{cmd_select_less, GDK_KEY_a, GDK_KEY_greater, 0, 0, FALSE, FALSE}, \
	{cmd_select_bracket, GDK_KEY_a, GDK_KEY_bracketleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_bracket, GDK_KEY_a, GDK_KEY_bracketright, 0, 0, FALSE, FALSE}, \
	/* inner */ \
	{cmd_select_quotedbl_inner, GDK_KEY_i, GDK_KEY_quotedbl, 0, 0, FALSE, FALSE}, \
	{cmd_select_quoteleft_inner, GDK_KEY_i, GDK_KEY_quoteleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_apostrophe_inner, GDK_KEY_i, GDK_KEY_apostrophe, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace_inner, GDK_KEY_i, GDK_KEY_braceleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace_inner, GDK_KEY_i, GDK_KEY_braceright, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace_inner, GDK_KEY_i, GDK_KEY_B, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren_inner, GDK_KEY_i, GDK_KEY_parenleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren_inner, GDK_KEY_i, GDK_KEY_parenright, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren_inner, GDK_KEY_i, GDK_KEY_b, 0, 0, FALSE, FALSE}, \
	{cmd_select_less_inner, GDK_KEY_i, GDK_KEY_less, 0, 0, FALSE, FALSE}, \
	{cmd_select_less_inner, GDK_KEY_i, GDK_KEY_greater, 0, 0, FALSE, FALSE}, \
	{cmd_select_bracket_inner, GDK_KEY_i, GDK_KEY_bracketleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_bracket_inner, GDK_KEY_i, GDK_KEY_bracketright, 0, 0, FALSE, FALSE}, \
	/* END */


CmdDef text_object_cmds[] = {
	TEXT_OBJECT_CMDS
	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


#define EDIT_CMDS \
	/* deletion */ \
	{cmd_delete_char_copy, GDK_KEY_x, 0, 0, 0, FALSE, FALSE}, \
	{cmd_delete_char_copy, GDK_KEY_Delete, 0, 0, 0, FALSE, FALSE}, \
	{cmd_delete_char_copy, GDK_KEY_KP_Delete, 0, 0, 0, FALSE, FALSE}, \
	{cmd_delete_char_back_copy, GDK_KEY_X, 0, 0, 0, FALSE, FALSE}, \
	{cmd_delete_line, GDK_KEY_d, GDK_KEY_d, 0, 0, FALSE, FALSE}, \
	{cmd_clear_right, GDK_KEY_D, 0, 0, 0, FALSE, FALSE}, \
	/* copy/paste */ \
	{cmd_copy_line, GDK_KEY_y, GDK_KEY_y, 0, 0, FALSE, FALSE}, \
	{cmd_copy_line, GDK_KEY_Y, 0, 0, 0, FALSE, FALSE}, \
	{cmd_paste_after, GDK_KEY_p, 0, 0, 0, FALSE, FALSE}, \
	{cmd_paste_before, GDK_KEY_P, 0, 0, 0, FALSE, FALSE}, \
	/* changing text */ \
	{cmd_enter_insert_cut_line, GDK_KEY_c, GDK_KEY_c, 0, 0, FALSE, FALSE}, \
	{cmd_enter_insert_cut_line, GDK_KEY_S, 0, 0, 0, FALSE, FALSE}, \
	{cmd_enter_insert_clear_right, GDK_KEY_C, 0, 0, 0, FALSE, FALSE}, \
	{cmd_enter_insert_delete_char, GDK_KEY_s, 0, 0, 0, FALSE, FALSE}, \
	{cmd_replace_char, GDK_KEY_r, 0, 0, 0, TRUE, FALSE}, \
	{cmd_switch_case, GDK_KEY_asciitilde, 0, 0, 0, FALSE, FALSE}, \
	{cmd_indent, GDK_KEY_greater, GDK_KEY_greater, 0, 0, FALSE, FALSE}, \
	{cmd_unindent, GDK_KEY_less, GDK_KEY_less, 0, 0, FALSE, FALSE}, \
	{cmd_repeat_subst, GDK_KEY_ampersand, 0, 0, 0, FALSE, FALSE}, \
	{cmd_join_lines, GDK_KEY_J, 0, 0, 0, FALSE, FALSE}, \
	/* undo/redo */ \
	{cmd_undo, GDK_KEY_U, 0, 0, 0, FALSE, FALSE}, \
	{cmd_undo, GDK_KEY_u, 0, 0, 0, FALSE, FALSE}, \
	{cmd_redo, GDK_KEY_r, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

CmdDef edit_cmds[] = {
	EDIT_CMDS
	OPERATOR_CMDS
	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


#define ENTER_EX_CMDS \
	{cmd_enter_ex, GDK_KEY_colon, 0, 0, 0, FALSE, FALSE}, \
	{cmd_enter_ex, GDK_KEY_slash, 0, 0, 0, FALSE, FALSE}, \
	{cmd_enter_ex, GDK_KEY_KP_Divide, 0, 0, 0, FALSE, FALSE}, \
	{cmd_enter_ex, GDK_KEY_question, 0, 0, 0, FALSE, FALSE}, \
	/* END */


#define SEARCH_CMDS \
	{cmd_search_next, GDK_KEY_n, 0, 0, 0, FALSE, FALSE}, \
	{cmd_search_next, GDK_KEY_N, 0, 0, 0, FALSE, FALSE}, \
	{cmd_search_current_next, GDK_KEY_asterisk, 0, 0, 0, FALSE, FALSE}, \
	{cmd_search_current_next, GDK_KEY_KP_Multiply, 0, 0, 0, FALSE, FALSE}, \
	{cmd_search_current_prev, GDK_KEY_numbersign, 0, 0, 0, FALSE, FALSE}, \
	/* END */

CmdDef cmd_mode_cmds[] = {
	/* enter insert mode */
	{cmd_enter_insert_after, GDK_KEY_a, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert_line_end, GDK_KEY_A, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert, GDK_KEY_i, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert, GDK_KEY_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert, GDK_KEY_KP_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert_line_start_nonempty, GDK_KEY_I, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert_line_start, GDK_KEY_g, GDK_KEY_I, 0, 0, FALSE, FALSE},
	{cmd_enter_insert_next_line, GDK_KEY_o, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert_prev_line, GDK_KEY_O, 0, 0, 0, FALSE, FALSE},
	/* enter replace mode */
	{cmd_enter_replace, GDK_KEY_R, 0, 0, 0, FALSE, FALSE},
	/* enter visual mode */
	{cmd_enter_visual, GDK_KEY_v, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_visual_line, GDK_KEY_V, 0, 0, 0, FALSE, FALSE},

	/* special */
	{cmd_repeat_last_command, GDK_KEY_period, 0, 0, 0, FALSE, FALSE},
	{cmd_repeat_last_command, GDK_KEY_KP_Decimal, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command, GDK_KEY_Escape, 0, 0, 0, FALSE, FALSE},
	{cmd_nop, GDK_KEY_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_nop, GDK_KEY_KP_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_write_exit, GDK_KEY_Z, GDK_KEY_Z, 0, 0, FALSE, FALSE},
	{cmd_force_exit, GDK_KEY_Z, GDK_KEY_Q, 0, 0, FALSE, FALSE},

	EDIT_CMDS
	OPERATOR_CMDS
	SEARCH_CMDS
	MOVEMENT_CMDS
	TEXT_OBJECT_CMDS
	ENTER_EX_CMDS

	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


CmdDef vis_mode_cmds[] = {
	{cmd_enter_command, GDK_KEY_Escape, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command, GDK_KEY_c, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_enter_visual, GDK_KEY_v, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_visual_line, GDK_KEY_V, 0, 0, 0, FALSE, FALSE},

	{cmd_enter_insert_cut_line_sel, GDK_KEY_C, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert_cut_line_sel, GDK_KEY_S, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert_cut_line_sel, GDK_KEY_R, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command_cut_line_sel, GDK_KEY_D, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command_cut_line_sel, GDK_KEY_X, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command_cut_sel, GDK_KEY_x, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command_cut_sel, GDK_KEY_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command_cut_sel, GDK_KEY_KP_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_insert_cut_sel, GDK_KEY_s, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command_copy_line_sel, GDK_KEY_Y, 0, 0, 0, FALSE, FALSE},

	{cmd_upper_case, GDK_KEY_U, 0, 0, 0, FALSE, FALSE},
	{cmd_lower_case, GDK_KEY_u, 0, 0, 0, FALSE, FALSE},
	{cmd_join_lines_sel, GDK_KEY_J, 0, 0, 0, FALSE, FALSE},
	{cmd_replace_char_sel, GDK_KEY_r, 0, 0, 0, TRUE, FALSE},

	{cmd_swap_anchor, GDK_KEY_o, 0, 0, 0, FALSE, FALSE},
	{cmd_swap_anchor, GDK_KEY_O, 0, 0, 0, FALSE, FALSE},
	{cmd_nop, GDK_KEY_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_nop, GDK_KEY_KP_Insert, 0, 0, 0, FALSE, FALSE},

	SEARCH_CMDS
	MOVEMENT_CMDS
	TEXT_OBJECT_CMDS
	OPERATOR_CMDS
	ENTER_EX_CMDS

	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


CmdDef ins_mode_cmds[] = {
	{cmd_enter_command, GDK_KEY_Escape, 0, 0, 0, FALSE, FALSE},
	{cmd_enter_command, GDK_KEY_c, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_enter_command, GDK_KEY_bracketleft, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_enter_command_single, GDK_KEY_o, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_goto_line_last, GDK_KEY_End, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_goto_line_last, GDK_KEY_KP_End, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_goto_line, GDK_KEY_Home, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_goto_line, GDK_KEY_KP_Home, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_newline, GDK_KEY_m, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_newline, GDK_KEY_j, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_tab, GDK_KEY_i, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_paste_inserted_text, GDK_KEY_a, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_paste_inserted_text_leave_ins, GDK_KEY_at, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	/* it's enough to press Ctrl+2 instead of Ctrl+Shift+2 to get Ctrl+@ */
	{cmd_paste_inserted_text_leave_ins, GDK_KEY_2, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_delete_char, GDK_KEY_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_char, GDK_KEY_KP_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_char_back, GDK_KEY_h, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_del_word_left, GDK_KEY_w, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_indent_ins, GDK_KEY_t, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_unindent_ins, GDK_KEY_d, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_copy_char_from_below, GDK_KEY_e, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_copy_char_from_above, GDK_KEY_y, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_paste_before, GDK_KEY_r, 0, GDK_CONTROL_MASK, 0, TRUE, FALSE},

	ARROW_MOTIONS

	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


static gboolean is_in_cmd_group(CmdDef *cmds, CmdDef *def)
{
	int i;
	for (i = 0; cmds[i].cmd != NULL; i++)
	{
		CmdDef *d = &cmds[i];
		if (def->cmd == d->cmd && def->key1 == d->key1 && def->key2 == d->key2 &&
			def->modif1 == d->modif1 && def->modif2 == d->modif2 && def->param == d->param)
			return TRUE;
	}
	return FALSE;
}


static gboolean key_equals(KeyPress *kp, guint key, guint modif)
{
	return kp->key == key && (kp->modif & modif || kp->modif == modif);
}


/* is the current keypress the first character of a 2-keypress command? */
static gboolean is_cmdpart(GSList *kpl, CmdDef *cmds)
{
	gint i;
	KeyPress *curr = g_slist_nth_data(kpl, 0);

	for (i = 0; cmds[i].cmd != NULL; i++)
	{
		CmdDef *cmd = &cmds[i];
		if ((cmd->key2 != 0 || cmd->param) && key_equals(curr, cmd->key1, cmd->modif1))
			return TRUE;
	}

	return FALSE;
}


static CmdDef *get_cmd_to_run(GSList *kpl, CmdDef *cmds, gboolean have_selection)
{
	gint i;
	KeyPress *curr = g_slist_nth_data(kpl, 0);
	KeyPress *prev = g_slist_nth_data(kpl, 1);
	GSList *below = g_slist_next(kpl);
	ViMode mode = vi_get_mode();

	if (!kpl)
		return NULL;

	// commands such as rc or fc (replace char c, find char c) which are specified
	// by the previous character and current character is used as their parameter
	if (prev != NULL && !kp_isdigit(prev))
	{
		for (i = 0; cmds[i].cmd != NULL; i++)
		{
			CmdDef *cmd = &cmds[i];
			if (cmd->key2 == 0 && cmd->param &&
					((cmd->needs_selection && have_selection) || !cmd->needs_selection) &&
					key_equals(prev, cmd->key1, cmd->modif1))
				return cmd;
		}
	}

	// 2-letter commands
	if (prev != NULL && !kp_isdigit(prev))
	{
		for (i = 0; cmds[i].cmd != NULL; i++)
		{
			CmdDef *cmd = &cmds[i];
			if (cmd->key2 != 0 && !cmd->param &&
					((cmd->needs_selection && have_selection) || !cmd->needs_selection) &&
					key_equals(curr, cmd->key2, cmd->modif2) &&
					key_equals(prev, cmd->key1, cmd->modif1))
				return cmd;
		}
	}

	// 1-letter commands
	for (i = 0; cmds[i].cmd != NULL; i++)
	{
		CmdDef *cmd = &cmds[i];
		if (cmd->key2 == 0 && !cmd->param &&
			((cmd->needs_selection && have_selection) || !cmd->needs_selection) &&
			key_equals(curr, cmd->key1, cmd->modif1))
		{
			// now solve some quirks manually
			if (curr->key == GDK_KEY_0 && !VI_IS_INSERT(mode))
			{
				// 0 jumps to the beginning of line only when not preceded
				// by another number in which case we want to add it to the accumulator
				if (prev == NULL || !kp_isdigit(prev))
					return cmd;
			}
			else if (curr->key == GDK_KEY_percent && !VI_IS_INSERT(mode))
			{
				// % when preceded by a number jumps to N% of the file, otherwise
				// % goes to matching brace
				Cmd c = cmd_goto_matching_brace;
				gint val = kpl_get_int(below, NULL);
				if (val != -1)
					c = cmd_goto_doc_percentage;
				if (cmd->cmd == c)
					return cmd;
			}
			else if (prev && prev->key == GDK_KEY_g)
			{
				// takes care of operator commands like g~, gu, gU where we
				// have no selection yet so the 2-letter command isn't found
				// above and a corresponding 1-letter command ~, u, U exists and
				// would be used instead of waiting for the full command
			}
			else if (is_cmdpart(kpl, text_object_cmds) &&
					get_cmd_to_run(below, operator_cmds, TRUE))
			{
				// if we received "a" or "i", we have to check if there's not
				// an operator command below because these can be part of
				// text object commands (like a<) and in this case we don't
				// want to have "a" or "i" executed yet
			}
			else
				return cmd;
		}
	}

	return NULL;
}


static void perform_cmd(CmdDef *def, CmdContext *ctx)
{
	GSList *top;
	gint num;
	gint cmd_len = 0;
	gboolean num_present;
	CmdParams param;
	gint orig_pos = SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0);
	gint sel_start, sel_len;

	if (def->key1 != 0)
		cmd_len++;
	if (def->key2 != 0)
		cmd_len++;
	if (def->param)
		cmd_len++;
	top = g_slist_nth(ctx->kpl, cmd_len);
	num = kpl_get_int(top, &top);
	num_present = num != -1;

	sel_start = SSM(ctx->sci, SCI_GETSELECTIONSTART, 0, 0);
	sel_len = SSM(ctx->sci, SCI_GETSELECTIONEND, 0, 0) - sel_start;
	cmd_params_init(&param, ctx->sci,
		num_present ? num : 1, num_present, ctx->kpl, FALSE,
		sel_start, sel_len);

	SSM(ctx->sci, SCI_BEGINUNDOACTION, 0, 0);

//	if (def->cmd != cmd_undo && def->cmd != cmd_redo)
//		SSM(sci, SCI_ADDUNDOACTION, param.pos, UNDO_MAY_COALESCE);

	def->cmd(ctx, &param);

	if (VI_IS_COMMAND(vi_get_mode()))
	{
		gboolean is_text_object_cmd = is_in_cmd_group(text_object_cmds, def);
		if (is_text_object_cmd ||is_in_cmd_group(movement_cmds, def))
		{
			def = get_cmd_to_run(top, operator_cmds, TRUE);
			if (def)
			{
				gint new_pos = SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0);

				SET_POS(ctx->sci, orig_pos, FALSE);

				if (is_text_object_cmd)
				{
					sel_start = param.sel_start;
					sel_len = param.sel_len;
				}
				else
				{
					sel_start = MIN(new_pos, orig_pos);
					sel_len = ABS(new_pos - orig_pos);
				}
				cmd_params_init(&param, ctx->sci,
					1, FALSE, top, TRUE,
					sel_start, sel_len);

				def->cmd(ctx, &param);
			}
		}
	}

	/* mode could have changed after performing command */
	if (VI_IS_COMMAND(vi_get_mode()))
		clamp_cursor_pos(ctx->sci);

	SSM(ctx->sci, SCI_ENDUNDOACTION, 0, 0);
}


static gboolean perform_repeat_cmd(CmdContext *ctx)
{
	GSList *top = g_slist_next(ctx->kpl);  // get behind "."
	gint num = kpl_get_int(top, NULL);
	CmdDef *def;
	gint i;

	def = get_cmd_to_run(ctx->repeat_kpl, edit_cmds, FALSE);
	if (!def)
		return FALSE;

	num = num == -1 ? 1 : num;
	for (i = 0; i < num; i++)
		perform_cmd(def, ctx);

	return TRUE;
}


static gboolean process_cmd(CmdDef *cmds, CmdContext *ctx)
{
	gboolean consumed;
	gboolean performed = FALSE;
	ViMode orig_mode = vi_get_mode();
	gboolean have_selection =
		SSM(ctx->sci, SCI_GETSELECTIONEND, 0, 0) - SSM(ctx->sci, SCI_GETSELECTIONSTART, 0, 0) > 0;
	CmdDef *def = get_cmd_to_run(ctx->kpl, cmds, have_selection);

	consumed = is_cmdpart(ctx->kpl, cmds);

	if (def)
	{
		if (def->cmd == cmd_repeat_last_command)
		{
			if (perform_repeat_cmd(ctx))
			{
				performed = TRUE;

				g_slist_free_full(ctx->kpl, g_free);
				ctx->kpl = NULL;
			}
		}
		else
		{
			perform_cmd(def, ctx);
			performed = TRUE;

			if (is_in_cmd_group(edit_cmds, def))
			{
				g_slist_free_full(ctx->repeat_kpl, g_free);
				ctx->repeat_kpl = ctx->kpl;
			}
			else
				g_slist_free_full(ctx->kpl, g_free);
			ctx->kpl = NULL;
		}
	}

	consumed = consumed || performed;

	if (performed)
	{
		if (orig_mode == VI_MODE_COMMAND_SINGLE)
			vi_set_mode(VI_MODE_INSERT);
	}
	else if (!consumed && ctx->kpl)
	{
		g_free(ctx->kpl->data);
		ctx->kpl = g_slist_delete_link(ctx->kpl, ctx->kpl);
	}

	return consumed;
}


gboolean cmd_perform_cmd(CmdContext *ctx)
{
	return process_cmd(cmd_mode_cmds, ctx);
}


gboolean cmd_perform_vis(CmdContext *ctx)
{
	return process_cmd(vis_mode_cmds, ctx);
}


gboolean cmd_perform_ins(CmdContext *ctx)
{
	return process_cmd(ins_mode_cmds, ctx);
}
