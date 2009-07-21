/*
 *      tasks - tasks.c
 *
 *      Copyright 2009 Bert Vermeulen <bert@biot.com>
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

#include "geanyplugin.h"

#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "addons.h"
#include "tasks.h"


#define DEFAULT_TOKENS { "TODO", "FIXME", NULL };


typedef struct {
	unsigned int line;
	GString *description;
} GeanyTask;


static GString *linebuf = NULL;
static char *tokens[] = DEFAULT_TOKENS;
static GHashTable *globaltasks = NULL;
static GtkListStore *taskstore = NULL;
static GtkWidget *notebook_page = NULL;
static gboolean tasks_enabled = FALSE;


static gboolean tasks_button_cb(GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean tasks_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void free_editor_tasks(gpointer key, gpointer value, gpointer data);
static void scan_all_documents(void);
static void scan_document_for_tasks(GeanyDocument *doc);
static void create_tasks_tab(void);
static int scan_line_for_tokens(ScintillaObject *sci, unsigned int line);
static int scan_buf_for_tokens(char *buf);
static GeanyTask *create_task(unsigned int line, char *description);
static int find_line(GeanyTask *task, unsigned int *line);
static void found_token(GeanyEditor *editor, unsigned int line, char *d);
static void no_token(GeanyEditor *editor, unsigned int line);
static void lines_moved(GeanyEditor *editor, unsigned int line, int change);
static int keysort(GeanyTask *a, GeanyTask *b);
static void render_taskstore(GeanyEditor *editor);


static void tasks_init(void)
{
	globaltasks = g_hash_table_new(NULL, NULL);
	linebuf = g_string_sized_new(256);
	create_tasks_tab();
	scan_all_documents();

	tasks_enabled = TRUE;
}

static void tasks_cleanup(void)
{
	GtkWidget *notebook;
	int page;

	g_string_free(linebuf, TRUE);

	g_hash_table_foreach(globaltasks, free_editor_tasks, NULL);
	g_hash_table_destroy(globaltasks);

	notebook = ui_lookup_widget(geany->main_widgets->window, "notebook_info");
	page = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), notebook_page);
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), page);

	tasks_enabled = FALSE;
}

void tasks_set_enable(gboolean enable)
{
	if (tasks_enabled != enable)
	{
		if (enable)
			tasks_init();
		else
			tasks_cleanup();
	}
}

void tasks_on_document_close(GObject *object, GeanyDocument *doc, gpointer data)
{

	if(tasks_enabled && doc->is_valid)
		free_editor_tasks(doc->editor, NULL, NULL);

}


void tasks_on_document_open(GObject *object, GeanyDocument *doc, gpointer data)
{

	if(tasks_enabled && doc->is_valid)
		scan_document_for_tasks(doc);

}


void tasks_on_document_activate(GObject *object, GeanyDocument *doc, gpointer data)
{

	if(tasks_enabled && doc->is_valid)
		render_taskstore(doc->editor);

}


gboolean tasks_on_editor_notify(GObject *object, GeanyEditor *editor,
								 SCNotification *nt, gpointer data)
{
	static int mod_line = -1;
	int pos, line, offset;

	if (! tasks_enabled)
		return FALSE;

	switch (nt->nmhdr.code)
	{
		case SCN_MODIFIED:
			line = sci_get_line_from_position(editor->sci, nt->position);
			if(nt->linesAdded != 0)
				/* check if existing tasks had their line numbers changed */
				lines_moved(editor, line, nt->linesAdded);
			else
				/* same-line change: we'll check it later */
				mod_line = line;
			break;
		case SCN_UPDATEUI:
			pos = sci_get_current_position(editor->sci);
			line = sci_get_line_from_position(editor->sci, pos);
			if(mod_line != -1 && line != mod_line)
			{
				/* cursor left a line that was changed, scan it for tokens */
				offset = scan_line_for_tokens(editor->sci, mod_line);
				if(offset)
					found_token(editor, mod_line, linebuf->str + offset);
				else
					no_token(editor, mod_line);
				render_taskstore(editor);
				mod_line = -1;
			}
			break;
	}

	return FALSE;
}


