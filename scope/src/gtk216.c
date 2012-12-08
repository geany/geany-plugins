/*
 *  gtk216.c
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"

#if !GTK_CHECK_VERSION(2, 18, 0)
void gtk_widget_set_visible(GtkWidget *widget, gboolean visible)
{
	if (visible)
		gtk_widget_show(widget);
	else
		gtk_widget_hide(widget);
}
#endif  /* GTK 2.18.0 */

typedef struct _SortColumnId
{
	const char *id;
	gint sort_column_id;
} SortColumnId;

static SortColumnId sort_column_ids[] =
{
	{ "thread_id_column",        0 },
	{ "thread_pid_column",       3 },
	{ "thread_group_id_column",  4 },
	{ "thread_state_column",     5 },
	{ "thread_base_name_column", 1 },
	{ "thread_func_column",      7 },
	{ "thread_addr_column",      8 },
	{ "thread_target_id_column", 9 },
	{ "thread_core_column",      10 },
	{ "break_type_column",       4 },
	{ "break_id_column",         0 },
	{ "break_enabled_column",    5 },
	{ "break_display_column",    14 },
	{ "break_addr_column",       8 },
	{ "break_times_column",      9 },
	{ "break_ignore_column",     10 },
	{ "break_cond_column",       11 },
	{ "break_script_column",     12 },
	{ "stack_id_column",         0 },
	{ "stack_base_name_column",  1 },
	{ "stack_func_column",       4 },
	{ "stack_args_column",       5 },
	{ "stack_addr_column",       6 },
	{ "local_name_column",       0 },
	{ "local_arg1_column",       5 },
	{ "local_display_column",    1 },
	{ "watch_enabled_column",    6 },
	{ "watch_expr_column",       0 },
	{ "watch_display_column",    1 },
	{ NULL, 0 }
};

void gtk216_init(void)
{
	SortColumnId *scd;

	for (scd = sort_column_ids; scd->id; scd++)
	{
		GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN(get_object(scd->id));
		gtk_tree_view_column_set_sort_column_id(column, scd->sort_column_id);
	}
}
