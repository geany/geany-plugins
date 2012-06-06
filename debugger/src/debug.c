/*
 *      debug.c
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
 * 		Debug activities handlers (Run, Stop, etc)
 *		Manages GUI that is debug-state dependent.
 * 		Finaly, after checking current debug state - passes
 * 		command to the active debug module.
 * 		Also creates debug-related GUI (local, debug terminal pages)
 * 		and handles run-time watches and breakpoints changes.
 * 		Contains callbacks from debugger module to handle debug state changes.
 */

#include <stdio.h>

#include <stdlib.h>
int unlockpt(int fildes);
int grantpt(int fd);

#include <string.h>
#include <unistd.h>
#include <pty.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <vte/vte.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

#include "tpage.h"
#include "breakpoints.h"
#include "debug.h"
#include "utils.h"
#include "stree.h"
#include "watch_model.h"
#include "wtree.h"
#include "atree.h"
#include "tpage.h"
#include "calltip.h"
#include "bptree.h"
#include "btnpanel.h"
#include "dconfig.h"
#include "tabs.h"

/*
 *  calltip size  
 */ 
#define CALLTIP_HEIGHT 20
#define CALLTIP_WIDTH 200

/* module description structure (name/module pointer) */
typedef struct _module_description {
	const gchar *title;
	dbg_module *module;
} module_description;

/* holds current debug state */
enum dbs debug_state = DBS_IDLE;

/* debug modules declarations */
extern dbg_module dbg_module_gdb;
/* extern dbg_module dbg_module_bash; */

/* active debug module */
dbg_module *active_module = NULL;

/* Interrupt relateed data
 * Interrtion is requested when breakpoint is set/added/removed
 * asyncronously. Then debug_request_interrupt is called,
 * supplied with interrupt reason (interrupt_flags),
 * breakpoint pointer (interrupt_data) and callback to call
 * after interruption
 */
bs_callback			interrupt_cb = NULL;
gpointer			interrupt_data = NULL;

/* flag to set when debug stop is requested while debugger is running.
 * Then this flag is set to TRUE, and debug_request_interrupt function is called
 */
gboolean exit_pending = FALSE;

/* debug terminal PTY master/slave file descriptors */
int pty_master, pty_slave;

/* debug terminal widget */
GtkWidget *terminal = NULL;

/* GtkTextView to put debugger messages */
static GtkWidget *debugger_messages_textview = NULL;

/* 
 * Adjustments of a scroll view containing debugger messages
 * text view. Values are changed during new text adding. 
 */
static GtkAdjustment *hadj = NULL;
static GtkAdjustment *vadj = NULL;

/* stack trace/watch/autos CtkTreeView widgets */
static GtkWidget *stree = NULL;
static GtkWidget *wtree = NULL;
static GtkWidget *atree = NULL;

/* watch tree view model and store */
GtkTreeStore *wstore = NULL;
GtkTreeModel *wmodel = NULL;

/* array of widgets, ti enable/disable regard of a debug state */
static GtkWidget **sensitive_widget[] = {&stree, &wtree, &atree, NULL};

/* 
 * current stack for holding
 * position of ffreames markers
 */
static GList* stack = NULL;

/*
 * pages which are loaded in debugger and therefore, are set readonly
 */
static GList *read_only_pages = NULL;

/* available modules */
static module_description modules[] = 
{
	{ "GDB", &dbg_module_gdb },
	{ NULL, NULL }
};

/* calltips cache  */
static GHashTable *calltips = NULL;

/* 
 * remove stack margin markers
 */
 void remove_stack_markers(void)
{
	int active_frame_index = active_module->get_active_frame();
	
	GList *iter;
	int frame_index;
	for (iter = stack, frame_index = 0; iter; iter = iter->next, frame_index++)
	{
		if (iter)
		{
			frame *f = (frame*)iter->data;
			if (f->have_source)
			{
				if (active_frame_index == frame_index)
				{
					markers_remove_current_instruction(f->file, f->line);
				}
				else
				{
					markers_remove_frame(f->file, f->line);
				}
			}
		}
	}
}

/* 
 * add stack margin markers
 */
static void add_stack_markers(void)
{
	int active_frame_index = active_module->get_active_frame();
	
	GList *iter;
	int frame_index;
	for (iter = stack, frame_index = 0; iter; iter = iter->next, frame_index++)
	{
		if (iter)
		{
			frame *f = (frame*)iter->data;
			if (f->have_source)
			{
				if (active_frame_index == frame_index)
				{
					markers_add_current_instruction(f->file, f->line);
				}
				else
				{
					markers_add_frame(f->file, f->line);
				}
			}
		}
	}
}

