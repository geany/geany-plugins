/*
 *		wtree.c
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
 *		Watch tree view.
 */


#include <gtk/gtk.h>

#include "watch_model.h"
#include "vtree.h"

/* drag types */
enum
{
  TARGET_STRING,
};
static GtkTargetEntry targetentries[] =
{
  { "STRING",        0, TARGET_STRING }
};

/* reference to an empty row */
static GtkTreeRowReference *empty_row = NULL;

/* tree view widget */
static GtkWidget *tree = NULL;
static GtkTreeModel *model = NULL;
static GtkTreeStore *store = NULL;

/*
 * add empty row
 */
static void add_empty_row()
{
	if (empty_row)
		gtk_tree_row_reference_free(empty_row);
	
	GtkTreeIter empty;
	gtk_tree_store_prepend (store, &empty, NULL);
	gtk_tree_store_set (store, &empty,
		W_NAME, "",
		W_VALUE, "",
		W_TYPE, "",
		W_INTERNAL, "",
		W_EXPRESSION, "",
		W_VALUE, "",
		-1);

	/* save reference */
	GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &empty);
	empty_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(store), path);
	gtk_tree_path_free(path);
}

/*
 * name column renderer
 */
void on_render_name(GtkTreeViewColumn *tree_column,
	 GtkCellRenderer *cell,
	 GtkTreeModel *tree_model,
	 GtkTreeIter *iter,
	 gpointer data)
{
	/* sets editable to true only for root items */
	GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
	g_object_set (cell, "editable", gtk_tree_path_get_depth(path) <= 1, NULL);
	gtk_tree_path_free(path);
}

/*
 * get iterator to an empty row
 */
GtkTreeIter wtree_empty_row()
{
	GtkTreeIter empty;
	
	gtk_tree_model_get_iter(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)),
		&empty,
		gtk_tree_row_reference_get_path(empty_row));
	
	return empty;
}

/*
 * get an empty row path
 */
GtkTreePath* wtree_empty_path()
{
	return gtk_tree_row_reference_get_path(empty_row);
}

/*
 * iterating function to collect all watches
 */
gboolean watches_foreach_collect(GtkTreeModel *model, 
	GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer data)
{
	if (gtk_tree_path_compare(path, wtree_empty_path()) &&  1 == gtk_tree_path_get_depth(path))
	{
		gchar *watch;
		gtk_tree_model_get (
			model,
			iter,
			W_NAME, &watch,
			-1);
		
		GList **watches = (GList**)data;
		*watches = g_list_append(*watches, watch);
	}
    
    return FALSE;
}

/*
 * init watches tree view
 */
GtkWidget* wtree_init(watch_expanded_callback expanded,
	new_watch_dragged dragged,
	watch_key_pressed keypressed,
	watch_expression_changed changed,
	watch_button_pressed buttonpressed)
{
	tree = vtree_create(on_render_name, changed);
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
	store = GTK_TREE_STORE(model);
	
	gtk_tree_view_enable_model_drag_dest(
		GTK_TREE_VIEW(tree),
		targetentries,
		1,
		GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
		
	g_signal_connect(G_OBJECT(tree), "row-expanded", G_CALLBACK (expanded), NULL);
	g_signal_connect(G_OBJECT(tree), "drag_data_received", G_CALLBACK(dragged), NULL);
	g_signal_connect(G_OBJECT(tree), "key-press-event", G_CALLBACK (keypressed), NULL);
    g_signal_connect(G_OBJECT(tree), "button-press-event", G_CALLBACK (buttonpressed) , NULL);
		
	/* add empty row */
	add_empty_row();

	/* set multiple selection */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	return tree;
}

/*
 * get watches list
 */
GList *wtree_get_watches()
{
	GList *watches = NULL;
	gtk_tree_model_foreach(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)), watches_foreach_collect, &watches);
	
	return watches;
}

/*
 * remove all watches from the tree view
 */
void wtree_remove_all()
{
	gtk_tree_store_clear(store);
	add_empty_row();
}

/*
 * add new watche to the tree view
 */
void wtree_add_watch(gchar *watch)
{
	GtkTreeIter newvar;
	GtkTreeIter empty = wtree_empty_row();
	gtk_tree_store_insert_before(store, &newvar, NULL, &empty);

	variable_set_name_only(store, &newvar, watch);
}
