/*
 *		tpage.c
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

/* boxes margins */
#define SPACING 5

/* config file name */
#define CONFIG_NAME ".debugger"

/* config file markers */
#define ENVIRONMENT_MARKER	"[ENV]"
#define BREAKPOINTS_MARKER	"[BREAK]"
#define WATCH_MARKER		"[WATCH]"

/* maximus config file line length */
#define MAXLINE 1000

/* environment variables tree view columns minumum width in characters */
#define MW_NAME		15
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

/* holds config file path for the currently open folder */
gchar current_path[FILENAME_MAX];

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

/* config manipulating buttons */
GtkWidget *savebtn = NULL;
GtkWidget *loadbtn = NULL;
GtkWidget *clearbtn = NULL;

/* env variable name cloumn */
GtkTreeViewColumn *column_name = NULL;
GtkCellRenderer *renderer_name = NULL;

/* env variable value cloumn */
GtkTreeViewColumn *column_value = NULL;
GtkCellRenderer *renderer_value = NULL;

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
		delete_selected_rows();

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
		g_object_set (cell, "editable", FALSE);
	else
	{
		/* do not allow to edit value for empty row */
		GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
		gboolean empty = !gtk_tree_path_compare(path, gtk_tree_row_reference_get_path(empty_row));
		g_object_set (cell, "editable", entering_new_var || !empty);
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

	gboolean res = gtk_tree_model_get_iter (
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

	gboolean res = gtk_tree_model_get_iter (
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
	}
	
	gtk_tree_path_free(tree_path);
	g_free(oldvalue);
	g_free(striped);
}

/*
 * GList and function to collect breaks from breakpoints page to save in config
 */
GList *breaks = NULL;
void collect_breaks(void* bp)
{
	breaks = g_list_append(breaks, bp);
}

/*
 * save config
 */
void on_save_config(GtkButton *button, gpointer user_data)
{
	/* open config file */
	FILE *config = fopen(current_path, "w");
	
	/* get target */
	const gchar *target = gtk_entry_get_text(GTK_ENTRY(targetname));
	fprintf(config, "%s\n", target);
	
	/* debugger type */
	gchar *debugger = gtk_combo_box_get_active_text(GTK_COMBO_BOX(cmb_debugger));
	fprintf(config, "%s\n", debugger);
	
	/* get command line arguments */
	GtkTextIter start, end;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	
	gchar *args = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	gchar** lines = g_strsplit(args, "\n", 0);
	g_free(args);
	args = g_strjoinv(" ", lines);
	g_strfreev(lines);
	fprintf(config, "%s\n", args);
	g_free(args);

	/* environment */
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(envtree));
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
			fprintf(config, "%s\n", ENVIRONMENT_MARKER);
			fprintf(config, "%s\n%s\n", name, value);
		 }
		 
		 g_free(name);
		 g_free(value);
	}
	while (gtk_tree_model_iter_next(model, &iter));

	/* breakpoints */
	breaks_iterate(collect_breaks);
	GList *biter = breaks;
	while (biter)
	{
		breakpoint *bp = (breakpoint*)biter->data;
		
		fprintf(config, "%s\n", BREAKPOINTS_MARKER);
		fprintf(config, "%s\n%i\n%i\n%s\n%i\n",
			bp->file, bp->line, bp->hitscount, bp->condition, bp->enabled);
				
		biter = biter->next;
	}
	g_list_free(breaks);
	breaks = NULL;
	
	/* watches */
	GList *watches = wtree_get_watches();
	biter = watches;
	while (biter)
	{
		gchar *watch = (gchar*)biter->data;
		
		fprintf(config, "%s\n", WATCH_MARKER);
		fprintf(config, "%s\n", watch);
				
		biter = biter->next;
	}
	g_list_foreach(watches, (GFunc)g_free, NULL);
	g_list_free(watches);
	watches = NULL;
	

	fclose(config);

	dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Config saved successfully"));
}

/*
 * reads line from a file
 */
int readline(FILE *file, gchar *buffer, int buffersize)
{
	gchar c;
	int read = 0;
	while (buffersize && fread(&c, 1, 1, file) && '\n' != c)
	{
		buffer[read++] = c;
		buffersize--;
	}
	buffer[read] = '\0';

	return read;
} 

