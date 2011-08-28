/*
 *		tpage.c
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
 *		Contains function to manipulate target page in debug notebook.
 */

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <sys/stat.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

#include "breakpoint.h"
#include "breakpoints.h"
#include "utils.h"
#include "watch_model.h"
#include "wtree.h"
#include "debug.h"
#include "dconfig.h"
#include "tpage.h"

/* boxes margins */
#define SPACING 5

/* environment variables tree view columns minumum width in characters */
#define MW_NAME	15
#define MW_VALUE	0

/* environment variables tree view columns */
enum
{
   NAME,
   VALUE,
   N_COLUMNS
};

/* flag shows that the page is readonly (debug is running) */
gboolean page_read_only = FALSE;

/* flag shows we entered new env variable name and now entering its value */
gboolean entering_new_var = FALSE;

/* reference to the env tree view empty row */
static GtkTreeRowReference *empty_row = NULL;

/* page widget */
static GtkWidget *page =			NULL;

/* target name textentry */
static GtkWidget *targetname =		NULL;

/* browse target button */
static GtkWidget *button_browse =	NULL;

/* env variables tree view */
static GtkWidget *envtree =			NULL;
static GtkTreeModel *model =		NULL;
static GtkListStore *store =		NULL;

static GtkTreePath *being_edited_value = NULL;

/* debugger type combo box */
static GtkWidget *cmb_debugger =	NULL;

/* debugger messages text view */
GtkWidget *textview = NULL;

/* env variable name cloumn */
GtkTreeViewColumn *column_name = NULL;
GtkCellRenderer *renderer_name = NULL;

/* env variable value cloumn */
GtkTreeViewColumn *column_value = NULL;
GtkCellRenderer *renderer_value = NULL;

/*
 * tells config to update when target arguments change 
 */
void on_arguments_changed(GtkTextBuffer *textbuffer, gpointer user_data)
{
	dconfig_set_changed();
}

/*
 * tells config to update when target changes 
 */
void on_target_changed (GtkEditable *editable, gpointer user_data)
{
	dconfig_set_changed();
}

/*
 * delete selected rows from env variables page 
 */
void delete_selected_rows()
{
	/* path to select after deleting finishes */
	GtkTreeRowReference *reference_to_select = NULL;

	/* empty row path */
	GtkTreePath *empty_path = gtk_tree_row_reference_get_path(empty_row);

	/* get selected rows */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(envtree));
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
						reference_to_select = gtk_tree_row_reference_new(gtk_tree_view_get_model(GTK_TREE_VIEW(envtree)), sibling);
				}
				else if (gtk_tree_path_next(sibling), gtk_tree_path_compare(path, sibling))
					reference_to_select = gtk_tree_row_reference_new(gtk_tree_view_get_model(GTK_TREE_VIEW(envtree)), sibling);
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

			iter = iter->next;
		}

		/* set selection */
		gtk_tree_selection_unselect_all(selection);
		GtkTreePath *path = gtk_tree_row_reference_get_path(reference_to_select);
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(envtree), path, NULL, TRUE, 0.5, 0.5);

		/* free references list */
		g_list_foreach (references, (GFunc)gtk_tree_row_reference_free, NULL);
		g_list_free (references);
	}
	
	/* free selection reference */
	gtk_tree_row_reference_free(reference_to_select);

	/* free rows list */
	g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (rows);
}

/*
 * env tree key pressed handler 
 */
gboolean on_envtree_keypressed(GtkWidget *widget, GdkEvent  *event, gpointer user_data)
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
		dconfig_set_changed();
	}

	return GDK_Tab == keyval;
}

/*
 * adds empty row to env tree view 
 */
static void add_empty_row()
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
 * env tree view value column render function 
 */
void on_render_value(GtkTreeViewColumn *tree_column,
	 GtkCellRenderer *cell,
	 GtkTreeModel *tree_model,
	 GtkTreeIter *iter,
	 gpointer data)
{
	/* do not allow to edit value in read only mode */
	if (page_read_only)
		g_object_set (cell, "editable", FALSE, NULL);
	else
	{
		/* do not allow to edit value for empty row */
		GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
		gboolean empty = !gtk_tree_path_compare(path, gtk_tree_row_reference_get_path(empty_row));
		g_object_set (cell, "editable", entering_new_var || !empty, NULL);
		gtk_tree_path_free(path);
	}
}

/*
 * env tree view value changed hadler 
 */
