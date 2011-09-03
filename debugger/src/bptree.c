/*
 *      bptree.c
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
 * 		Contains breakpoints GtkTreeView manipulating functions 
 * 		and handlers for tree view events.
 * 		Handlers only collect data from the tree and call
 * 		corresponding breaks_set_.... functions, which call
 * 		bptree_set... if breakpoint has been changed altered/added/removed
 */

#include <stdlib.h>
#include <memory.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "geanyplugin.h"

#include "breakpoint.h"
#include "breakpoints.h"
#include "bptree.h"
#include "utils.h"
#include "dconfig.h"

#include "xpm/breakpoint.xpm"
#include "xpm/breakpoint_disabled.xpm"
#include "xpm/breakpoint_condition.xpm"

/* columns minumum width in characters */
#define MW_CONDITION	20
#define MW_FILE			20

#define ICON_PADDING	10

/* Tree view columns */
enum
{
   ICON,
   FILEPATH,
   CONDITION,
   HITSCOUNT,
   LINE,
   ENABLED,
   N_COLUMNS
};

/* tree view and store handles */
static GtkWidget		*scrolled_window = NULL;
static GtkWidget		*tree = NULL;
static GtkTreeModel		*model = NULL;
static GtkTreeStore		*store = NULL;

/* column cell renderes */
static GtkCellRenderer	*enable_renderer;
static GtkCellRenderer	*hcount_renderer;
static GtkCellRenderer	*condition_renderer;

/* tells to checkbox click handler whether page is in readonly mode (debug running) */
static gboolean readonly = FALSE;

/* hash table to keep file nodes in the tree */
static GHashTable *files;

/* pixbufers */
GdkPixbuf *file_pixbuf = NULL;
GdkPixbuf *break_pixbuf = NULL;
GdkPixbuf *break_disabled_pixbuf = NULL;
GdkPixbuf *break_condition_pixbuf = NULL;

/* callback handler */
move_to_line_cb on_break_clicked = NULL;

/* 
 * traversed comparing functions
 */
static gboolean compare_path_references(gpointer key, gpointer value, gpointer user_data)
{
	GtkTreeRowReference *being_clicked = (GtkTreeRowReference*)user_data;
	GtkTreeRowReference *compared = (GtkTreeRowReference*)value;
	
	return !gtk_tree_path_compare (
		gtk_tree_row_reference_get_path(being_clicked),
		gtk_tree_row_reference_get_path(compared)
	);
		
}

/* 
 * checks file ENABLED column if all childs are enabled and unchecks otherwise
 */
static void update_file_node(GtkTreeIter *file_iter)
{
	GtkTreeIter child;
	gboolean check = TRUE;
	if(gtk_tree_model_iter_children(model, &child, file_iter))
	{
		do
		{
			gboolean enabled;
			gtk_tree_model_get (
				model,
				&child,
				ENABLED, &enabled,
				-1);
				
			if (!enabled)
			{
				check = FALSE;
				break;
			}
		}
		while(gtk_tree_model_iter_next(model, &child));
	}

	gtk_tree_store_set(store, file_iter, ENABLED, check, -1);
}

/* 
 * GtkTreeView cell renderer functions that do not render and make ineditable file rows
 */
void on_render(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model,
	GtkTreeIter *iter, gpointer data)
{
	GtkTreePath *path = gtk_tree_model_get_path(model, iter);
	if (data)
	{
		if (gtk_tree_path_get_depth(path) == 1)
		{
			g_object_set(cell, "text", "", NULL);
			g_object_set(cell, "editable", FALSE, NULL);
		}
		else
		{
			g_object_set(cell, "editable", TRUE, NULL);
		}
	}
	else
	{
		if (gtk_tree_path_get_depth(path) == 1)
		{
			g_object_set(cell, "visible", FALSE, NULL);
		}
		else
		{
			g_object_set(cell, "visible", TRUE, NULL);
		}
	}
	gtk_tree_path_free(path);
}