/* 
 * Handlers for GUI maked changes in watches
 */

/* 
 * watch expression has been changed
 */
static void on_watch_changed(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data)
{
	/* get iterator to the changed row */
	GtkTreeIter  iter;
	GtkTreePath *tree_path = gtk_tree_path_new_from_string (path);
	gtk_tree_model_get_iter (
		 gtk_tree_view_get_model(GTK_TREE_VIEW(wtree)),
		 &iter,
		 tree_path);
	
	/* get oldvalue */
	gchar* oldvalue;
	gtk_tree_model_get (
		wmodel,
		&iter,
		W_NAME, &oldvalue,
       -1);
	gchar *internal = NULL;
		gtk_tree_model_get (
		wmodel,
		&iter,
		W_INTERNAL, &internal,
		-1);

	/* check if it is empty row */
	GtkTreePath *empty_path = wtree_empty_path();
	gboolean is_empty_row = !gtk_tree_path_compare (tree_path, empty_path);
	gtk_tree_path_free(empty_path);

   	gchar *striped = g_strstrip(g_strdup(new_text));
	if (!strlen(striped) &&
		!is_empty_row &&
		dialogs_show_question(_("Delete variable?")))
	{
		/* if new value is empty string on non-empty row
		 * offer to delete watch */
		gtk_tree_store_remove(wstore, &iter);
		if (DBS_STOPPED == debug_state)
			active_module->remove_watch(internal);

		config_set_debug_changed();
	}
	else if (strcmp(oldvalue, striped))
    {
		/* new value is non empty */

		/* insert new row if changing was the last empty row */
		GtkTreeIter newiter;
		if (is_empty_row)
			gtk_tree_store_insert_before(wstore, &newiter, NULL, &iter);
		
		/* set expression */
		variable_set_name_only(wstore, is_empty_row ? &newiter : &iter, striped);

		/* if debug is active - remove old watch and add new one */
		if (DBS_STOPPED == debug_state)
		{
			active_module->remove_watch(internal);
			variable *newvar = active_module->add_watch(striped);
			change_watch(GTK_TREE_VIEW(wtree), is_empty_row ? &newiter : &iter, newvar);
		}
		
		/* if new watch has been added - set selection to the new created row */
		if (is_empty_row)
		{
			GtkTreePath *_path = gtk_tree_model_get_path(wmodel, &newiter);
			GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(wtree));
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, _path);
			gtk_tree_path_free(_path);
		}

		config_set_debug_changed();
	}
	
	/* free resources */
	gtk_tree_path_free(tree_path);
	g_free(oldvalue);
	g_free(internal);
	g_free(striped);
}

/* 
 * text has been dragged into the watch tree view
 */
static void on_watch_dragged_callback(GtkWidget *wgt, GdkDragContext *context, int x, int y,
	GtkSelectionData *seldata, guint info, guint _time,
    gpointer userdata)
{
	/* string that is dragged */
	gchar *expression = (gchar*)seldata->data;
	
	/* lookup for where the text has been dropped */
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(wtree), x, y, &path, &pos);

	/* if dropped into last row - insert before it */
	GtkTreePath *empty_path = wtree_empty_path();
	if (!gtk_tree_path_compare(empty_path, path))
		pos = GTK_TREE_VIEW_DROP_BEFORE;
	gtk_tree_path_free(empty_path);
	
	/* if dropped into children area - insert before parent */
	if (gtk_tree_path_get_depth(path) > 1)
	{
		while (gtk_tree_path_get_depth(path) > 1)
			gtk_tree_path_up(path);
		pos = GTK_TREE_VIEW_DROP_BEFORE;
	}
	
	/* insert new row */
	GtkTreeIter newvar;
	if (path)
	{
		GtkTreeIter sibling;
		gtk_tree_model_get_iter(wmodel, &sibling, path);
		
		if (GTK_TREE_VIEW_DROP_BEFORE == pos || GTK_TREE_VIEW_DROP_INTO_OR_BEFORE == pos)
			gtk_tree_store_insert_before(wstore, &newvar, NULL, &sibling);
		else
			gtk_tree_store_insert_after(wstore, &newvar, NULL, &sibling);
	}
	else
	{
		GtkTreeIter empty = wtree_empty_row();
		gtk_tree_store_insert_before(wstore, &newvar, NULL, &empty);
	}
	
	/* if debugger is active (in stopped condition) - add to run-time watch list
	 *  if not - just set new expession in the tree view */ 
	if (DBS_STOPPED == debug_state)
	{
		variable *var = active_module->add_watch(expression);
		change_watch(GTK_TREE_VIEW(wtree), &newvar, var);
	}
	else
		variable_set_name_only(wstore, &newvar, expression);

	config_set_debug_changed();
}

