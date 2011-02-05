/*
 *		vtree.c
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

/*
 *		Variables tree view.
 * 		Base class for locals and watch tree views.
 */

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "geanyplugin.h"

#include "watch_model.h"
#include "utils.h"

/* columns minumum width in characters */
#define MW_NAME		20
#define MW_VALUE	0
#define MW_TYPE		10

/*
 * on row collapsed handler
 */
void on_row_collapsed (GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	/* autosize tree view columns */
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW(tree_view));
}

/*
 * key pressed event
 */
static gboolean on_key_pressed(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    guint keyval = ((GdkEventKey*)event)->keyval;

	if (GDK_Left == keyval || GDK_Right == keyval)
	{
		GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(widget));
		if(1 == gtk_tree_selection_count_selected_rows(selection))
		{
			GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
			GList *rows = gtk_tree_selection_get_selected_rows(selection, &model);

			if (GDK_Right == keyval)
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), (GtkTreePath*)rows->data, FALSE);
			else
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), (GtkTreePath*)rows->data);
			
			g_list_foreach(rows, (GFunc)gtk_tree_path_free, NULL);		
			g_list_free(rows);
		}
	}

	return FALSE;
}


/*
 * value rendere function
 */
void render_value(GtkTreeViewColumn *tree_column,
	 GtkCellRenderer *cell,
	 GtkTreeModel *tree_model,
	 GtkTreeIter *iter,
	 gpointer data)
{
	/* paint changed values in red */
	gboolean changed = FALSE;
	gtk_tree_model_get (
		tree_model,
		iter,
		W_CHANGED, &changed,
		-1);
	g_object_set (cell, "foreground", changed ? "red" : "black", NULL);
}

/*
 * create variables tree view widget
 * arguments:
 * 		on_render_name - custom name column renderer function
 * 		on_expression_changed - callback to call on expression changed
 */
GtkWidget* vtree_create(watch_render_name on_render_name, watch_expression_changed on_expression_changed)
{
	/* create tree view */
	GtkTreeStore* store = gtk_tree_store_new (
		W_N_COLUMNS,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_INT);
	GtkWidget* tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
		
	/* set tree view parameters */
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_grid_lines (GTK_TREE_VIEW(tree), GTK_TREE_VIEW_GRID_LINES_VERTICAL);
	g_object_set(tree, "rules-hint", TRUE, NULL);

	/* connect signals */
	g_signal_connect(G_OBJECT(tree), "row-collapsed", G_CALLBACK (on_row_collapsed), NULL);
	g_signal_connect(G_OBJECT(tree), "key-press-event", G_CALLBACK (on_key_pressed), NULL);

	/* create columns */
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	gchar				*header;
	
	int	char_width = get_char_width(tree);

	/* Name */
	header = _("Name");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, FALSE,
		get_header_string_width(header, MW_NAME, char_width),
		"text", W_NAME);
	if (on_render_name)
		gtk_tree_view_column_set_cell_data_func(column, renderer, on_render_name, NULL, NULL);
	if (on_expression_changed)
	{
		g_object_set (renderer, "editable", TRUE, NULL);
		g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (on_expression_changed), NULL);
	}
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* Value */
	header = _("Value");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, TRUE,
		get_header_string_width(header, MW_VALUE, char_width),
		"text", W_VALUE);
	gtk_tree_view_column_set_cell_data_func(column, renderer, render_value, NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* Type */
	header = _("Type");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, FALSE,
		get_header_string_width(header, MW_TYPE, char_width),
		"text", W_TYPE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* Internal (unvisible column) */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Internal", renderer, "text", W_INTERNAL, NULL);
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	
	/* Path expression (unvisible column) */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Expression", renderer, "text", W_EXPRESSION, NULL);
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* STUB (unvisible column) */
	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes ("Need Update", renderer, "active", W_STUB, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	gtk_tree_view_column_set_visible(column, FALSE);

	/* Changed (unvisible column) */
	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes ("Changed", renderer, "active", W_CHANGED, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	gtk_tree_view_column_set_visible(column, FALSE);

	return tree;
}