static gboolean tasks_button_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	GeanyDocument *doc;
	GtkTreeView *tv;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean ret = FALSE;
	unsigned int line;

	if (event->button == 1)
	{
		ret = TRUE;

		tv = GTK_TREE_VIEW(ui_lookup_widget(geany->main_widgets->window, "treeview_tasks"));
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
		if (gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			gtk_tree_model_get(model, &iter, 0, &line, -1);
			doc = document_get_current();
			ret = navqueue_goto_line(NULL, doc, line + 1);
		}
	}

	return ret;
}


static gboolean tasks_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GtkTreeView *tv;
	GdkEventButton button_event;

	if(event->keyval == GDK_Return ||
		event->keyval == GDK_ISO_Enter ||
		event->keyval == GDK_KP_Enter ||
		event->keyval == GDK_space)
	{
		button_event.button = 1;
		button_event.time = event->time;
		tv = GTK_TREE_VIEW(ui_lookup_widget(geany->main_widgets->window, "treeview_tasks"));
		tasks_button_cb(NULL, &button_event, tv);

		return TRUE;
	}

	return FALSE;
}


static void free_editor_tasks(gpointer key, gpointer value, gpointer data)
{
	GList *tasklist, *entry;
	GeanyTask *task;

	tasklist = (value) ? value : g_hash_table_lookup(globaltasks, key);
	if(tasklist)
	{
		for(entry = g_list_first(tasklist); entry; entry = g_list_next(entry))
		{
			task = (GeanyTask *) entry->data;
			g_string_free(task->description, TRUE);
			g_free(task);
		}
		g_list_free(tasklist);
	}
	g_hash_table_remove(globaltasks, key);

}


static void scan_all_documents(void)
{
	unsigned int i;

	for(i = 0; i < geany->documents_array->len; i++)
	{
		if(document_index(i)->is_valid)
		{
			scan_document_for_tasks(document_index(i));
		}
	}

}

/* go through every line of a document and scan it for tasks tokens. add the
 * task to the tasklist for that document if found. */
static void scan_document_for_tasks(GeanyDocument *doc)
{
	unsigned int lines, line, offset;

	lines = sci_get_line_count(doc->editor->sci);
	for(line = 0; line < lines; line++)
	{
		offset = scan_line_for_tokens(doc->editor->sci, line);
		if(offset)
			found_token(doc->editor, line, linebuf->str + offset);
	}
	render_taskstore(doc->editor);

}


static void create_tasks_tab(void)
{
	GtkWidget *tv, *notebook;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	int page;

	taskstore = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
	tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(taskstore));
	g_object_set_data(G_OBJECT(geany->main_widgets->window), "treeview_tasks", tv);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tv), FALSE);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(tv, "button-release-event", G_CALLBACK(tasks_button_cb), (gpointer) tv);
	g_signal_connect(tv, "key-press-event", G_CALLBACK(tasks_key_cb), (gpointer) tv);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tv), column);

	notebook = ui_lookup_widget(geany->main_widgets->window, "notebook_info");
	page = gtk_notebook_insert_page(GTK_NOTEBOOK(notebook), tv, gtk_label_new(_("Tasks")), -1);
	gtk_widget_show_all(tv);
	notebook_page = tv;
}


/* copy the line into linebuf and scan it for tokens. returns 0 if no tokens
 * were found, or the offset to the start of the task in linebuf otherwise. */
static int scan_line_for_tokens(ScintillaObject *sci, unsigned int line)
{
	unsigned int len, len_done, offset;

	offset = 0;
	len = sci_get_line_length(sci, line);
	if(len)
	{
		if(len+1 > linebuf->allocated_len)
		{
			/* why doesn't GString have this functionality? */
			linebuf->str = g_realloc(linebuf->str, len+1);
			if(linebuf->str == NULL)
				return 0;
			linebuf->allocated_len = len+1;
		}
		len_done = scintilla_send_message(sci, SCI_GETLINE, line, (sptr_t) linebuf->str);
		linebuf->str[len] = 0;
		if(len_done)
			offset = scan_buf_for_tokens(linebuf->str);
	}

	return offset;
}