/* 
 * key has been pressed while being in watch tree view
 */
static gboolean on_watch_key_pressed_callback(GtkWidget *widget, GdkEvent  *event, gpointer user_data)
{
	/* handling only Delete button pressing
	 * that means "delete selected rows" */
	int keyval = ((GdkEventKey*)event)->keyval;
	if (keyval != GDK_Delete)
		return FALSE;

	/* get selected rows */
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(wtree));
	GList *rows = gtk_tree_selection_get_selected_rows(selection, &wmodel);
	
	/* empty row path */
	GtkTreePath *empty_path = wtree_empty_path();

	/* check whether only empty row was selected */
	if (1 != gtk_tree_selection_count_selected_rows(selection) ||
	    gtk_tree_path_compare((GtkTreePath*)rows->data, empty_path))
	{
		/* path reference to select after deleteing finishes */
		GtkTreeRowReference *reference_to_select = NULL;

		/* get references to the rows */
		GList *references = NULL;
		GList *iter = rows;
		while (iter)
		{
			GtkTreePath *path = (GtkTreePath*)iter->data;

			/* move up paths to the root elements */
			while (gtk_tree_path_get_depth(path) > 1)
				gtk_tree_path_up(path);

			/* add path reference if it's not an empty row*/
			if (gtk_tree_path_compare(path, empty_path))
				references = g_list_append(references, gtk_tree_row_reference_new(wmodel, path));

			iter = iter->next;
		}

		/* iterate through references and remove */
		iter = references;
		while (iter)
		{
			GtkTreeRowReference *reference = (GtkTreeRowReference*)iter->data;
			/* check for valid reference because two or more equal
			refernces could be put in the list if several child items
			of the same node were selected and the path for the
			current reference was already deleted */
			if (gtk_tree_row_reference_valid(reference))
			{
				GtkTreePath *path = gtk_tree_row_reference_get_path(reference);

				if (!reference_to_select)
				{
					/* select upper sibling of the upper
					selected row that has unselected upper sibling */
					GtkTreePath *sibling = gtk_tree_path_copy(path);
					if(gtk_tree_path_prev(sibling))
					{
						if (!gtk_tree_selection_path_is_selected(selection, sibling))
							reference_to_select = gtk_tree_row_reference_new(gtk_tree_view_get_model(GTK_TREE_VIEW(wtree)), sibling);
					}
					else if (gtk_tree_path_next(sibling), gtk_tree_path_compare(path, sibling))
						reference_to_select = gtk_tree_row_reference_new(gtk_tree_view_get_model(GTK_TREE_VIEW(wtree)), sibling);
				}

				/* get iterator */
				GtkTreeIter titer;
				gtk_tree_model_get_iter(wmodel, &titer, path);

				/* remove from the debug session, if it's active */
				if (DBS_STOPPED == debug_state)
				{

					gchar *internal = NULL;
					gtk_tree_model_get (
						wmodel,
						&titer,
						W_INTERNAL, &internal,
						-1);

					active_module->remove_watch(internal);

					g_free(internal);
				}


				gtk_tree_store_remove(wstore, &titer);

				gtk_tree_path_free(path);
			}
			
			iter = iter->next;
		}

		/* if all (with or without empty row) was selected - set empty row
		as a path to be selected after deleting */
		if (!reference_to_select)
		{
			GtkTreePath *path = wtree_empty_path();
			reference_to_select = gtk_tree_row_reference_new (gtk_tree_view_get_model(GTK_TREE_VIEW(wtree)), path);
			gtk_tree_path_free(path);
		}

		/* set selection */
		gtk_tree_selection_unselect_all(selection);
		GtkTreePath *path_to_select = gtk_tree_row_reference_get_path(reference_to_select);
		gtk_tree_selection_select_path(selection, path_to_select);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(widget), path_to_select, NULL, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path_to_select);	

		/* free references list */
		g_list_foreach (references, (GFunc)gtk_tree_row_reference_free, NULL);
		g_list_free (references);

		config_set_debug_changed();
	}

	gtk_tree_path_free(empty_path);

	/* free rows list */
	g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (rows);

	return FALSE;
}

/* 
 * mouse button has been pressed while being in watch(autos) tree view
 */