/* 
 * GtkTreeView event handlers
 */

/*
 * double click
 */
static void on_row_double_click(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	if (1 == gtk_tree_path_get_depth(path))
	{
		return;
	}
	
	GtkTreeIter iter;
	gtk_tree_model_get_iter (
		 model,
		 &iter,
		 path);
    
    gchar *file;
	int line;
	
	GtkTreeIter parent_iter;
	gtk_tree_model_iter_parent(model, &parent_iter, &iter);
	
	gtk_tree_model_get (
		model,
		&parent_iter,
		FILEPATH, &file,
        -1);

	gtk_tree_model_get (
		model,
		&iter,
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

	gtk_tree_model_get_iter (
		 model,
		 &iter,
		 tree_path);
	
	GtkTreeIter parent_iter;
	gtk_tree_model_iter_parent(model, &parent_iter, &iter);

	gchar *file;
	gtk_tree_model_get (
		model,
		&parent_iter,
		FILEPATH, &file,
		-1);
	
	int line;
	gchar* oldcondition;
	gtk_tree_model_get (
		model,
		&iter,
		CONDITION, &oldcondition,
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

	gtk_tree_model_get_iter (
		 model,
		 &iter,
		 tree_path);

	GtkTreeIter parent_iter;
	gtk_tree_model_iter_parent(model, &parent_iter, &iter);

	gchar *file;
	gtk_tree_model_get (
		model,
		&parent_iter,
		FILEPATH, &file,
		-1);

	int line;
	gint oldcount;
	gtk_tree_model_get (
		model,
		&iter,
		HITSCOUNT, &oldcount,
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
	
	gtk_tree_model_get_iter (
		model,
		&iter,
		tree_path);
    
	gboolean current_state = gtk_cell_renderer_toggle_get_active(cell_renderer);

    /* check if this is a file row */
    GtkTreeRowReference *path_reference = gtk_tree_row_reference_new(model, tree_path);
    if(g_hash_table_find(files, compare_path_references, (gpointer)path_reference))
    {
		GtkTreeIter child;
		gtk_tree_model_iter_children(model, &child, &iter);

		gchar *file;
		gtk_tree_model_get (
			model,
			&iter,
			FILEPATH, &file,
			-1);
		
		dconfig_set_modifyable(FALSE);
		do
		{
			gboolean enabled;
			gtk_tree_model_get (
				model,
				&child,
				ENABLED, &enabled,
				-1);
			
			if (!current_state ^ enabled)
			{
				int line;
				gtk_tree_model_get (
					model,
					&child,
					LINE, &line,
					-1);
				breaks_switch(file, line);
			}
		}
		while(gtk_tree_model_iter_next(model, &child));

		gtk_tree_store_set(store, &iter, ENABLED, !current_state, -1);

		g_free(file);

		dconfig_set_modifyable(TRUE);
		dconfig_set_changed();
	}
	else
	{
		GtkTreeIter parent_iter;
		gtk_tree_model_iter_parent(model, &parent_iter, &iter);

		gchar *file;
		int line;
		gboolean parent_enabed;

		gtk_tree_model_get (
			model,
			&parent_iter,
			FILEPATH, &file,
			ENABLED, &parent_enabed,
			-1);

		gtk_tree_model_get (
			model,
			&iter,
			LINE, &line,
			-1);
			
		breaks_switch(file, line);
		
		if (current_state && parent_enabed)
		{
			gtk_tree_store_set(store, &parent_iter, ENABLED, FALSE, -1);
		}
		else if (!current_state && !parent_enabed)
		{
			update_file_node(&parent_iter);
		}
		
		g_free(file);
	}

	gtk_tree_path_free(tree_path);
}

/*
 * key pressed event
 */
static gboolean on_key_pressed(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    guint keyval = ((GdkEventKey*)event)->keyval;

	/* get selected rows */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	GList *rows = gtk_tree_selection_get_selected_rows(selection, &model);

	if (keyval == GDK_Delete && rows && g_list_length(rows))
	{
		/* "delete selected rows" */

		/* path to select after deleteing finishes */
		GtkTreeRowReference *reference_to_select = NULL;

		/* get references to rows to keep references actual when altering model */
		GList *references = NULL;
		GList *iter = rows;
		while (iter)
		{
			GtkTreePath *path = (GtkTreePath*)iter->data;
			references = g_list_append(references, gtk_tree_row_reference_new(model, path));
			iter = iter->next;
		}
		
		/* remove break rows leaving file rowa for the next step */
		dconfig_set_modifyable(FALSE);
		iter = references;
		while (iter)
		{
			GtkTreeRowReference *reference = (GtkTreeRowReference*)iter->data;
			if (gtk_tree_row_reference_valid(reference))
			{
				GtkTreePath *path = gtk_tree_row_reference_get_path(reference);
				if (gtk_tree_path_get_depth(path) > 1)
				{
					GtkTreeIter titer, piter;
					gtk_tree_model_get_iter(model, &titer, path);
					gtk_tree_model_iter_parent(model, &piter, &titer);
					
					gchar *filename = NULL;
					gtk_tree_model_get (
						model,
						&piter,
						FILEPATH, &filename,
						-1);

					gint line;
					gtk_tree_model_get (
						model,
						&titer,
						LINE, &line,
						-1);

					breaks_remove(filename, line);
					
					g_free(filename);
				}
			}
			
			iter = iter->next;
		}
		
		/* remove file rows and children if exist */
		iter = references;
		while (iter)
		{
			GtkTreePath *path = (GtkTreePath*)iter->data;
			if (FALSE)//1 == gtk_tree_path_get_depth(path))
			{
			}
			iter = iter->next;
		}

		/* free rows list */
		g_list_foreach (references, (GFunc)gtk_tree_row_reference_free, NULL);
		g_list_free (references);

		dconfig_set_modifyable(TRUE);
		dconfig_set_changed(TRUE);

		/* get path to select */
		/*GtkTreePath *path = NULL;
		if (reference_to_select)
			path = gtk_tree_row_reference_get_path(reference_to_select);
		else
		{
			GtkTreeIter tree_iter;
			gtk_tree_model_get_iter_first(model, &tree_iter);
			path = gtk_tree_model_get_path(model, &tree_iter);
		}*/
		
		/* set selection if any */
		/*if (path)
		{
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(widget), path, NULL, TRUE, 0.5, 0.5);
			gtk_tree_path_free(path);	
		}*/

		/* free references list */
		g_list_foreach (references, (GFunc)gtk_tree_row_reference_free, NULL);
		g_list_free (references);
	}

	/* free rows list */
	g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (rows);

	return FALSE;
}

/*
 * update existing breakpoint icon
 * arguments:
 * 		bp - breakpoint to update
 */
void bptree_update_break_icon(breakpoint* bp)
{
	GdkPixbuf *pixbuf;
	if (!bp->enabled)
	{
		pixbuf = break_disabled_pixbuf;
	}
	else if (bp->hitscount || strlen(bp->condition))
	{
		pixbuf = break_condition_pixbuf;
	}
	else
	{
		pixbuf = break_pixbuf;
	}

	gtk_tree_store_set (store, &bp->iter, ICON, pixbuf, -1);
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
	
	/* crete hash table for file nodes */
	files = g_hash_table_new_full(
		g_str_hash,
		g_str_equal,
		(GDestroyNotify)g_free,
		(GDestroyNotify)gtk_tree_row_reference_free
	);
	
	file_pixbuf = gdk_pixbuf_new_from_xpm_data(breakpoint_xpm);
	break_pixbuf = gdk_pixbuf_new_from_xpm_data(breakpoint_xpm);
	break_disabled_pixbuf = gdk_pixbuf_new_from_xpm_data(breakpoint_disabled_xpm);
	break_condition_pixbuf = gdk_pixbuf_new_from_xpm_data(breakpoint_condition_xpm);

	/* create tree view */
	store = gtk_tree_store_new (
		N_COLUMNS,
		GDK_TYPE_PIXBUF,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_BOOLEAN);
	model = GTK_TREE_MODEL(store);
	tree = gtk_tree_view_new_with_model (model);
	
	/* set tree view properties */
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), 1);
	/* multiple selection */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	
	/* connect signals */
	g_signal_connect(G_OBJECT(tree), "key-press-event", G_CALLBACK (on_key_pressed), NULL);
	g_signal_connect(G_OBJECT(tree), "row-activated", G_CALLBACK (on_row_double_click), NULL);

	/* creating columns */
	GtkTreeViewColumn	*column;
	const gchar			*header;
	
	int	char_width = get_char_width(tree);

	/* icon, file */
	header = _("File");
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new();
	
	gtk_tree_view_column_pack_end(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "text", FILEPATH, NULL);	
	
	GtkCellRenderer *icon_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set(icon_renderer, "follow-state", TRUE, NULL);
	gtk_tree_view_column_pack_end(column, icon_renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, icon_renderer, "pixbuf", ICON, NULL);	
	gtk_tree_view_column_set_cell_data_func(column, icon_renderer, on_render, NULL, NULL);
	
	gtk_tree_view_column_set_spacing(column, char_width);
	gtk_tree_view_column_set_expand(column, TRUE);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* condition */
	header = _("Condition");
	condition_renderer = gtk_cell_renderer_text_new ();
    g_object_set (condition_renderer, "editable", TRUE, NULL);
    g_signal_connect (G_OBJECT (condition_renderer), "edited", G_CALLBACK (on_condition_changed), NULL);
	column = create_column(header, condition_renderer, TRUE,
		get_header_string_width(header, MW_CONDITION, char_width),
		"text", CONDITION);
	gtk_tree_view_column_set_cell_data_func(column, condition_renderer, on_render, (gpointer)TRUE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	
	/* hits count */
	header = _("Hit count");
	hcount_renderer = gtk_cell_renderer_spin_new ();
	GtkAdjustment* adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 100000.0, 1.0, 2.0, 2.0));
	g_object_set (hcount_renderer,
		"adjustment", adj,
        "digits", 0, NULL);
	g_signal_connect (G_OBJECT (hcount_renderer), "edited", G_CALLBACK (on_hitscount_changed), NULL);
	column = create_column(header, hcount_renderer, FALSE,
		0,
		"text", HITSCOUNT);
	gtk_tree_view_column_set_cell_data_func(column, hcount_renderer, on_render, (gpointer)TRUE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	
	/* line */
	header = _("Line");
	renderer = gtk_cell_renderer_text_new ();
	column = create_column(header, renderer, FALSE,
		0,
		"text", LINE);
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* enabled */
	header = _("Enabled");
	enable_renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (G_OBJECT(enable_renderer), "toggled", G_CALLBACK(on_activeness_changed), NULL);
	column = create_column(header, enable_renderer, FALSE,
		0,
		 "active", ENABLED);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolled_window);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_NONE);

	gtk_container_add (GTK_CONTAINER (scrolled_window), tree);

	return TRUE;
}

