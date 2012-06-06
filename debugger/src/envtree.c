/*
 *      envtree.c
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
 * 		Working with the evironment variables tree view.
 */

#include <string.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

extern GeanyFunctions	*geany_functions;
extern GeanyPlugin		*geany_plugin;

#include <gdk/gdkkeysyms.h>

#include "dconfig.h"

/* environment variables tree view columns */
enum
{
   NAME,
   VALUE,
   LAST_VISIBLE,
   N_COLUMNS
};

static GtkListStore *store;
static GtkTreeModel *model;
static GtkWidget *tree;

/* flag shows that the page is readonly (debug is running) */
static gboolean page_read_only = FALSE;

/* flag shows we entered new env variable name and now entering its value */
static gboolean entering_new_var = FALSE;

/* reference to the env tree view empty row */
static GtkTreeRowReference *empty_row = NULL;

static GtkTreePath *being_edited_value = NULL;

/* env variable name cloumn */
static GtkTreeViewColumn *column_name = NULL;
static GtkCellRenderer *renderer_name = NULL;

/* env variable value cloumn */
static GtkTreeViewColumn *column_value = NULL;
static GtkCellRenderer *renderer_value = NULL;

/*
 * adds empty row to env tree view 
 */
static void add_empty_row(void)
{
	if (empty_row)
		gtk_tree_row_reference_free(empty_row);
	
	GtkTreeIter empty;
	gtk_list_store_append (store, &empty);
	gtk_list_store_set (store, &empty,
		NAME, "",
		VALUE, "",
		-1);

	/* remeber reference */
	GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &empty);
	empty_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(store), path);
	gtk_tree_path_free(path);
}

/*
 * delete selected rows from env variables page 
 */
static void delete_selected_rows(void)
{
	/* path to select after deleting finishes */
	GtkTreeRowReference *reference_to_select = NULL;

	/* empty row path */
	GtkTreePath *empty_path = gtk_tree_row_reference_get_path(empty_row);

	/* get selected rows */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	GList *rows = gtk_tree_selection_get_selected_rows(selection, &model);

	/* check whether only empty row was selected */
	if (1 != gtk_tree_selection_count_selected_rows(selection) ||
	    gtk_tree_path_compare((GtkTreePath*)rows->data, empty_path))
	{
		/* get references to the selected rows and find out what to
		select after deletion */
		GList *references = NULL;
		GList *iter = rows;
		while (iter)
		{
			GtkTreePath *path = (GtkTreePath*)iter->data;
			if (!reference_to_select)
			{
				/* select upper sibling of the upper
				selected row that has unselected upper sibling */
				GtkTreePath *sibling = gtk_tree_path_copy(path);
				if(gtk_tree_path_prev(sibling))
				{
					if (!gtk_tree_selection_path_is_selected(selection, sibling))
						reference_to_select = gtk_tree_row_reference_new(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)), sibling);
				}
				else if (gtk_tree_path_next(sibling), gtk_tree_path_compare(path, sibling))
					reference_to_select = gtk_tree_row_reference_new(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)), sibling);
			}
			
			if (gtk_tree_path_compare(path, empty_path))
				references = g_list_append(references, gtk_tree_row_reference_new(model, path));
			
			iter = iter->next;
		}
		
		/* if all (with or without empty row) was selected - set empty row
		as a path to be selected after deleting */
		if (!reference_to_select)
			reference_to_select = gtk_tree_row_reference_copy (empty_row);

		iter = references;
		while (iter)
		{
			GtkTreeRowReference *reference = (GtkTreeRowReference*)iter->data;
			GtkTreePath *path = gtk_tree_row_reference_get_path(reference);

			GtkTreeIter titer;
			gtk_tree_model_get_iter(model, &titer, path);
			gtk_list_store_remove(store, &titer);

			gtk_tree_path_free(path);
			iter = iter->next;
		}

		/* set selection */
		gtk_tree_selection_unselect_all(selection);
		GtkTreePath *path = gtk_tree_row_reference_get_path(reference_to_select);
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree), path, NULL, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path);

		/* free references list */
		g_list_foreach (references, (GFunc)gtk_tree_row_reference_free, NULL);
		g_list_free (references);
	}
	
	/* free selection reference */
	gtk_tree_row_reference_free(reference_to_select);

	gtk_tree_path_free(empty_path);

	/* free rows list */
	g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (rows);
}