gboolean on_watch_button_pressed_callback(GtkWidget *treeview, GdkEventButton *event, gpointer userdata)
{
    if (event->type == GDK_2BUTTON_PRESS  &&  event->button == 1)
	{
		GtkTreePath *path = NULL;
		if (gtk_tree_view_get_path_at_pos(
			GTK_TREE_VIEW(treeview),
		    (int)event->x, (int)event->y, &path, NULL, NULL, NULL))
		{
			GtkTreeIter iter;
			GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
			gtk_tree_model_get_iter (model, &iter, path);

			gchar *expression = NULL;
			gtk_tree_model_get(model, &iter,
				W_EXPRESSION, &expression,
			    -1);

			if (strlen(expression))
			{
				GtkTreeIter newvar;
				GtkTreeIter empty = wtree_empty_row();
				gtk_tree_store_insert_before(wstore, &newvar, NULL, &empty);
			
				/* if debugger is active (in stopped condition) - add to run-time watch list
				 *  if not - just set new expession in the tree view */ 
				if (DBS_STOPPED == debug_state)
				{
					variable *var = active_module->add_watch(expression);
					change_watch(GTK_TREE_VIEW(wtree), &newvar, var);
				}
				else
					variable_set_name_only(wstore, &newvar, expression);

				config_set_debug_changed();
			}

			g_free(expression);
		}
	}
	
	return FALSE;
}


/* 
 * watch that has children has been expanded
 */
static void on_watch_expanded_callback(GtkTreeView *tree, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	/* get tree view model and store as it can be watch or autos tree */
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	
	/* get a flag indicating that item has only stub andd need to
	 * evaluate its children */
	gboolean only_stub = FALSE;
	gtk_tree_model_get (
		model,
		iter,
		W_STUB, &only_stub,
		-1);
	
	if (only_stub)
	{
		/* if item has not been expanded before */
		gchar *internal;
		gtk_tree_model_get (
			model,
			iter,
			W_INTERNAL, &internal,
			-1);
		
		/* get children list */
		GList *children = active_module->get_children(internal);
		
		/* remove stub and add children */
		expand_stub(tree, iter, children);
		
		/* free children list */
		free_variables_list(children);

		/* unset W_STUB flag */
		gtk_tree_store_set (store, iter,
			W_STUB, FALSE,
			-1);

		g_free(internal);	
	}
}

/* 
 * GUI functions
 */

/* 
 * enable/disable sensitive widgets
 * arguments:
 * 		enable - enable or disable widgets 
 */
static void enable_sensitive_widgets(gboolean enable)
{
	int i;
	for(i = 0; sensitive_widget[i]; i++)
		gtk_widget_set_sensitive(*sensitive_widget[i], enable);
}

/* 
 * Debug state changed hanflers
 */

/* 
 * called from debug module when debugger is being run 
 */
static void on_debugger_run (void)
{
	/* update debug state */
	debug_state = DBS_RUNNING;

	/* if curren instruction marker was set previously - remove it */
	if (stack)
	{
		remove_stack_markers();
		g_list_foreach(stack, (GFunc)frame_free, NULL);
		g_list_free(stack);
		stack = NULL;

		stree_remove_frames();
	}

	/* disable widgets */
	enable_sensitive_widgets(FALSE);

	/* update buttons panel state */
	btnpanel_set_debug_state(debug_state);
}


/* 
 * called from debug module when debugger is being stopped 
 */
