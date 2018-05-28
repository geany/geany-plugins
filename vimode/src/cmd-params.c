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

#include "cmd-params.h"

void cmd_params_init(CmdParams *param, ScintillaObject *sci,
	gint num, gboolean num_present, GSList *kpl, gboolean is_operator_cmd,
	gint sel_start, gint sel_len)
{
	param->sci = sci;

	param->num = num;
	param->num_present = num_present;
	param->last_kp = g_slist_nth_data(kpl, 0);
	param->is_operator_cmd = is_operator_cmd;

	param->sel_start = sel_start;
	param->sel_len = sel_len;
	param->sel_first_line = SSM(sci, SCI_LINEFROMPOSITION, sel_start, 0);
	param->sel_first_line_begin_pos = SSM(sci, SCI_POSITIONFROMLINE, param->sel_first_line, 0);
	param->sel_last_line = SSM(sci, SCI_LINEFROMPOSITION, sel_start + sel_len, 0);
	param->sel_last_line_end_pos = SSM(sci, SCI_GETLINEENDPOSITION, param->sel_last_line, 0);

	param->pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	param->line = GET_CUR_LINE(sci);
	param->line_end_pos = SSM(sci, SCI_GETLINEENDPOSITION, param->line, 0);
	param->line_start_pos = SSM(sci, SCI_POSITIONFROMLINE, param->line, 0);
	param->line_num = SSM(sci, SCI_GETLINECOUNT, 0, 0);
	param->line_visible_first = SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	param->line_visible_num = SSM(sci, SCI_LINESONSCREEN, 0, 0);
}