static void on_value_changed(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data)
{
	GtkTreeIter  iter;
	GtkTreePath *tree_path = gtk_tree_path_new_from_string (path);
    
	gboolean empty = !gtk_tree_path_compare(tree_path, gtk_tree_row_reference_get_path(empty_row));

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
				dconfig_set_changed();

				gtk_widget_grab_focus(envtree);
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
			dconfig_set_changed();
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
void on_value_editing_started(GtkCellRenderer *renderer, GtkCellEditable *editable, gchar *path, gpointer user_data)
{
	being_edited_value = gtk_tree_path_new_from_string(path);
}

/*
 * env tree view value editing cancelled (Escape pressed)
 */
void on_value_editing_cancelled(GtkCellRenderer *renderer, gpointer user_data)
{
	/* check whether escape was pressed when editing
	new variable value cell */
	if(!gtk_tree_path_compare(being_edited_value, gtk_tree_row_reference_get_path(empty_row)))
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
}

/*
 * env tree view name changed hadler 
 */
static void on_name_changed(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data)
{
	GtkTreeIter  iter;
	GtkTreePath *tree_path = gtk_tree_path_new_from_string (path);
    
	gboolean empty = !gtk_tree_path_compare(tree_path, gtk_tree_row_reference_get_path(empty_row));

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
			dconfig_set_changed();

			gtk_widget_grab_focus(envtree);
		}
	}
	else if (strcmp(oldvalue, striped))
    {
		gtk_list_store_set(store, &iter, NAME, striped, -1);
		if (empty)
		{
			/* if it was a new row - move cursor to a value cell */
			entering_new_var = TRUE;
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(envtree), tree_path, column_value, renderer_value, TRUE);
		}
		if (!empty)
		{
			dconfig_set_changed();
		}
	}
	
	gtk_tree_path_free(tree_path);
	g_free(oldvalue);
	g_free(striped);
}

/*
 * set target 
 */
void tpage_set_target(const gchar *newvalue)
{
	gtk_entry_set_text(GTK_ENTRY(targetname), newvalue);
}

/*
 * set debugger 
 */
void tpage_set_debugger(const gchar *newvalue)
{
	int module = debug_get_module_index(newvalue);
	if (-1 == module)
	{
		module = 0;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_debugger), module);
}

/*
 * set command line 
 */
void tpage_set_commandline(const gchar *newvalue)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_set_text(buffer, newvalue, -1);
}

/*
 * add environment variable
 */
void tpage_add_environment(const gchar *name, const gchar *value)
{
	GtkTreeIter iter;
	gtk_list_store_prepend(store, &iter);
	gtk_list_store_set(store, &iter, NAME, name, VALUE, value, -1);
}

/*
 * removes all data (clears widgets)
 */
void tpage_clear()
{
	/* target */
	gtk_entry_set_text(GTK_ENTRY(targetname), "");
	
	/* reset debugger type */
	gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_debugger), 0);

	/* arguments */
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_set_text(buffer, "", -1);

	/* environment variables */
	gtk_list_store_clear(store);
	add_empty_row();
}

/*
 * target browse button clicked handler
 */
void on_target_browse_clicked(GtkButton *button, gpointer   user_data)
{
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new (_("Choose target file"),
					  NULL,
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					  NULL);
	
	gchar path[FILENAME_MAX];

	const gchar *prevfile = gtk_entry_get_text(GTK_ENTRY(targetname));
	gchar *prevdir = g_path_get_dirname(prevfile);
	if (strcmp(".", prevdir))
		strcpy(path, prevdir);
	else
		strcpy(path, g_path_get_dirname(DOC_FILENAME(document_get_current())));		
	g_free(prevdir);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (dialog), path);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_entry_set_text(GTK_ENTRY(targetname), filename);
		g_free (filename);
	}
	gtk_widget_destroy (dialog);
}

/*
 * get target file name
 */
gchar* tpage_get_target()
{
	return g_strdup(gtk_entry_get_text(GTK_ENTRY(targetname)));
}

/*
 * get selected debugger module index
 */
int tpage_get_debug_module_index()
{
	return gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_debugger));
}

/*
 * get selected debugger name
 */
gchar* tpage_get_debugger()
{
	return gtk_combo_box_get_active_text(GTK_COMBO_BOX(cmb_debugger));
}

/*
 * get command line
 */
gchar* tpage_get_commandline()
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	
	gchar *args = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	gchar** lines = g_strsplit(args, "\n", 0);
	g_free(args);
	args = g_strjoinv(" ", lines);
	g_strfreev(lines);
	
	return args;
}

/*
 * get list of environment variables
 */
