/*
 *		stree.c
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
 *		Contains function to manipulate stack trace tree view.
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "geanyplugin.h"

#include "breakpoints.h"
#include "utils.h"
#include "breakpoint.h"
#include "debug_module.h"

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyPlugin		*geany_plugin;

/* columns minumum width in characters */
#define MW_ADRESS	10
#define MW_FUNCTION	10
#define MW_FILE		0
#define MW_LINE		4

/* Tree view columns */
enum
{
   S_ADRESS,
   S_FUNCTION,
   S_FILEPATH,
   S_LINE,
   S_N_COLUMNS
};

/* hash table to remember whether source file fo stack frame exists */
GHashTable* frames = NULL;

/* double click callback pointer */
static move_to_line_cb callback = NULL;

/* tree view, model and store handles */
static GtkWidget* tree = NULL;
static GtkTreeModel* model = NULL;
static GtkListStore* store = NULL;

/* flag to indicate whether to handle selection change */
static gboolean handle_selection = TRUE;

/*
 *  Tree view selection changed callback
 */
void on_selection_changed(GtkTreeSelection *treeselection, gpointer user_data)
{
	if (handle_selection)
	{
		GList *rows = gtk_tree_selection_get_selected_rows(treeselection, &model);
		GtkTreePath *path = (GtkTreePath*)rows->data;
		
		GtkTreeIter iter;
		gboolean res = gtk_tree_model_get_iter (
			 gtk_tree_view_get_model(GTK_TREE_VIEW(tree)),
			 &iter,
			 path);
		gchar *file;
		int line;
		gtk_tree_model_get (
			gtk_tree_view_get_model(GTK_TREE_VIEW(tree)),
			&iter,
			S_FILEPATH, &file,
			S_LINE, &line,
			-1);
		
		/* check if file name is not empty and we have source files for the frame */
		if (strlen(file) && g_hash_table_lookup(frames, (gpointer)file))
			callback(file, line);
		
		g_free(file);

		gtk_tree_path_free(path);
		g_list_free(rows);
	}
}

/*
 *	inits stack trace tree
 *	arguments:
 * 		cb - callback to call on double click	
 */
gboolean stree_init(move_to_line_cb cb)
{
	callback = cb;

	/* create tree view */
	store = gtk_list_store_new (
		S_N_COLUMNS,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT);
	model = GTK_TREE_MODEL(store);
	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
	
	/* set tree view properties */
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), 1);
	gtk_tree_view_set_grid_lines (GTK_TREE_VIEW(tree), GTK_TREE_VIEW_GRID_LINES_VERTICAL);
	g_object_set(tree, "rules-hint", TRUE, NULL);

	/* connect signals */
	g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree))), "changed",
	                 G_CALLBACK (on_selection_changed), NULL);

	/* creating columns */
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn	*column;
	gchar				*header;
	
	int	char_width = get_char_width(tree);

	/* adress */
	header = _("Address");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, FALSE,
		get_header_string_width(header, MW_ADRESS, char_width),
		"text", S_ADRESS);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* function */
	header = _("Function");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, FALSE,
		get_header_string_width(header, MW_FUNCTION, char_width),
		"text", S_FUNCTION);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	
	/* file */
	header = _("File");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, TRUE,
		get_header_string_width(header, MW_FILE, char_width),
		"text", S_FILEPATH);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	
	/* line */
	header = _("Line");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, FALSE,
		get_header_string_width(header, MW_LINE, char_width),
		"text", S_LINE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* create frames hash table */
	frames =  g_hash_table_new_full(
		g_str_hash,
		g_str_equal,
		(GDestroyNotify)g_free,
		NULL);
		
	return TRUE;
}

/*
 *	get stack trace tree view
 */
GtkWidget* stree_get_widget()
{
	return tree;
}

/*
 *	add frame to the tree view
 */
void stree_add(frame *f)
{
	GtkTreeIter iter;
	gtk_list_store_prepend (store, &iter);
	gtk_list_store_set (store, &iter,
                    S_ADRESS, f->address,
                    S_FUNCTION, f->function,
                    S_FILEPATH, f->file,
                    S_LINE, f->line,
                    -1);
    
	/* remember if we have source for this frame */
    if (f->have_source && !g_hash_table_lookup(frames, (gpointer)f->file))
		g_hash_table_insert(frames, g_strdup(f->file), (gpointer)f->have_source);
}

/*
 *	clear tree view
 */
void stree_clear()
{
	handle_selection = FALSE;
	
	gtk_list_store_clear(store);
	g_hash_table_remove_all(frames);

	handle_selection = TRUE;
}

/*
 *	select first item in tree view
 */
void stree_select_first()
{
	GtkTreePath* path = gtk_tree_path_new_first();
	
	gtk_tree_selection_select_path (
		gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)),
		path);
	
	gtk_tree_path_free(path);
}
