/*
 *      bptree.c
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
 * 		Contains breakpoints GtkTreeView manipulating functions 
 * 		and handlers for tree view events.
 * 		Handlers only collect data from the tree and call
 * 		corresponding breaks_set_.... functions, which call
 * 		bptree_set... if breakpoint has been changed altered/added/removed
 */

#include <memory.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "geanyplugin.h"

#include "breakpoint.h"
#include "breakpoints.h"
#include "bptree.h"
#include "utils.h"

/* columns minumum width in characters */
#define MW_ENABLED		3
#define MW_HITSCOUNT	3
#define MW_CONDITION	20
#define MW_FILE			0
#define MW_LINE			4

/* Tree view columns */
enum
{
   ENABLED,
   HITSCOUNT,
   CONDITION,
   FILEPATH,
   LINE,
   N_COLUMNS
};

/* tree view and store handles */
static GtkWidget		*tree = NULL;
static GtkTreeModel		*model = NULL;
static GtkListStore		*store = NULL;

/* column cell renderes */
static GtkCellRenderer	*enable_renderer;
static GtkCellRenderer	*hcount_renderer;
static GtkCellRenderer	*condition_renderer;

/* tells to checkbox click handler whether page is in readonly mode (debug running) */
static gboolean readonly = FALSE;

/* callback handler */
move_to_line_cb on_break_clicked = NULL;

/* 
 * GtkTreeView event handlers
 */

/*
 * double click
 */
void on_row_double_click(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeIter iter;
	gboolean res = gtk_tree_model_get_iter (
		 model,
		 &iter,
		 path);
    
    gchar *file;
	int line;
	gtk_tree_model_get (
		model,
		&iter,
		FILEPATH, &file,
		LINE, &line,
        -1);
        
    /* use callback, supplied in bptree_init */
	on_break_clicked(file, line);
	
	g_free(file);
}

/*
 * editing "condition" column value finished
 */
void on_condition_changed(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data)
{
	GtkTreeIter  iter;
    GtkTreePath *tree_path = gtk_tree_path_new_from_string (path);

	gboolean res = gtk_tree_model_get_iter (
		 model,
		 &iter,
		 tree_path);
	
    gchar *file;
	int line;
	gchar* oldcondition;
	gtk_tree_model_get (
		model,
		&iter,
		CONDITION, &oldcondition,
		FILEPATH, &file,
		LINE, &line,
        -1);
        
    if (strcmp(oldcondition, new_text))
		breaks_set_condition(file, line, new_text);
	
	gtk_tree_path_free(tree_path);
	g_free(file);
	g_free(oldcondition);
}

/*
 * editing "hitscount" column value finished
 */
void on_hitscount_changed(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data)
{
	int count = atoi(new_text);
	if (!count && strcmp(new_text, "0"))
		return;

	GtkTreeIter  iter;
    GtkTreePath *tree_path = gtk_tree_path_new_from_string (path);

	gboolean res = gtk_tree_model_get_iter (
		 model,
		 &iter,
		 tree_path);

    gchar *file;
	int line;
	gint oldcount;
	gtk_tree_model_get (
		model,
		&iter,
		HITSCOUNT, &oldcount,
		FILEPATH, &file,
		LINE, &line,
        -1);
        
    if (oldcount != count)
    	breaks_set_hits_count(file, line, count);
	
	gtk_tree_path_free(tree_path);
	g_free(file);
}                                                        

/*
 * "Enabled" checkbox has been clicked
 */
void on_activeness_changed(GtkCellRendererToggle *cell_renderer, gchar *path, gpointer user_data)
{
	/* do not process event is page is readonly (debug is running) */
	if (readonly)
		return;
	
	GtkTreeIter  iter;
    GtkTreePath *tree_path = gtk_tree_path_new_from_string (path);

	gboolean res = gtk_tree_model_get_iter (
		 model,
		 &iter,
		 tree_path);

    gchar *file;
	int line;
	gtk_tree_model_get (
		model,
		&iter,
		FILEPATH, &file,
		LINE, &line,
        -1);
        
	breaks_switch(file, line);
	
	gtk_tree_path_free(tree_path);
	g_free(file);
}

/*
 * key pressed event
 */