GList* tpage_get_environment()
{
	GList *env = NULL;
	
	GtkTreeIter iter;
	GtkTreeModel *_model = gtk_tree_view_get_model(GTK_TREE_VIEW(envtree));
	gtk_tree_model_get_iter_first(_model, &iter);
	do
	{
		gchar *name, *value;
		gtk_tree_model_get (
			_model,
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
	while (gtk_tree_model_iter_next(_model, &iter));
	
	return env;
}

/*
 * create target page
 */
void tpage_init()
{
	page = gtk_hbox_new(FALSE, 0);
	
	GtkWidget *lbox =		gtk_vbox_new(FALSE, SPACING);
	GtkWidget *mbox =		gtk_vbox_new(FALSE, SPACING);
	
	GtkWidget *hombox =	gtk_hbox_new(TRUE, 0);

	/* left box */
	gtk_container_set_border_width(GTK_CONTAINER(lbox), SPACING);

	/* Target frame */
	GtkWidget *_frame = gtk_frame_new(_("Target"));
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	/* filename hbox */
	GtkWidget *hbox = gtk_hbox_new(FALSE, SPACING);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), SPACING);

	targetname = gtk_entry_new ();
	g_signal_connect(G_OBJECT(targetname), "changed", G_CALLBACK (on_target_changed), NULL);
	
	button_browse = gtk_button_new_with_label(_("Browse"));
	g_signal_connect(G_OBJECT(button_browse), "clicked", G_CALLBACK (on_target_browse_clicked), NULL);
	
	gtk_box_pack_start(GTK_BOX(hbox), targetname, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), button_browse, FALSE, TRUE, 0);

	/* pack in the vertical box */
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	
	/* debugger type hbox */
	hbox = gtk_hbox_new(FALSE, SPACING);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), SPACING);
	GtkWidget *label = gtk_label_new(_("Debugger:")); 
	cmb_debugger = gtk_combo_box_new_text();

	GList *modules = debug_get_modules();
	GList *iter = modules;
	while (iter)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_debugger), (gchar*)iter->data);
		iter = iter->next;
	}
	g_list_free(modules);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_debugger), 0);

	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cmb_debugger, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(_frame), vbox);

	gtk_box_pack_start(GTK_BOX(lbox), _frame, FALSE, FALSE, 0);

	/* Arguments frame */
	_frame = gtk_frame_new(_("Arguments"));
	hbox = gtk_vbox_new(FALSE, SPACING);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), SPACING);
	
	textview = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_CHAR);

	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	g_signal_connect(G_OBJECT(buffer), "changed", G_CALLBACK (on_arguments_changed), NULL);
	
	gtk_box_pack_start(GTK_BOX(hbox), textview, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(_frame), hbox);

	gtk_box_pack_start(GTK_BOX(lbox), _frame, TRUE, TRUE, 0);
	

	/* Environment */
	gtk_container_set_border_width(GTK_CONTAINER(mbox), SPACING);
	_frame = gtk_frame_new(_("Environment variables"));
	hbox = gtk_hbox_new(FALSE, SPACING);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), SPACING);

	store = gtk_list_store_new (
		N_COLUMNS,
		G_TYPE_STRING,
		G_TYPE_STRING);
	model = GTK_TREE_MODEL(store);
	envtree = gtk_tree_view_new_with_model (model);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(envtree), TRUE);
	gtk_tree_view_set_grid_lines (GTK_TREE_VIEW(envtree), GTK_TREE_VIEW_GRID_LINES_VERTICAL);
	g_object_set(envtree, "rules-hint", TRUE, NULL);
	g_signal_connect(G_OBJECT(envtree), "key-press-event", G_CALLBACK (on_envtree_keypressed), NULL);

	const gchar *header;
	int	char_width = get_char_width(envtree);

	header = _("Name");
	renderer_name = gtk_cell_renderer_text_new ();
	g_object_set (renderer_name, "editable", TRUE, NULL);
	g_signal_connect (G_OBJECT (renderer_name), "edited", G_CALLBACK (on_name_changed), NULL);
	column_name = create_column(header, renderer_name, FALSE,
		get_header_string_width(header, MW_NAME, char_width),
		"text", NAME);
	gtk_tree_view_append_column (GTK_TREE_VIEW (envtree), column_name);

	header = _("Value");
	renderer_value = gtk_cell_renderer_text_new ();
	column_value = create_column(header, renderer_value, TRUE,
		get_header_string_width(header, MW_VALUE, char_width),
		"text", VALUE);
	g_signal_connect (G_OBJECT (renderer_value), "edited", G_CALLBACK (on_value_changed), NULL);
	g_signal_connect (G_OBJECT (renderer_value), "editing-started", G_CALLBACK (on_value_editing_started), NULL);
	g_signal_connect (G_OBJECT (renderer_value), "editing-canceled", G_CALLBACK (on_value_editing_cancelled), NULL);
	gtk_tree_view_column_set_cell_data_func(column_value, renderer_value, on_render_value, NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (envtree), column_value);

	/* add empty row */
	add_empty_row();

	/* set multiple selection */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(envtree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	gtk_box_pack_start(GTK_BOX(hbox), envtree, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(_frame), hbox);
	gtk_box_pack_start(GTK_BOX(mbox), _frame, TRUE, TRUE, 0);


	gtk_box_pack_start(GTK_BOX(hombox), lbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hombox), mbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), hombox, TRUE, TRUE, 0);
}

/*
 * get page widget
 */
GtkWidget* tpage_get_widget()
{
	return page;
}

/*
 * set the page readonly
 */
void tpage_set_readonly(gboolean readonly)
{
	gtk_editable_set_editable (GTK_EDITABLE (targetname), !readonly);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), !readonly);
	g_object_set (renderer_name, "editable", !readonly, NULL);
	gtk_widget_set_sensitive (button_browse, !readonly);
	gtk_widget_set_sensitive (cmb_debugger, !readonly);
	
	page_read_only = readonly;
}
