/*
 *		watch_model.h
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/* tree view columns */
enum
{
   W_NAME,
   W_VALUE,
   W_TYPE,
   W_INTERNAL,
   W_EXPRESSION,
   W_STUB,
   W_CHANGED,
   W_N_COLUMNS
};

/* types for the callbacks */
typedef void (*watch_expanded_callback)(GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data);
typedef void (*new_watch_dragged)(GtkWidget *wgt, GdkDragContext *context, int x, int y, GtkSelectionData *seldata, guint info, guint time, gpointer userdata);
typedef gboolean (*watch_key_pressed)(GtkWidget *widget, GdkEvent *event, gpointer user_data);
typedef gboolean (*watch_button_pressed)(GtkWidget *treeview, GdkEventButton *event, gpointer userdata);
typedef void (*watch_render_name)(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);
typedef void (*watch_expression_changed)(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data);

void	update_variables(GtkTreeView *tree, GtkTreeIter *parent, GList *vars);
void	clear_watch_values(GtkTreeView *tree);
GList*	get_root_items(GtkTreeView *tree);
void	change_watch(GtkTreeView *tree, GtkTreeIter *iter, gpointer var);
void	free_variables_list(GList *vars);
void	variable_set_name_only(GtkTreeStore *store, GtkTreeIter *iter, gchar *name);
