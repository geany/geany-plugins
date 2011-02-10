/*
 *      debug.c
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
#include <string.h>
#include <unistd.h>
#include <pty.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <vte/vte.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

#include "tpage.h"
#include "breakpoint.h"
#include "debug.h"
#include "utils.h"
#include "breakpoints.h"
#include "stree.h"
#include "watch_model.h"
#include "wtree.h"
#include "ltree.h"
#include "tpage.h"

/* module description structure (name/module pointer) */
typedef struct _module_description {
	gchar *title;
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
breakpoint*			interrupt_data = NULL;
break_set_activity	interrupt_flags;

/* flag to set when debug stop is requested while debugger is running.
 * Then this flag is set to TRUE, and debug_request_interrupt function is called
 */
gboolean exit_pending = FALSE;

/* debug terminal PTY master/slave file descriptors */
int pty_master, pty_slave;

/* debug terminal widget */
GtkWidget *terminal = NULL;

/* pointer to the debug notebook widget */
static GtkWidget* debug_notebook = NULL;

/* GtkTextView to put debugger messages */
static GtkWidget *debugger_messages_textview = NULL;

/* 
 * Adjustments of a scroll view containing debugger messages
 * text view. Values are changed during new text adding. 
 */
static GtkAdjustment *hadj = NULL;
static GtkAdjustment *vadj = NULL;

/* stack trace/watch/locals CtkTreeView widgets */
static GtkWidget *stree = NULL;
static GtkWidget *wtree = NULL;
static GtkWidget *ltree = NULL;

/* watch tree view model and store */
GtkTreeStore *wstore = NULL;
GtkTreeModel *wmodel = NULL;

/* array of widgets, ti enable/disable regard of a debug state */
static GtkWidget **sensitive_widget[] = {&stree, &wtree, &ltree, NULL};

/* 
 * information about current instruction
 * used to remove markers when stepping forward
 */
struct ci {
	char file[FILENAME_MAX + 1];
	int line;
} current_instruction;

/*
 * pages which are loaded in debugger and therefore, are set readonly
 */
static GList *read_only_pages = NULL;

/* available modules */
static module_description modules[] = 
{
	{ "GDB", &dbg_module_gdb },
	/*{ "Bash", &dbg_module_bash },*/
	{ NULL, NULL }
};

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
	gboolean res = gtk_tree_model_get_iter (
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

	/* check if it is empty row */
	gboolean is_empty_row = !gtk_tree_path_compare (tree_path, wtree_empty_path());

   	gchar *striped = g_strstrip(g_strdup(new_text));
	if (!strlen(striped) &&
		!is_empty_row &&
		dialogs_show_question(_("Delete variable?")))
	{
		/* if new value is empty string on non-empty row
		 * offer to delete watch */
		gtk_tree_store_remove(wstore, &iter);
		if (DBS_STOPPED == debug_state)
			active_module->remove_watch(oldvalue);
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
			active_module->remove_watch(oldvalue);
			variable *newvar = active_module->add_watch(striped);
			change_watch(GTK_TREE_VIEW(wtree), is_empty_row ? &newiter : &iter, newvar);
		}
		
		/* if new watch has been added - set selection to the new created row */
		if (is_empty_row)
		{
			GtkTreePath *path = gtk_tree_model_get_path(wmodel, &newiter);
			GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(wtree));
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_path_free(path);
		}
	}
	
	/* free resources */
	gtk_tree_path_free(tree_path);
	g_free(oldvalue);
	g_free(striped);
}

/* 
 * text has been dragged into the watch tree view
 */
static void on_watch_dragged_callback(GtkWidget *wgt, GdkDragContext *context, int x, int y,
	GtkSelectionData *seldata, guint info, guint time,
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

					gchar *name = NULL;
					gtk_tree_model_get (
						wmodel,
						&titer,
						W_NAME, &name,
						-1);

					active_module->remove_watch(name);

					g_free(name);
				}


				gtk_tree_store_remove(wstore, &titer);
			}
			
			iter = iter->next;
		}

		/* if all (with or without empty row) was selected - set empty row
		as a path to be selected after deleting */
		if (!reference_to_select)
			reference_to_select = gtk_tree_row_reference_new (gtk_tree_view_get_model(GTK_TREE_VIEW(wtree)), wtree_empty_path());

		/* set selection */
		gtk_tree_selection_unselect_all(selection);
		GtkTreePath *path_to_select = gtk_tree_row_reference_get_path(reference_to_select);
		gtk_tree_selection_select_path(selection, path_to_select);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(widget), path_to_select, NULL, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path_to_select);	

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
 * mouse button has been pressed while being in watch(locals) tree view
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
			    NULL);

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
	/* get tree view model and store as it can be watch or locals tree */
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
static void on_debugger_run ()
{
	/* update debug state */
	debug_state = DBS_RUNNING;

	/* if curren instruction marker was set previously - remove it */
	if (strlen(current_instruction.file))
	{
		markers_remove_current_instruction(current_instruction.file, current_instruction.line);
		strcpy(current_instruction.file, "");
	}

	/* disable widgets */
	enable_sensitive_widgets(FALSE);
}