static gboolean on_key_pressed(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    guint keyval = ((GdkEventKey*)event)->keyval;

	if (keyval == GDK_Delete)
	{
		/* "delete selected rows" */

		/* path to select after deleteing finishes */
		GtkTreeRowReference *reference_to_select = NULL;

		/* get selected rows */
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		GList *rows = gtk_tree_selection_get_selected_rows(selection, &model);
		
		/* get references to rows to keep references actual when altering model */
		GList *references = NULL;
		GList *iter = rows;
		while (iter)
		{
			GtkTreePath *path = (GtkTreePath*)iter->data;
			references = g_list_append(references, gtk_tree_row_reference_new(model, path));

			/*
			 * set reference to select after deletion - upper sibling of the
			 * first row to delete that has upper sibling
			 */
			if (!reference_to_select && gtk_tree_path_prev(path))
				reference_to_select = gtk_tree_row_reference_new(model, path);
				
			iter = iter->next;
		}

		/* free rows list */
		g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (rows);

		/* iterate through references removing them */
		iter = references;
		while (iter)
		{
			GtkTreeRowReference *reference = (GtkTreeRowReference*)iter->data;
			if (gtk_tree_row_reference_valid(reference))
			{
				GtkTreePath *path = gtk_tree_row_reference_get_path(reference);
				if (1 == gtk_tree_path_get_depth(path))
				{
					GtkTreeIter titer;
					gtk_tree_model_get_iter(model, &titer, path);
					
					gchar *filename = NULL;
					gint line;
					
					gtk_tree_model_get (
						model,
						&titer,
						FILEPATH, &filename,
						LINE, &line,
						-1);

					breaks_remove(filename, line);
					
					g_free(filename);
				}
			}
			
			iter = iter->next;
		}

		/* get path to select */
		GtkTreePath *path = NULL;
		if (reference_to_select)
			path = gtk_tree_row_reference_get_path(reference_to_select);
		else
		{
			GtkTreeIter iter;
			gtk_tree_model_get_iter_first(model, &iter);
			path = gtk_tree_model_get_path(model, &iter);
		}
		
		/* set selection if any */
		if (path)
		{
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(widget), path, NULL, TRUE, 0.5, 0.5);
			gtk_tree_path_free(path);	
		}

		/* free references list */
		g_list_foreach (references, (GFunc)gtk_tree_row_reference_free, NULL);
		g_list_free (references);
	}


	return FALSE;
}

/*
 * Interface functions
 */

/*
 * init breaks tree view and return it if succesfull
 * arguments:
 * 		cb - callback to call on treeview double click
 */
gboolean bptree_init(move_to_line_cb cb)
{
	/* save double click callback */
	on_break_clicked = cb;
	
	/* create tree view */
	store = gtk_list_store_new (
		N_COLUMNS,
		G_TYPE_BOOLEAN,
		G_TYPE_INT,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT);
	model = GTK_TREE_MODEL(store);
	tree = gtk_tree_view_new_with_model (model);
	
	/* set tree view properties */
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), 1);
	gtk_tree_view_set_grid_lines (GTK_TREE_VIEW(tree), GTK_TREE_VIEW_GRID_LINES_VERTICAL);
	/* multiple selection */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	/* interlaced rows colors */
	g_object_set(tree, "rules-hint", TRUE, NULL);
	
	/* connect signals */
	g_signal_connect(G_OBJECT(tree), "key-press-event", G_CALLBACK (on_key_pressed), NULL);
	g_signal_connect(G_OBJECT(tree), "row-activated", G_CALLBACK (on_row_double_click), NULL);

	/* creating columns */
	GtkTreeViewColumn	*column;
	gchar				*header;
	
	int	char_width = get_char_width(tree);

	/* enabled */
	header = _("Enabled");
	enable_renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (G_OBJECT(enable_renderer), "toggled", G_CALLBACK(on_activeness_changed), NULL);
	column = create_column(header, enable_renderer, FALSE,
		get_header_string_width(header, MW_ENABLED, char_width),
		 "active", ENABLED);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	/* hits count */
	header = _("Hit count");
	hcount_renderer = gtk_cell_renderer_spin_new ();
    GtkAdjustment* adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 100000.0, 1.0, 2.0, 2.0));
    g_object_set (hcount_renderer,
		"editable", TRUE,
		"adjustment", adj,
        "digits", 0, NULL);
    g_signal_connect (G_OBJECT (hcount_renderer), "edited", G_CALLBACK (on_hitscount_changed), NULL);
	column = create_column(header, hcount_renderer, FALSE,
		get_header_string_width(header, MW_HITSCOUNT, char_width),
		"text", HITSCOUNT);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	/* condition */
	header = _("Condition");
	condition_renderer = gtk_cell_renderer_text_new ();
    g_object_set (condition_renderer, "editable", TRUE, NULL);
    g_signal_connect (G_OBJECT (condition_renderer), "edited", G_CALLBACK (on_condition_changed), NULL);
	column = create_column(header, condition_renderer, FALSE,
		get_header_string_width(header, MW_CONDITION, char_width),
		"text", CONDITION);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	/* file */
	header = _("File");
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, TRUE,
		get_header_string_width(header, MW_FILE, char_width),
		"text", FILEPATH);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	/* line */
	header = _("Line");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, FALSE,
		get_header_string_width(header, MW_LINE, char_width),
		"text", LINE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	return TRUE;
}