/*
 * destroy breaks tree and associated data
 * arguments:
 */
void bptree_destroy()
{
	g_hash_table_destroy(files);

	g_object_unref(file_pixbuf);
	g_object_unref(break_pixbuf);
	g_object_unref(break_disabled_pixbuf);
	g_object_unref(break_condition_pixbuf);
}

/*
 * enable/disable break
 * arguments:
 * 		bp 	- breakpoint
 */
void bptree_set_enabled(breakpoint *bp)
{
  gtk_tree_store_set(store, &(bp->iter), ENABLED, bp->enabled, -1);
  bptree_update_break_icon(bp);
}

/*
 * set breaks hits count
 * arguments:
 * 		bp 	- breakpoint
 */
void bptree_set_hitscount(breakpoint *bp)
{
  gtk_tree_store_set(store, &(bp->iter), HITSCOUNT, bp->hitscount, -1);
  bptree_update_break_icon(bp);
}

/*
 * set breaks condition
 * arguments:
 * 		bp 	- breakpoint
 */
void bptree_set_condition(breakpoint* bp)
{
  gtk_tree_store_set(store, &(bp->iter), CONDITION, bp->condition, -1);
  bptree_update_break_icon(bp);
}

/*
 * get breaks condition
 * arguments:
 * 		iter 	- tree view iterator
 * return value	- breaks condition
 */