static void on_debugger_stopped (int thread_id)
{
	/* update debug state */
	debug_state = DBS_STOPPED;

	/* update buttons panel state */
	if (!interrupt_data)
	{
		btnpanel_set_debug_state(debug_state);
	}

	/* clear calltips cache */
	g_hash_table_remove_all(calltips);

	/* if a stop was requested for asyncronous exiting -
	 * stop debug module and exit */
	if (exit_pending)
	{
		active_module->stop();
		exit_pending = FALSE;
		return;
	}

	/* check for async activities pending */
	if (interrupt_data)
	{
		interrupt_cb(interrupt_data);
		interrupt_data = NULL;
		
		active_module->resume();
		
		return;
	}

	/* clear stack tree view */
	stree_set_active_thread_id(thread_id);

	/* get current stack trace and put in the tree view */
	stack = active_module->get_stack();
	
	GList *iter = stack;
	while (iter)
	{
		frame *f = (frame*)iter->data;
		stree_add(f);
		iter = g_list_next(iter);
	}
	stree_select_first_frame(TRUE);

	/* files */
	GList *files = active_module->get_files();
	/* remove from list and make writable those files,
	that are not in the current list */
	iter = read_only_pages;
	while (iter)
	{
		if (!g_list_find_custom(files, iter->data, (GCompareFunc)g_strcmp0))
		{
			/* set document writable */
			GeanyDocument *doc = document_find_by_real_path((const gchar*)iter->data);
			if (doc)
				scintilla_send_message(doc->editor->sci, SCI_SETREADONLY, 0, 0);

			/* free file name */
			g_free(iter->data);
			/* save next item pointer */
			GList *next = iter->next;
			/* remove current item */
			read_only_pages = g_list_delete_link(read_only_pages, iter);

			/* set next item and continue */
			iter = next;
			continue;
		}

		iter = iter->next;
	}
	/* add to the list and make readonly those files
	from the current list that are new */
	iter = files;
	while (iter)
	{
		if (!g_list_find_custom(read_only_pages, iter->data, (GCompareFunc)g_strcmp0))
		{
			/* set document readonly */
			GeanyDocument *doc = document_find_by_real_path((const gchar*)iter->data);
			if (doc)
				scintilla_send_message(doc->editor->sci, SCI_SETREADONLY, 1, 0);

			/* add new file to the list */
			read_only_pages = g_list_append(read_only_pages, g_strdup((gchar*)iter->data));
		}
		iter = iter->next;
	}
	g_list_free(files);

	/* autos */
	GList *autos = active_module->get_autos();
	update_variables(GTK_TREE_VIEW(atree), NULL, autos);
	
	/* watches */
	GList *watches = active_module->get_watches();
	update_variables(GTK_TREE_VIEW(wtree), NULL, watches);

	if (stack)
	{
		frame *current = (frame*)stack->data;

		if (current->have_source)
		{
			/* open current instruction position */
			editor_open_position(current->file, current->line);
		}

		/* add current instruction marker */
		add_stack_markers();
	}

	/* enable widgets */
	enable_sensitive_widgets(TRUE);

	/* remove breaks readonly if current module doesn't support run-time breaks operation */
	if (!(active_module->features & MF_ASYNC_BREAKS))
		bptree_set_readonly(FALSE);
}

/* 
 * called when debugger exits 
 */
static void on_debugger_exited (int code)
{
	/* remove marker for current instruction if was set */
	if (stack)
	{
		remove_stack_markers();
		g_list_foreach(stack, (GFunc)frame_free, NULL);
		g_list_free(stack);
		stack = NULL;
	}
	
	/* clear watch page */
	clear_watch_values(GTK_TREE_VIEW(wtree));
	
	/* clear autos page */
	gtk_tree_store_clear(GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(atree))));

	/* clear stack trace tree */
	stree_clear();

	/* clear debug terminal */
	vte_terminal_reset(VTE_TERMINAL(terminal), TRUE, TRUE);

	/* clear debug messages window */
	GtkTextIter start, end;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(debugger_messages_textview));
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	gtk_text_buffer_delete(buffer, &start, &end);

	/* enable target page */
	tpage_set_readonly(FALSE);

	/* remove breaks readonly if current module doesn't support run-time breaks operation */
	if (!(active_module->features & MF_ASYNC_BREAKS))
		bptree_set_readonly(FALSE);
	
	/* set files that was readonly during debug writable */
	GList *iter = read_only_pages;
	while (iter)
	{
		GeanyDocument *doc = document_find_by_real_path((const gchar*)iter->data);
		if (doc)
			scintilla_send_message(doc->editor->sci, SCI_SETREADONLY, 0, 0);

		/* free file name */
		g_free(iter->data);

		iter = iter->next;
	}
	g_list_free(read_only_pages);
	read_only_pages = NULL;

	/* clear and destroy calltips cache */
	g_hash_table_destroy(calltips);
	calltips = NULL;

	/* enable widgets */
	enable_sensitive_widgets(TRUE);

	/* update buttons panel state */
	btnpanel_set_debug_state(DBS_IDLE);
	
	/* update debug state */
	debug_state = DBS_IDLE;
}

/* 
 * called from debugger module to show a message in  debugger messages pane 
 */
void on_debugger_message (const gchar* message, const gchar *color)
{
	gchar *msg = g_strdup_printf("%s\n", message);

	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(debugger_messages_textview));

	GtkTextIter iter;
	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, msg, -1, color, NULL);

	g_free(msg);

	gtk_adjustment_set_value(vadj, gtk_adjustment_get_upper(vadj));
}

/* 
 * called from debugger module to clear messages tab 
 */
