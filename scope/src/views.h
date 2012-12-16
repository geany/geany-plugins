/*
 *  views.h
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

#ifndef VIEWS_H

typedef enum _ViewIndex
{
	VIEW_TERMINAL,
	VIEW_THREADS,
	VIEW_BREAKS,
	VIEW_STACK,
	VIEW_LOCALS,
	VIEW_WATCHES,
	VIEW_CONSOLE,
	VIEW_INSPECT,
	VIEW_TOOLTIP,
	VIEW_POPMENU,
	VIEW_COUNT
} ViewIndex;

#define DS_SORTABLE 0

void view_dirty(ViewIndex index);
void views_clear(void);
void views_update(DebugState state);
gboolean view_stack_update(void);
void view_inspect_update(void);

void on_view_changed(GtkNotebook *notebook, gpointer page, gint page_num, gpointer gdata);
typedef void (*ViewSeeker)(gboolean focus);
gboolean on_view_key_press(GtkWidget *widget, GdkEventKey *event, ViewSeeker seeker);
gboolean on_view_button_1_press(GtkWidget *widget, GdkEventButton *event, ViewSeeker seeker);
gboolean on_view_query_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_tip,
	GtkTooltip *tooltip, GtkTreeViewColumn *base_name_column);

enum
{
	COLUMN_ID,
	COLUMN_FILE,  /* locale */
	COLUMN_LINE   /* 1-based */
};

typedef struct _TreeCell
{
	const char *name;
	GCallback callback;
} TreeCell;

GtkTreeView *view_create(const char *name, GtkTreeModel **model, GtkTreeSelection **selection);
GtkTreeView *view_connect(const char *name, GtkTreeModel **model, GtkTreeSelection **selection,
	const TreeCell *cell_info, const char *window, GObject **display);
/* note: 2 references to column */
#define view_set_sort_func(sortable, column, compare) \
	gtk_tree_sortable_set_sort_func((sortable), (column), (compare), \
		GINT_TO_POINTER(column), NULL)
void view_set_line_data_func(const char *column, const char *cell, gint column_id);
void view_display_edited(GtkTreeModel *model, GtkTreeIter *iter, const gchar *new_text,
	const char *format);

void view_column_set_visible(const char *name, gboolean visible);
void view_seek_selected(GtkTreeSelection *selection, gboolean focus, SeekerType seeker);
void view_command_line(const gchar *text, const gchar *title, const gchar *seek,
	gboolean seek_after);
void views_update_state(DebugState state);

void views_init(void);
void views_finalize(void);

#define VIEWS_H 1
#endif