/* 
 * called from debug module when debugger is being stopped 
 */
static void on_debugger_stopped ()
{
	/* update debug state */
	debug_state = DBS_STOPPED;

	/* if the stop was requested for asyncronous exitig -
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
		interrupt_cb(interrupt_data, interrupt_flags);
		interrupt_data = NULL;
		return;
	}

	/* clear stack tree view */
	stree_clear();

	/* get current stack trace and put in the tree view */
	GList* stack = active_module->get_stack();

	/* if upper frame has source file - remember file anf line for
	current instruction marker */
	frame *f = (frame*)stack->data;
	if (f->have_source)
	{
		strcpy(current_instruction.file, f->file);
		current_instruction.line = (int)f->line;
	}
	
	stack = g_list_reverse(stack);
	
	GList *iter = stack;
	while (iter)
	{
		frame *f = (frame*)iter->data;
		stree_add(f);
		iter = g_list_next(iter);
	}
	g_list_foreach(stack, (GFunc)g_free, NULL);
	g_list_free(stack);
	stree_select_first();

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

	/* locals */
	GList *locals = active_module->get_locals();
	update_variables(GTK_TREE_VIEW(ltree), NULL, locals);
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW(ltree));
	
	/* watches */
	GList *watches = active_module->get_watches();
	update_variables(GTK_TREE_VIEW(wtree), NULL, watches);
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW(wtree));
	
	if (strlen(current_instruction.file))
	{
		/* open current instruction position */
		editor_open_position(current_instruction.file, current_instruction.line);

		/* add current instruction marker */
		markers_add_current_instruction(current_instruction.file, current_instruction.line);
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
	if (strlen(current_instruction.file))
	{
		markers_remove_current_instruction(current_instruction.file, current_instruction.line);
		strcpy(current_instruction.file, "");
	}
	
	/* clear watch page */
	clear_watch_values(GTK_TREE_VIEW(wtree));
	
	/* clear locals page */
	gtk_tree_store_clear(GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ltree))));

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

	/* enable widgets */
	enable_sensitive_widgets(TRUE);

	/* update debug state */
	debug_state = DBS_IDLE;
}

/* 
 * called from debugger module to show a message in  debugger messages pane 
 */
static void on_debugger_message (gchar* message, gchar *color)
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
 * called from debugger module to show an error message box 
 */
static void on_debugger_error (gchar* message)
{
	dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", message);
}

/* callbacks structure to pass to debugger module */
dbg_callbacks callbacks = {
	on_debugger_run,
	on_debugger_stopped,
	on_debugger_exited,
	on_debugger_message,
	on_debugger_error,
};

/*
 * iterating staff to add breakpoints on startup
 * erroneous_break - pointer to the breakpoint that caused error
 * when setting.
 */
breakpoint* erroneous_break;
void set_new_break(void* bp)
{
	if (erroneous_break)
		return;
	
	if(!active_module->set_break((breakpoint*)bp, BSA_NEW_BREAK))
		erroneous_break = (breakpoint*)bp;
}

/*
 * Interface functions
 */

/*
 * init debug related GUI (watch tree view)
 * arguments:
 * 		nb - GtkNotebook to add watch page
 */
void debug_init(GtkWidget* nb)
{
	debug_notebook = nb;

	/* create watch page */
	wtree = wtree_init(on_watch_expanded_callback,
		on_watch_dragged_callback,
		on_watch_key_pressed_callback,
		on_watch_changed,
		on_watch_button_pressed_callback);
	wmodel = gtk_tree_view_get_model(GTK_TREE_VIEW(wtree));
	wstore = GTK_TREE_STORE(wmodel);
	
	GtkWidget *sview = gtk_scrolled_window_new(
		gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(wtree)),
		gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(wtree))
	);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sview),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(sview), wtree);
	gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook),
		sview,
		gtk_label_new(_("Watch")));

	/* create locals page */
	ltree = ltree_init(on_watch_expanded_callback, on_watch_button_pressed_callback);
	sview = gtk_scrolled_window_new(
		gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(ltree)),
		gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(ltree))
	);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sview),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(sview), ltree);
	gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook),
		sview,
		gtk_label_new(_("Locals")));

	/* create stack trace page */
	stree_init(editor_open_position);
	stree = stree_get_widget();
	sview = gtk_scrolled_window_new(
		gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(stree )),
		gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(stree ))
	);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sview),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(sview), stree);
	gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook),
		sview,
		gtk_label_new(_("Call Stack")));

	/* create debug terminal page */
	terminal = vte_terminal_new();
	/* create PTY */
	int res = openpty(&pty_master, &pty_slave, NULL,
		    NULL,
		    NULL);
	grantpt(pty_master);
	unlockpt(pty_master);
	vte_terminal_set_pty(VTE_TERMINAL(terminal), pty_master);
	GtkWidget *scrollbar = gtk_vscrollbar_new(GTK_ADJUSTMENT(VTE_TERMINAL(terminal)->adjustment));
	GTK_WIDGET_UNSET_FLAGS(scrollbar, GTK_CAN_FOCUS);
	GtkWidget *frame = gtk_frame_new(NULL);
	GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
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
	gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook),
		frame,
		gtk_label_new(_("Debug terminal")));
		
	/* debug messages page */
	sview = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sview),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sview));
	vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sview));
	GtkWidget* viewport = gtk_viewport_new(hadj, vadj);
	
	debugger_messages_textview =  gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(debugger_messages_textview), GTK_WRAP_CHAR);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (debugger_messages_textview), FALSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sview), debugger_messages_textview);
	gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook),
		sview,
		gtk_label_new(_("Debugger messages")));

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

	gtk_widget_show_all(debug_notebook);
}