static void on_debugger_messages_clear (void)
{
	/* clear debug messages window */
	GtkTextIter start, end;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(debugger_messages_textview));
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	gtk_text_buffer_delete(buffer, &start, &end);
}

/* 
 * called from debugger module to show an error message box 
 */
static void on_debugger_error (const gchar* message)
{
	dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", message);
}

/* 
 * called from debugger module when a thead has been removed 
 */
static void on_thread_removed(int thread_id)
{
	stree_remove_thread(thread_id);
}

/* 
 * called from debugger module when a new thead has been added 
 */
static void on_thread_added (int thread_id)
{
	stree_add_thread(thread_id);
}

/* callbacks structure to pass to debugger module */
dbg_callbacks callbacks = {
	on_debugger_run,
	on_debugger_stopped,
	on_debugger_exited,
	on_debugger_message,
	on_debugger_messages_clear,
	on_debugger_error,
	on_thread_added,
	on_thread_removed,
};

/*
 * Interface functions
 */

/* 
 * called when a frame in the stack tree has been selected 
 */
static void on_select_frame(int frame_number)
{
	frame *f = (frame*)g_list_nth(stack, active_module->get_active_frame())->data;
	markers_remove_current_instruction(f->file, f->line);
	markers_add_frame(f->file, f->line);

	active_module->set_active_frame(frame_number);
	
	/* clear calltips cache */
	g_hash_table_remove_all(calltips);
	
	/* autos */
	GList *autos = active_module->get_autos();
	update_variables(GTK_TREE_VIEW(atree), NULL, autos);
	
	/* watches */
	GList *watches = active_module->get_watches();
	update_variables(GTK_TREE_VIEW(wtree), NULL, watches);

	f = (frame*)g_list_nth(stack, frame_number)->data;
	markers_remove_frame(f->file, f->line);
	markers_add_current_instruction(f->file, f->line);
}

/*
 * init debug related GUI (watch tree view)
 * arguments:
 */
void debug_init(void)
{
	/* create watch page */
	wtree = wtree_init(on_watch_expanded_callback,
		on_watch_dragged_callback,
		on_watch_key_pressed_callback,
		on_watch_changed,
		on_watch_button_pressed_callback);
	wmodel = gtk_tree_view_get_model(GTK_TREE_VIEW(wtree));
	wstore = GTK_TREE_STORE(wmodel);
	
	tab_watch = gtk_scrolled_window_new(
		gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(wtree)),
		gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(wtree))
	);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab_watch),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(tab_watch), wtree);

	/* create autos page */
	atree = atree_init(on_watch_expanded_callback, on_watch_button_pressed_callback);
	tab_autos = gtk_scrolled_window_new(
		gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(atree)),
		gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(atree))
	);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab_autos),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(tab_autos), atree);
	
	/* create stack trace page */
	stree = stree_init(editor_open_position, on_select_frame);
	tab_call_stack = gtk_scrolled_window_new(
		gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(stree )),
		gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(stree ))
	);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab_call_stack),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(tab_call_stack), stree);
	
	/* create debug terminal page */
	terminal = vte_terminal_new();
	/* create PTY */
	openpty(&pty_master, &pty_slave, NULL,
		    NULL,
		    NULL);
	grantpt(pty_master);
	unlockpt(pty_master);
	vte_terminal_set_pty(VTE_TERMINAL(terminal), pty_master);
	GtkWidget *scrollbar = gtk_vscrollbar_new(GTK_ADJUSTMENT(VTE_TERMINAL(terminal)->adjustment));
	GTK_WIDGET_UNSET_FLAGS(scrollbar, GTK_CAN_FOCUS);
	tab_terminal = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type (GTK_FRAME(tab_terminal), GTK_SHADOW_NONE);
	GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(tab_terminal), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), terminal, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);
	/* set the default widget size first to prevent VTE expanding too much,
	 * sometimes causing the hscrollbar to be too big or out of view. */
	gtk_widget_set_size_request(GTK_WIDGET(terminal), 10, 10);
	vte_terminal_set_size(VTE_TERMINAL(terminal), 30, 1);
	/* set terminal font. */
	GKeyFile *config = g_key_file_new();
	gchar *configfile = g_strconcat(geany_data->app->configdir, G_DIR_SEPARATOR_S, "geany.conf", NULL);
	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);
	gchar *font = utils_get_setting_string(config, "VTE", "font", "Monospace 10");
	vte_terminal_set_font_from_string (VTE_TERMINAL(terminal), font);	
		
	/* debug messages page */
	tab_messages = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab_messages),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(tab_messages));
	vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(tab_messages));
	
	debugger_messages_textview =  gtk_text_view_new();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (debugger_messages_textview), FALSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(tab_messages), debugger_messages_textview);
	
	/* create tex tags */
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(debugger_messages_textview));
	gtk_text_buffer_create_tag(buffer, "black", "foreground", "#000000", NULL); 
	gtk_text_buffer_create_tag(buffer, "grey", "foreground", "#AAAAAA", NULL); 
	gtk_text_buffer_create_tag(buffer, "red", "foreground", "#FF0000", NULL); 
	gtk_text_buffer_create_tag(buffer, "green", "foreground", "#00FF00", NULL); 
	gtk_text_buffer_create_tag(buffer, "blue", "foreground", "#0000FF", NULL); 
	gtk_text_buffer_create_tag(buffer, "yellow", "foreground", "#FFFF00", NULL);
	gtk_text_buffer_create_tag(buffer, "brown", "foreground", "#BB8915", NULL);
	gtk_text_buffer_create_tag(buffer, "rose", "foreground", "#BA92B7", NULL);
}