/*
 * env tree key pressed handler 
 */
static gboolean on_envtree_keypressed(GtkWidget *widget, GdkEvent  *event, gpointer user_data)
{
	/* do not allow deleting while debugging */
	if(page_read_only)
		return FALSE;
	
	/* handling only Delete button pressing
	that means "delete selected rows" */
	guint keyval = ((GdkEventKey*)event)->keyval;
	
	if (GDK_Delete == keyval)
	{
		delete_selected_rows();
		config_set_debug_changed();
	}

	return GDK_Tab == keyval;
}

/*
 * env tree view value column render function 
 */
static void on_render_value(GtkTreeViewColumn *tree_column,
	 GtkCellRenderer *cell,
	 GtkTreeModel *tree_model,
	 GtkTreeIter *iter,
	 gpointer data)
{
	/* do not allow to edit value in read only mode */
	if (page_read_only)
	{
		g_object_set (cell, "editable", FALSE, NULL);
	}
	else
	{
		/* do not allow to edit value for empty row */
		GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
		GtkTreePath *empty_path = gtk_tree_row_reference_get_path(empty_row);
		gboolean empty = !gtk_tree_path_compare(path, empty_path);
		g_object_set (cell, "editable", entering_new_var || !empty, NULL);
		gtk_tree_path_free(path);
		gtk_tree_path_free(empty_path);
	}
}

/*
 * env tree view value changed handler 
 */
static void on_value_changed(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data)
{
	GtkTreeIter  iter;
	GtkTreePath *tree_path = gtk_tree_path_new_from_string (path);

    GtkTreePath *empty_path = gtk_tree_row_reference_get_path(empty_row);
	gboolean empty = !gtk_tree_path_compare(tree_path, empty_path);
	gtk_tree_path_free(empty_path);

	gtk_tree_model_get_iter (
		 model,
		 &iter,
		 tree_path);
	
	gchar *striped = g_strstrip(g_strdup(new_text));
	
	if (!strlen(striped))
	{
		/* if new value is empty string, if it's a new row - do nothig
		 otheerwise - offer to delete a variable */
		if (empty)
			gtk_list_store_set(store, &iter, NAME, "", -1);
		else
		{
			if (dialogs_show_question(_("Delete variable?")))
			{
				delete_selected_rows();
				config_set_debug_changed();

				gtk_widget_grab_focus(tree);
			}
		}
	}
	else
	{
		/* if old variable - change value, otherwise - add another empty row below */
		gchar* oldvalue;
		gtk_tree_model_get (
			model,
			&iter,
			VALUE, &oldvalue,
		   -1);

		if (strcmp(oldvalue, striped))
		{
			gtk_list_store_set(store, &iter, VALUE, striped, -1);
			if (empty)
				add_empty_row();
			
			g_object_set (renderer_value, "editable", FALSE, NULL);
			config_set_debug_changed();
		}
		
		g_free(oldvalue);
	}
	
	if (empty)
		entering_new_var = FALSE;

	gtk_tree_path_free(tree_path);
	g_free(striped);

	gtk_tree_path_free(being_edited_value);
}

/*
 * env tree view value editing started
 */
static void on_value_editing_started(GtkCellRenderer *renderer, GtkCellEditable *editable, gchar *path, gpointer user_data)
{
	being_edited_value = gtk_tree_path_new_from_string(path);
}

/*
 * env tree view value editing cancelled (Escape pressed)
 */
static void on_value_editing_cancelled(GtkCellRenderer *renderer, gpointer user_data)
{
	/* check whether escape was pressed when editing
	new variable value cell */
	GtkTreePath *empty_path = gtk_tree_row_reference_get_path(empty_row);
	if(!gtk_tree_path_compare(being_edited_value, empty_path))
	{
		/* if so - clear name sell */
		GtkTreeIter iter;
		gtk_tree_model_get_iter(model, &iter, being_edited_value);
		
		gtk_list_store_set (
			store,
			&iter,
			NAME, "",
		   -1);

		entering_new_var = FALSE;
	}

	g_object_set (renderer_value, "editable", FALSE, NULL);

	gtk_tree_path_free(being_edited_value);
	gtk_tree_path_free(empty_path);
}

/*
 * env tree view name changed hadler 
 */