gchar* bptree_get_condition(breakpoint *bp)
{
    gchar *condition;
	gtk_tree_model_get (
		model,
		&(bp->iter),
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
	return scrolled_window;
}

/*
 * add new breakpoint to the tree view
 * arguments:
 * 		bp - breakpoint to add
 */
void bptree_add_breakpoint(breakpoint* bp)
{
	GtkTreeIter file_iter;
	GtkTreeRowReference *file_reference = (GtkTreeRowReference*)g_hash_table_lookup(files, bp->file);
	if (!file_reference)
	{
		gtk_tree_store_prepend (store, &file_iter, NULL);
		gtk_tree_store_set (store, &file_iter,
						FILEPATH, bp->file,
						-1);

		GtkTreePath *file_path = gtk_tree_model_get_path(model, &file_iter);
		file_reference = gtk_tree_row_reference_new(model, file_path);
		gtk_tree_path_free(file_path);

		g_hash_table_insert(files, (gpointer)g_strdup(bp->file),(gpointer)file_reference);
	}
	else
	{
		gtk_tree_model_get_iter(model, &file_iter, gtk_tree_row_reference_get_path(file_reference));
	}
	
	/* lookup where to insert new row */
	GtkTreeIter child, *sibling = NULL;
	if(gtk_tree_model_iter_children(model, &child, &file_iter))
	{
		do
		{
			int line;
			gtk_tree_model_get (
				model,
				&child,
				LINE, &line,
				-1);
			if (line > bp->line)
			{
				sibling = &child;
				break;
			}
		}
		while(gtk_tree_model_iter_next(model, &child));
	}
	
	GtkTreeIter iter;
	gtk_tree_store_insert_before(store, &iter, &file_iter, sibling);
    bp->iter = iter;
    
    bptree_update_breakpoint(bp);
}

/*
 * update existing breakpoint
 * arguments:
 * 		bp - breakpoint to update
 */
void bptree_update_breakpoint(breakpoint* bp)
{
	gchar *location = g_strdup_printf(_("line %i"), bp->line);
	
	gtk_tree_store_set (store, &bp->iter,
                    ENABLED, bp->enabled,
                    HITSCOUNT, bp->hitscount,
                    CONDITION, bp->condition,
                    FILEPATH, location,
                    LINE, bp->line,
                    -1);
	
	bptree_update_break_icon(bp);

    g_free(location);
}

/*
 * remove breakpoint
 * arguments:
 * 		bp - breakpoint to revove
 */
void bptree_remove_breakpoint(breakpoint* bp)
{
	GtkTreeIter file;
	gtk_tree_model_iter_parent(model, &file, &(bp->iter));
	
	gtk_tree_store_remove(store, &(bp->iter));

	if (!gtk_tree_model_iter_n_children(model, &file))
	{
		gtk_tree_store_remove(store, &file);
	}
}

/*
 * updates all file ENABLED checkboxes base on theit children states
 * arguments:
 */
void bptree_update_file_nodes()
{
	GtkTreeIter file;
	if(gtk_tree_model_iter_children(model, &file, NULL))
	{
		do
		{
			update_file_node(&file);
		}
		while(gtk_tree_model_iter_next(model, &file));
	}
}
