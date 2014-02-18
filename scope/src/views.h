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
	VIEW_MEMORY,
	VIEW_CONSOLE,
	VIEW_INSPECT,
	VIEW_REGISTERS,
	VIEW_TOOLTIP,
	VIEW_POPMENU,
	VIEW_COUNT
} ViewIndex;

void view_dirty(ViewIndex index);
void views_context_dirty(DebugState state, gboolean frame_only);
#define views_data_dirty(state) views_context_dirty((state), FALSE)
void views_clear(void);
void views_update(DebugState state);
gboolean view_stack_update(void);
#define view_frame_update() (g_strcmp0(frame_id, "0") && view_stack_update())
void view_local_update(void);

void on_view_changed(GtkNotebook *notebook, gpointer page, gint page_num, gpointer gdata);
typedef void (*ViewSeeker)(gboolean focus);
gboolean on_view_key_press(GtkWidget *widget, GdkEventKey *event, ViewSeeker seeker);
gboolean on_view_button_1_press(GtkWidget *widget, GdkEventButton *event, ViewSeeker seeker);
gboolean on_view_query_base_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_tip,
	GtkTooltip *tooltip, GtkTreeViewColumn *base_name_column);
gboolean on_view_editable_map(GtkWidget *widget, gchar *replace);

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

GtkTreeView *view_create(const char *name, ScpTreeStore **store, GtkTreeSelection **selection);
GtkTreeView *view_connect(const char *name, ScpTreeStore **store, GtkTreeSelection **selection,
	const TreeCell *cell_info, const char *window, GObject **display_cell);
/* note: 2 references to column */
#define view_set_sort_func(store, column, compare) \
	scp_tree_store_set_sort_func((store), (column), (GtkTreeIterCompareFunc) (compare), \
	GINT_TO_POINTER(column), NULL)
void view_set_line_data_func(const char *column, const char *cell, gint column_id);
void view_display_edited(ScpTreeStore *store, gboolean condition, const gchar *path_str,
	const char *format, gchar *new_text);

void view_column_set_visible(const char *name, gboolean visible);
void view_seek_selected(GtkTreeSelection *selection, gboolean focus, SeekerType seeker);
void view_command_line(const gchar *text, const gchar *title, const gchar *seek,
	gboolean seek_after);
void views_update_state(DebugState state);

void views_init(void);
void views_finalize(void);

#define VIEWS_H 1
#endif