/*
 * called when plugin is being unloaded to remove current instruction marker
 */
void debug_destroy()
{
	/* close PTY file descriptors */
	close(pty_master);
	close(pty_slave);

	/* remove current instruction marker if present */
	if (strlen(current_instruction.file))
		markers_remove_current_instruction(current_instruction.file, current_instruction.line);
}

/*
 * gets current debug state
 */
enum dbs debug_get_state()
{
	return debug_state;
}

/*
 * gets debug module index from name
 * arguments:
 * 		modulename - debug module name
 */
int debug_get_module_index(gchar *modulename)
{
	int index = 0;
	while (modules[index].title)
	{
		if (!strcmp(modules[index].title, modulename))
			return index;
		index++;
	}

	return -1;
}

/*
 * gets GList with all debug modules pointers
 */
GList* debug_get_modules()
{
	GList *mods = NULL;
	module_description *desc = modules;
	while (desc->title)
	{
		mods = g_list_append(mods, desc->title);
		desc++;
	}
	
	return mods;
}

/*
 * checks whether currently active debug module supports asyncronous breaks
 */
gboolean debug_supports_async_breaks()
{
	return active_module->features & MF_ASYNC_BREAKS;
}

/*
 * starts or continues debug process
 */
void debug_run()
{
	if (DBS_IDLE == debug_state)
	{
		/* init selected debugger  module */
	    if((active_module = modules[tpage_get_module_index()].module)->init(&callbacks))
		{
			/* gets parameters from the target page */
			gchar *target = g_strstrip(tpage_get_target());
			if (!strlen(target))
			{
				g_free(target);
				return;
			}
			gchar *commandline = tpage_get_commandline();
			GList *env = tpage_get_environment();

			/* collect watches */
			GList *watches = get_root_items(GTK_TREE_VIEW(wtree));

			/* load file */
			if (active_module->load(target, commandline, env, watches))
			{
				/* set breaks */
				erroneous_break = NULL;
				breaks_iterate(set_new_break);
				
				if (erroneous_break)
				{
					gchar msg[1000];
					sprintf(msg, _("Breakpoint at %s:%i cannot be set\nDebugger message: %s"),
						erroneous_break->file, erroneous_break->line, active_module->error_message());
						
					dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", msg);
						
					active_module->stop();
					debug_state = DBS_STOP_REQUESTED;
					return;
				}
				
				/* set target page - readonly */
				tpage_set_readonly(TRUE);
				
				/* run */
				active_module->run(ttyname(pty_slave));

				/* update debuf state */
				debug_state = DBS_RUN_REQUESTED;
			}
			
			/* free watch list */
			g_list_foreach(watches, (GFunc)g_free, NULL);
			g_list_free(watches);

			/* free target related values */
			g_free(target);
			g_free(commandline);
			g_list_foreach(env, (GFunc)g_free, NULL);
			g_list_free(env);
		}
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
 * stops debug process
 */
void debug_stop()
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
void debug_step_over()
{
	if (DBS_STOPPED == debug_state)
		active_module->step_over();
}

/*
 * step into
 */
void debug_step_into()
{
	if (DBS_STOPPED == debug_state)
		active_module->step_into();
}

/*
 * step out
 */
void debug_step_out()
{
	if (DBS_STOPPED == debug_state)
		active_module->step_out();
}

/*
 * step to position
 */
void debug_execute_until(gchar *file, int line)
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
void debug_request_interrupt(bs_callback cb, breakpoint* bp, break_set_activity flags)
{
	interrupt_cb = cb;
	interrupt_flags = flags;
	interrupt_data = bp;

	active_module->request_interrupt();	
}

/*
 * gets debug modules error message
 */
gchar* debug_error_message()
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
 * check whether source for the current instruction
 * is avaiable
 */
gboolean debug_current_instruction_have_sources()
{
	return strlen(current_instruction.file);
}

/*
 * opens position according to the current instruction
 */
void debug_jump_to_current_instruction()
{
	editor_open_position(current_instruction.file, current_instruction.line);
}

/*
 * called from the document open callback to set file
 * readonly if debug is active and file is a debugging source file
 */
void debug_on_file_open(GeanyDocument *doc)
{
	gchar *file = DOC_FILENAME(doc);
	if (g_list_find_custom(read_only_pages, (gpointer)file, (GCompareFunc)g_strcmp0))
		scintilla_send_message(doc->editor->sci, SCI_SETREADONLY, 1, 0);
}