/*
 * enable/disable break
 * arguments:
 * 		iter 	- tree view iterator
 * 		enabled - value
 */
void bptree_set_enabled(GtkTreeIter iter, gboolean enabled)
{
  gtk_list_store_set(store, &iter, ENABLED, enabled, -1);
}

/*
 * set breaks hits count
 * arguments:
 * 		iter 	- tree view iterator
 * 		hitscount - value
 */
void bptree_set_hitscount(GtkTreeIter iter, int hitscount)
{
  gtk_list_store_set(store, &iter, HITSCOUNT, hitscount, -1);
}

/*
 * set breaks condition
 * arguments:
 * 		iter 	- tree view iterator
 * 		condition - value
 */
void bptree_set_condition(GtkTreeIter iter, gchar* condition)
{
  gtk_list_store_set(store, &iter, CONDITION, condition, -1);
}

/*
 * get breaks condition
 * arguments:
 * 		iter 	- tree view iterator
 * return value	- breaks condition
 */
gchar* bptree_get_condition(GtkTreeIter iter)
{
    gchar *condition;
	gtk_tree_model_get (
		model,
		&iter,
		CONDITION, &condition,
        -1);

	return condition;
}

/*
 * set tree view accessible / inaccessible to user input 
 * arguments:
 * 		value - new value
 */
void bptree_set_readonly(gboolean value)
{
	readonly = value;
    g_object_set (hcount_renderer, "editable", !readonly, NULL);
    g_object_set (condition_renderer, "editable", !readonly, NULL);
}

/*
 * get tree view widget
 * return value	- tree view widget
 */
GtkWidget* bptree_get_widget()
{
	return tree;
}

/*
 * add new breakpoint to the tree view
 * arguments:
 * 		bp - breakpoint to add
 */
void bptree_add_breakpoint(breakpoint* bp)
{
	GtkTreeIter iter;
	gtk_list_store_prepend (store, &iter);
	gtk_list_store_set (store, &iter,
                    ENABLED, bp->enabled,
                    HITSCOUNT, bp->hitscount,
                    CONDITION, bp->condition,
                    FILEPATH, bp->file,
                    LINE, bp->line,
                    -1);
    bp->iter = iter;
}

/*
 * update existing breakpoint
 * arguments:
 * 		bp - breakpoint to update
 */
void bptree_update_breakpoint(breakpoint* bp)
{
	char file_and_line[FILENAME_MAX + 10];
	sprintf(file_and_line, "%s:%i", bp->file, bp->line);
	gtk_list_store_set (store, &bp->iter,
                    ENABLED, bp->enabled,
                    HITSCOUNT, bp->hitscount,
                    CONDITION, bp->condition,
                    FILEPATH, bp->file,
                    LINE, bp->line,
                    -1);
}

/*
 * remove breakpoint
 * arguments:
 * 		bp - breakpoint to revove
 */
void bptree_remove_breakpoint(breakpoint* bp)
{
	gtk_list_store_remove(store, &(bp->iter));	
}