/*
 * function to remove a break when loading config and iterating through
 * existing breaks
 */
void removebreak(void *p)
{
	breakpoint *bp = (breakpoint*)p;
	breaks_remove(bp->file, bp->line);
}

/*
 * load config file
 */
void on_load_config(GtkButton *button, gpointer user_data)
{
	FILE *config = fopen(current_path, "r");
	if (!config)
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Error reading config file"));

	/* target */
	gchar target[FILENAME_MAX];
	readline(config, target, FILENAME_MAX - 1);
	gtk_entry_set_text(GTK_ENTRY(targetname), target);
	
	/* debugger */
	gchar debugger[FILENAME_MAX];
	readline(config, debugger, FILENAME_MAX - 1);
	int index = debug_get_module_index(debugger);
	if (-1 == index)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Configuration error: debugger module \'%s\' is not found"), debugger);
		gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_debugger), 0);
	}
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_debugger), index);

	/* arguments */
	gchar arguments[FILENAME_MAX];
	readline(config, arguments, FILENAME_MAX - 1);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_set_text(buffer, arguments, -1);
	
	/* breakpoints and environment variables */
	breaks_iterate(removebreak);
	wtree_remove_all();
	
	gboolean wrongbreaks = FALSE;
	gchar line[MAXLINE];
	gtk_list_store_clear(store);
	while (readline(config, line, MAXLINE))
	{
		if (!strcmp(line, BREAKPOINTS_MARKER))
		{
			/* file */
			gchar file[FILENAME_MAX];
			readline(config, file, MAXLINE);
			
			/* line */
			int nline;
			readline(config, line, MAXLINE);
			sscanf(line, "%d", &nline);
			
			/* hitscount */
			int hitscount;
			readline(config, line, MAXLINE);
			sscanf(line, "%d", &hitscount);

			/* condition */
			gchar condition[MAXLINE];
			readline(config, condition, MAXLINE);

			/* enabled */
			gboolean enabled;
			readline(config, line, MAXLINE);
			sscanf(line, "%d", &enabled);
			
			/* check whether file is available */
			struct stat st;
			if(!stat(file, &st))
				breaks_add(file, nline, condition, enabled, hitscount);
			else
				wrongbreaks = TRUE;
		}
		else if (!strcmp(line, ENVIRONMENT_MARKER))
		{
			gchar name[MAXLINE], value[1000];
			readline(config, name, MAXLINE);
			readline(config, value, MAXLINE);
			
			GtkTreeIter iter;
			gtk_list_store_append(store, &iter);
			
			gtk_list_store_set(store, &iter, NAME, name, VALUE, value, -1);
		}
		else if (!strcmp(line, WATCH_MARKER))
		{
			gchar watch[MAXLINE];
			readline(config, watch, MAXLINE);
			wtree_add_watch(watch);
		}
		else
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Error reading config file"));
			break;
		}
	}
	if (wrongbreaks)
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Some breakpoints can't be set as the files are missed"));
	
	add_empty_row();
	
	fclose(config);
	
	if (user_data)
		dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Config loaded successfully"));
}

/*
 * clears current configuration
 */
void on_clear(GtkButton *button, gpointer user_data)
{
	/* breakpoints */
	breaks_iterate(removebreak);

	/* watches */
	wtree_remove_all();
	
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
int tpage_get_module_index()
{
	return gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_debugger));
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
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(envtree));
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
 * disable load/save config buttons on document close
 */
void tpage_on_document_close()
{
	gtk_widget_set_sensitive(loadbtn, FALSE);
	gtk_widget_set_sensitive(savebtn, FALSE);
}

/*
 * enable load/save config buttons on document activate
 * if page is not readonly and have config
 */
void tpage_on_document_activate(GeanyDocument *doc)
{
	if (page_read_only)
		return;
	
	if (!doc || !doc->real_path)
	{
		tpage_on_document_close();
		return;
	}

	gtk_widget_set_sensitive(savebtn, TRUE);
	
	gchar *dirname = g_path_get_dirname(DOC_FILENAME(doc));
	gchar *config = g_build_path("/", dirname, CONFIG_NAME, NULL);
	struct stat st;
	gtk_widget_set_sensitive(loadbtn, !stat(config, &st));
	
	strcpy(current_path, config);

	g_free(config);
}