static void on_name_changed(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data)
{
	GtkTreeIter  iter;
	GtkTreePath *tree_path = gtk_tree_path_new_from_string (path);
	GtkTreePath *empty_path = gtk_tree_row_reference_get_path(empty_row);
    
	gboolean empty = !gtk_tree_path_compare(tree_path, empty_path);

	gtk_tree_model_get_iter (
		 model,
		 &iter,
		 tree_path);
	
	gchar* oldvalue;
	gtk_tree_model_get (
		model,
		&iter,
		NAME, &oldvalue,
       -1);
    
	gchar *striped = g_strstrip(g_strdup(new_text));

	if (!strlen(striped))
	{
		/* if name is empty - offer to delete variable */
		if (!empty && dialogs_show_question(_("Delete variable?")))
		{
			delete_selected_rows();
			config_set_debug_changed();

			gtk_widget_grab_focus(tree);
		}
	}
	else if (strcmp(oldvalue, striped))
    {
		gtk_list_store_set(store, &iter, NAME, striped, -1);
		if (empty)
		{
			/* if it was a new row - move cursor to a value cell */
			entering_new_var = TRUE;
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(tree), tree_path, column_value, renderer_value, TRUE);
		}
		if (!empty)
		{
			config_set_debug_changed();
		}
	}
	
	gtk_tree_path_free(tree_path);
	gtk_tree_path_free(empty_path);
	g_free(oldvalue);
	g_free(striped);
}

/*
 * create env tree view and return a widget 
 */
GtkWidget* envtree_init(void)
{
	store = gtk_list_store_new (
		N_COLUMNS,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING);
	model = GTK_TREE_MODEL(store);
	tree = gtk_tree_view_new_with_model (model);
	g_object_unref(store);
	
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);
	g_signal_connect(G_OBJECT(tree), "key-press-event", G_CALLBACK (on_envtree_keypressed), NULL);

	renderer_name = gtk_cell_renderer_text_new ();
	g_object_set (renderer_name, "editable", TRUE, NULL);
	g_signal_connect (G_OBJECT (renderer_name), "edited", G_CALLBACK (on_name_changed), NULL);
	column_name = gtk_tree_view_column_new_with_attributes (_("Name"), renderer_name, "text", NAME, NULL);
	gtk_tree_view_column_set_resizable (column_name, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column_name);

	renderer_value = gtk_cell_renderer_text_new ();
	column_value = gtk_tree_view_column_new_with_attributes (_("Value"), renderer_value, "text", VALUE, NULL);
	g_signal_connect (G_OBJECT (renderer_value), "edited", G_CALLBACK (on_value_changed), NULL);
	g_signal_connect (G_OBJECT (renderer_value), "editing-started", G_CALLBACK (on_value_editing_started), NULL);
	g_signal_connect (G_OBJECT (renderer_value), "editing-canceled", G_CALLBACK (on_value_editing_cancelled), NULL);
	gtk_tree_view_column_set_cell_data_func(column_value, renderer_value, on_render_value, NULL, NULL);
	gtk_tree_view_column_set_resizable (column_value, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column_value);

	/* Last invisible column */
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", LAST_VISIBLE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	/* add empty row */
	add_empty_row();

	/* set multiple selection */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	return tree;
}

/*
 * deallocate data 
 */
void envtree_destroy(void)
{
	gtk_tree_row_reference_free(empty_row);
}

/*
 * clear tree 
 */
void envtree_clear(void)
{
	gtk_list_store_clear(store);
	add_empty_row();
}

/*
 * get list of environment variables
 */
GList* envpage_get_environment(void)
{
	GList *env = NULL;
	
	GtkTreeIter iter;
	gtk_tree_model_get_iter_first(model, &iter);
	do
	{
		gchar *name, *value;
		gtk_tree_model_get (
			model,
			&iter,
			NAME, &name,
			VALUE, &value,
		   -1);

		 if (strlen(name))
		 {
			env = g_list_append(env, name);
			env = g_list_append(env, value);
		 }
	}
	while (gtk_tree_model_iter_next(model, &iter));
	
	return env;
}

/*
 * disable interacting for the time of debugging
 */
void envtree_set_readonly(gboolean readonly)
{
	g_object_set (renderer_name, "editable", !readonly, NULL);
	page_read_only = readonly;
}

/*
 * add environment variable
 */
void envtree_add_environment(const gchar *name, const gchar *value)
{
	GtkTreeIter iter;
	gtk_list_store_prepend(store, &iter);
	gtk_list_store_set(store, &iter, NAME, name, VALUE, value, -1);
}