/*
 * called when plugin is being unloaded to remove current instruction marker
 */
void debug_destroy(void)
{
	/* close PTY file descriptors */
	close(pty_master);
	close(pty_slave);

	/* remove stack markers if present */
	if (stack)
	{
		remove_stack_markers();
		g_list_foreach(stack, (GFunc)frame_free, NULL);
		g_list_free(stack);
		stack = NULL;
	}
	
	stree_destroy();
}

/*
 * gets current debug state
 */
enum dbs debug_get_state(void)
{
	return debug_state;
}

/*
 * gets current stack frames lisy
 */
GList* debug_get_stack(void)
{
	return stack;
}

/*
 * gets debug module index from name
 * arguments:
 * 		modulename - debug module name
 */
int debug_get_module_index(const gchar *modulename)
{
	int _index = 0;
	while (modules[_index].title)
	{
		if (!strcmp(modules[_index].title, modulename))
			return _index;
		_index++;
	}

	return -1;
}

/*
 * gets GList with all debug modules pointers
 */
GList* debug_get_modules(void)
{
	GList *mods = NULL;
	module_description *desc = modules;
	while (desc->title)
	{
		mods = g_list_append(mods, (gpointer)desc->title);
		desc++;
	}
	
	return mods;
}

/*
 * checks whether currently active debug module supports asyncronous breaks
 */
gboolean debug_supports_async_breaks(void)
{
	return active_module->features & MF_ASYNC_BREAKS;
}

/*
 * starts or continues debug process
 */
void debug_run(void)
{
	if (DBS_IDLE == debug_state)
	{
		gchar *target = g_strstrip(tpage_get_target());
		if (!strlen(target))
		{
			g_free(target);
			return;
		}
		gchar *commandline = tpage_get_commandline();
		GList *env = tpage_get_environment();
		GList *watches = get_root_items(GTK_TREE_VIEW(wtree));
		GList *breaks = breaks_get_all();

		/* init selected debugger  module */
		active_module = modules[tpage_get_debug_module_index()].module;
		if(active_module->run(target, commandline, env, watches, breaks, ttyname(pty_slave), &callbacks))
		{
			/* set target page - readonly */
			tpage_set_readonly(TRUE);

			/* update debuf state */
			debug_state = DBS_RUN_REQUESTED;
		}

		/* free stuff */
		g_free(target);
		g_free(commandline);

		g_list_foreach(env, (GFunc)g_free, NULL);
		g_list_free(env);

		g_list_foreach(watches, (GFunc)g_free, NULL);
		g_list_free(watches);

		g_list_free(breaks);
	}
	else if (DBS_STOPPED == debug_state)
	{
		/* resume */
		active_module->resume();
		debug_state = DBS_RUN_REQUESTED;
	}

	/* set breaks readonly if current module doesn't support run-time breaks operation */
	if (!(active_module->features & MF_ASYNC_BREAKS))
		bptree_set_readonly(TRUE);
}

/*
 * restarts debug process
 */
void debug_restart(void)
{
	if (DBS_STOPPED == debug_state)
	{
		/* stop instantly if not running */
		vte_terminal_reset(VTE_TERMINAL(terminal), TRUE, TRUE);
		active_module->restart();
		debug_state = DBS_RUN_REQUESTED;
	}
}

/*
 * stops debug process
 */
void debug_stop(void)
{
	if (DBS_STOPPED == debug_state)
	{
		/* stop instantly if not running */
		active_module->stop();
		debug_state = DBS_STOP_REQUESTED;
	}
	else if (DBS_IDLE != debug_state)
	{
		/* if running - request interrupt */
		exit_pending = TRUE;
		active_module->request_interrupt();	
	}
}