/*
 * create target page
 */
void tpage_init()
{
	page = gtk_hbox_new(FALSE, 0);
	
	GtkWidget *lbox =		gtk_vbox_new(FALSE, SPACING);
	GtkWidget *mbox =		gtk_vbox_new(FALSE, SPACING);
	GtkWidget *rbox =		gtk_vbox_new(FALSE, SPACING);
	GtkWidget* separator =	gtk_vseparator_new();
	
	/* right box with Load/Save buttons */
	gtk_container_set_border_width(GTK_CONTAINER(rbox), SPACING);

	loadbtn = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	g_signal_connect(G_OBJECT(loadbtn), "clicked", G_CALLBACK (on_load_config), (gpointer)TRUE);

	savebtn = gtk_button_new_from_stock(GTK_STOCK_SAVE);
	g_signal_connect(G_OBJECT(savebtn), "clicked", G_CALLBACK (on_save_config), NULL);
	
	clearbtn = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
	g_signal_connect(G_OBJECT(clearbtn), "clicked", G_CALLBACK (on_clear), NULL);

	gtk_box_pack_start(GTK_BOX(rbox), loadbtn, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(rbox), savebtn, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(rbox), clearbtn, FALSE, TRUE, 0);
	

	GtkWidget *hombox =	gtk_hbox_new(TRUE, 0);

	/* left box */
	gtk_container_set_border_width(GTK_CONTAINER(lbox), SPACING);

	/* Target frame */
	GtkWidget *frame = gtk_frame_new(_("Target"));
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	/* filename hbox */
	GtkWidget *hbox = gtk_hbox_new(FALSE, SPACING);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), SPACING);

	targetname = gtk_entry_new ();
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

	gtk_container_add(GTK_CONTAINER(frame), vbox);

	gtk_box_pack_start(GTK_BOX(lbox), frame, FALSE, FALSE, 0);

	/* Arguments frame */
	frame = gtk_frame_new(_("Arguments"));
	hbox = gtk_vbox_new(FALSE, SPACING);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), SPACING);
	
	textview = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_CHAR);
	
	gtk_box_pack_start(GTK_BOX(hbox), textview, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox);

	gtk_box_pack_start(GTK_BOX(lbox), frame, TRUE, TRUE, 0);
	

	/* Environment */
	gtk_container_set_border_width(GTK_CONTAINER(mbox), SPACING);
	frame = gtk_frame_new(_("Environment variables"));
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

	gchar *header;
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
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_box_pack_start(GTK_BOX(mbox), frame, TRUE, TRUE, 0);


	gtk_box_pack_start(GTK_BOX(hombox), lbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hombox), mbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), hombox, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(page), separator, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(page), rbox, FALSE, TRUE, 0);

	/* update Load/Save config button */
	tpage_on_document_activate(document_get_current());
}

/*
 * get page widget
 */
GtkWidget* tpage_get_widget()
{
	return page;
}

/*
 * check if there is a config file in current directory
 */
gboolean tpage_have_config()
{
	GeanyDocument *doc = document_get_current();
	if (!doc)
		return FALSE;
	
	gchar *dirname = g_path_get_dirname(DOC_FILENAME(doc));
	gchar *config = g_build_path("/", dirname, CONFIG_NAME, NULL);
	struct stat st;
	gboolean retval = !stat(config, &st);
	g_free(config);

	return retval;
}

/*
 * set the page readonly
 */
void tpage_set_readonly(gboolean readonly)
{
	gtk_editable_set_editable (GTK_EDITABLE (targetname), !readonly);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), !readonly);
	g_object_set (renderer_name, "editable", !readonly, NULL);
	gtk_widget_set_sensitive (loadbtn, tpage_have_config() && !readonly);
	gtk_widget_set_sensitive (clearbtn, !readonly);
	gtk_widget_set_sensitive (button_browse, !readonly);
	gtk_widget_set_sensitive (cmb_debugger, !readonly);
	
	page_read_only = readonly;
}

/*
 * loads config
 */
void tpage_load_config()
{
	on_load_config(NULL, NULL);
}
