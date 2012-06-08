/*
 *		vtree.c
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
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
 * 		Base class for autos and watch tree views.
 */

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "vtree.h"
#include "breakpoint.h"
#include "debug_module.h"
#include "watch_model.h"
#include "utils.h"
#include "pixbuf.h"


/* columns minumum width in characters */
#define MIN_COLUMN_CHARS 20

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
void render_icon(GtkTreeViewColumn *tree_column,
	 GtkCellRenderer *cell,
	 GtkTreeModel *tree_model,
	 GtkTreeIter *iter,
	 gpointer data)
{
	variable_type vt;
	gtk_tree_model_get (
		tree_model,
		iter,
		W_VT, &vt,
		-1);

	if (VT_NONE != vt && VT_CHILD != vt)
	{
		GdkPixbuf *pixbuf = NULL;

		g_object_set(cell, "visible", TRUE, NULL);

		if (VT_ARGUMENT == vt)
			pixbuf = argument_pixbuf;
		else if (VT_LOCAL == vt)
			pixbuf = local_pixbuf;
		else if (VT_WATCH == vt)
			pixbuf = watch_pixbuf;

		g_object_set (cell, "pixbuf", (gpointer)pixbuf, NULL);
	}
	else
	{
		g_object_set(cell, "visible", FALSE, NULL);
	}
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
	GtkCellRenderer *renderer;
	GtkCellRenderer *icon_renderer;
	GtkTreeViewColumn *column;
	GtkTreeStore* store = gtk_tree_store_new (
		W_N_COLUMNS,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_INT);
	GtkWidget* tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
	g_object_unref(store);
		
	/* set tree view parameters */
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(tree), 10);

	/* connect signals */
	if (NULL != on_key_pressed)
	{
		g_signal_connect(G_OBJECT(tree), "key-press-event", G_CALLBACK (on_key_pressed), NULL);
	}

	/* create columns */
	
	/* Name */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Name"));
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "text", W_NAME, NULL);	
	
	icon_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set(icon_renderer, "follow-state", TRUE, NULL);
	gtk_tree_view_column_pack_end(column, icon_renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func(column, icon_renderer, render_icon, NULL, NULL);

	gtk_tree_view_column_set_resizable (column, TRUE);
	if (on_render_name)
		gtk_tree_view_column_set_cell_data_func(column, renderer, on_render_name, NULL, NULL);
	if (on_expression_changed)
	{
		g_object_set (renderer, "editable", TRUE, NULL);
		g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (on_expression_changed), NULL);
	}
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* Value */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Value"), renderer, "text", W_VALUE, NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer, render_value, NULL, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* Type */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer, "text", W_TYPE, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* Last invisible column */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", W_LAST_VISIBLE, NULL);
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