/*
 * step over
 */
void debug_step_over(void)
{
	if (DBS_STOPPED == debug_state)
		active_module->step_over();
}

/*
 * step into
 */
void debug_step_into(void)
{
	if (DBS_STOPPED == debug_state)
		active_module->step_into();
}

/*
 * step out
 */
void debug_step_out(void)
{
	if (DBS_STOPPED == debug_state)
		active_module->step_out();
}

/*
 * step to position
 */
void debug_execute_until(const gchar *file, int line)
{
	if (DBS_STOPPED == debug_state)
		active_module->execute_until(file, line);
}

/*
 * sets a break
 * arguments:
 *		bp - breakpoitn to set
 * 		bsa - what to do with breakpoint (add/remove/change)
 */
gboolean debug_set_break(breakpoint* bp, break_set_activity bsa)
{
	if (DBS_STOPPED == debug_state)
	{
		return active_module->set_break(bp, bsa);
	}
	return FALSE;
}

/*
 * removes a break
 * arguments:
 *		bp - breakpoitn to set
 */
gboolean debug_remove_break(breakpoint* bp)
{
	if (DBS_STOPPED == debug_state)
	{
		return active_module->remove_break(bp);
	}
	return FALSE;
}

/*
 * requests active debug module to interrupt fo further
 * breakpoint modifications
 * arguments:
 *		cb - callback to call on interruption happents
 * 		bp - breakpoint to deal with
 * 		flags - whar to do with breakpoint
 */
void debug_request_interrupt(bs_callback cb, gpointer data)
{
	interrupt_cb = cb;
	interrupt_data = data;

	active_module->request_interrupt();	
}

/*
 * gets debug modules error message
 */
gchar* debug_error_message(void)
{
	return 	active_module->error_message();	
}

/*
 * evaluates expression in runtime and returns its value or NULL if unevaluatable
 */
gchar* debug_evaluate_expression(gchar *expression)
{
	return active_module->evaluate_expression(expression);
}

/*
 * return list of strings for the calltip
 * first line is a header, others should be shifted right with tab
 */
gchar* debug_get_calltip_for_expression(gchar* expression)
{
	gchar *calltip = NULL;
	if (!calltips || !(calltip = g_hash_table_lookup(calltips, expression)))
	{
		GString *calltip_str = NULL;
		variable *var = active_module->add_watch(expression);
		if (var)
		{
			calltip_str = get_calltip_line(var, TRUE);
			if (var->has_children)
			{
				int lines_left = MAX_CALLTIP_HEIGHT - 1;
				GList* children = active_module->get_children(var->internal->str); 
				GList* child = children;
				while(child && lines_left)
				{
					variable *varchild = (variable*)child->data;
					GString *child_string = get_calltip_line(varchild, FALSE);
					g_string_append_printf(calltip_str, "\n%s", child_string->str);
					g_string_free(child_string, TRUE);

					child = child->next;
					lines_left--;
				}
				if (!lines_left && child)
				{
					g_string_append(calltip_str, "\n\t\t........");
				}
				g_list_foreach(children, (GFunc)variable_free, NULL);
				g_list_free(children);
			}

			active_module->remove_watch(var->internal->str);

			calltip = g_string_free(calltip_str, FALSE);

			if (!calltips)
			{
				calltips = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)g_free);
			}
			g_hash_table_insert(calltips, g_strdup(expression), calltip);
		}
	}

	return calltip;
}

/*
 * check whether source for the current instruction
 * is avaiable
 */
gboolean debug_current_instruction_have_sources(void)
{
	frame *current = (frame*)stack->data;
	return current->have_source ? strlen(current->file) : 0;
}

/*
 * opens position according to the current instruction
 */
void debug_jump_to_current_instruction(void)
{
	frame *current = (frame*)stack->data;
	editor_open_position(current->file, current->line);
}

/*
 * called from the document open callback to set file
 * readonly if debug is active and file is a debugging source file
 */
void debug_on_file_open(GeanyDocument *doc)
{
	const gchar *file = DOC_FILENAME(doc);
	if (g_list_find_custom(read_only_pages, (gpointer)file, (GCompareFunc)g_strcmp0))
		scintilla_send_message(doc->editor->sci, SCI_SETREADONLY, 1, 0);
}

/*
 * get active frame index
 */
int debug_get_active_frame(void)
{
	return active_module->get_active_frame();
}