static int scan_buf_for_tokens(char *buf)
{
	unsigned int t, offset, len, i;
	char *entry;

	offset = 0;
	for(t = 0; tokens[t]; t++)
	{
		entry = strstr(buf, tokens[t]);
		if(entry)
		{
			entry += strlen(tokens[t]);
			while(*entry == ' ' || *entry == ':')
				entry++;
			for(i = 0; entry[i]; i++)
			{
				/* strip off line endings */
				if(entry[i] == '\t' || entry[i] == '\r' || entry[i] == '\n')
				{
					entry[i] = 0;
					break;
				}
			}
			/* strip off */ /* no really, I mean */
			len = strlen(entry);
			if(len > 1 && entry[len-2] == '*' && entry[len-1] == '/')
				entry[len-2] = 0;
			offset = entry - buf;
		}
	}

	return offset;
}


static GeanyTask *create_task(unsigned int line, char *description)
{
	GeanyTask *task;

	task = malloc(sizeof(GeanyTask));
	g_return_val_if_fail(task != NULL, NULL);

	task->line = line;
	task->description = g_string_new(description);

	return task;
}


static int find_line(GeanyTask *task, unsigned int *line)
{

	if(task->line == *line)
		return 0;

	return 1;
}


static void found_token(GeanyEditor *editor, unsigned int line, char *description)
{
	GeanyTask *task;
	GList *tasklist, *entry;

	tasklist = g_hash_table_lookup(globaltasks, editor);
	if(tasklist)
	{
		entry = g_list_find_custom(tasklist, (gconstpointer) &line, (gconstpointer) find_line);
		if(entry)
		{
			task = (GeanyTask *) entry->data;
			if(strcmp(description, task->description->str))
				g_string_assign(task->description, description);
		}
		else
		{
			task = create_task(line, description);
			tasklist = g_list_append(tasklist, task);
			g_hash_table_replace(globaltasks, editor, tasklist);
		}
	}
	else
	{
		/* this editor doesn't have a tasklist yet */
		task = create_task(line, description);
		tasklist = g_list_append(NULL, task);
		g_hash_table_insert(globaltasks, editor, tasklist);
	}

}


/* no token was found on this line, make sure there's nothing in the tasklist either */
static void no_token(GeanyEditor *editor, unsigned int line)
{
	GList *tasklist, *entry;

	tasklist = g_hash_table_lookup(globaltasks, editor);
	if(tasklist)
	{
		entry = g_list_find_custom(tasklist, (gconstpointer) &line, (gconstpointer) find_line);
		if(entry)
		{
			tasklist = g_list_remove(tasklist, entry);
			g_hash_table_replace(globaltasks, editor, tasklist);
		}
	}

}


static void lines_moved(GeanyEditor *editor, unsigned int line, int change)
{
	GeanyTask *task;
	GList *tasklist, *entry, *to_delete;

	to_delete = NULL;
	tasklist = g_hash_table_lookup(globaltasks, editor);
	for(entry = g_list_first(tasklist); entry; entry = g_list_next(entry))
	{
		task = (GeanyTask *) entry->data;
		if(task->line >= line)
		{
			if(change < 0 && task->line < line - change)
				/* the line with this task on it was deleted, so mark the task for deletion */
				to_delete = g_list_append(to_delete, entry->data);
			else
				/* shift the line number of this task up or down along with the change */
				task->line += change;
		}
	}

	for(entry = g_list_first(to_delete); entry; entry = g_list_next(entry))
	{
		task = (GeanyTask *) entry->data;
		tasklist = g_list_remove(tasklist, entry->data);
		g_string_free(task->description, TRUE);
		g_free(task);
	}
	g_list_free(to_delete);

	g_hash_table_replace(globaltasks, editor, tasklist);
	render_taskstore(editor);

}


static int keysort(GeanyTask *a, GeanyTask *b)
{

	if(a->line < b->line)
		return -1;
	else if(a->line > b->line)
		return 1;

	return 0;
}


static void render_taskstore(GeanyEditor *editor)
{
	GeanyTask *task;
	GtkTreeIter iter;
	GList *tasklist, *entry;

	gtk_list_store_clear(taskstore);
	tasklist = g_hash_table_lookup(globaltasks, editor);
	if(!tasklist)
		/* empty list */
		return;
	tasklist = g_list_sort(tasklist, (GCompareFunc) keysort);
	g_hash_table_replace(globaltasks, editor, tasklist);

	for(entry = g_list_first(tasklist); entry; entry = g_list_next(entry))
	{
		task = (GeanyTask *) entry->data;
		gtk_list_store_append(taskstore, &iter);
		gtk_list_store_set(taskstore, &iter, 0, task->line, 1, task->description->str, -1);
	}

}



