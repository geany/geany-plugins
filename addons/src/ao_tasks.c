/*
 *      ao_tasks.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */


#include <string.h>
#include <gtk/gtk.h>
#include <glib-object.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "addons.h"
#include "ao_tasks.h"

#include <gdk/gdkkeysyms.h>


typedef struct _AoTasksPrivate AoTasksPrivate;

#define AO_TASKS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
	AO_TASKS_TYPE, AoTasksPrivate))

struct _AoTasks
{
	GObject parent;
};

struct _AoTasksClass
{
	GObjectClass parent_class;
};

struct _AoTasksPrivate
{
	gboolean enable_tasks;
	gboolean active;

	GtkListStore *store;
	GtkWidget *tree;

	GtkWidget *page;
	GtkWidget *popup_menu;
	GtkWidget *popup_menu_delete_button;

	gchar **tokens;

	gboolean scan_all_documents;

	GHashTable *selected_tasks;
	gint selected_task_line;
	GeanyDocument *selected_task_doc;
	gboolean ignore_selection_changed;
};

enum
{
	PROP_0,
	PROP_ENABLE_TASKS,
	PROP_TOKENS,
	PROP_SCAN_ALL_DOCUMENTS
};

enum
{
	TLIST_COL_FILENAME,
	TLIST_COL_DISPLAY_FILENAME,
	TLIST_COL_LINE,
	TLIST_COL_TOKEN,
	TLIST_COL_NAME,
	TLIST_COL_TOOLTIP,
	TLIST_COL_MAX
};

static void ao_tasks_finalize  			(GObject *object);
static void ao_tasks_show				(AoTasks *t);
static void ao_tasks_hide				(AoTasks *t);

G_DEFINE_TYPE(AoTasks, ao_tasks, G_TYPE_OBJECT)


static void ao_tasks_set_property(GObject *object, guint prop_id,
								  const GValue *value, GParamSpec *pspec)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_TASKS:
		{
			gboolean new_val = g_value_get_boolean(value);
			if (new_val && ! priv->enable_tasks)
				ao_tasks_show(AO_TASKS(object));
			if (! new_val && priv->enable_tasks)
				ao_tasks_hide(AO_TASKS(object));

			priv->enable_tasks = new_val;
			if (priv->enable_tasks && main_is_realized() && ! priv->active)
				ao_tasks_set_active(AO_TASKS(object));
			break;
		}
		case PROP_SCAN_ALL_DOCUMENTS:
		{
			priv->scan_all_documents = g_value_get_boolean(value);
			break;
		}
		case PROP_TOKENS:
		{
			const gchar *t = g_value_get_string(value);
			if (EMPTY(t))
				t = "TODO;FIXME"; /* fallback */
			g_strfreev(priv->tokens);
			priv->tokens = g_strsplit(t, ";", -1);
			ao_tasks_update(AO_TASKS(object), NULL);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void ao_tasks_class_init(AoTasksClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = ao_tasks_finalize;
	g_object_class->set_property = ao_tasks_set_property;
	g_type_class_add_private(klass, sizeof(AoTasksPrivate));

	g_object_class_install_property(g_object_class,
									PROP_SCAN_ALL_DOCUMENTS,
									g_param_spec_boolean(
									"scan-all-documents",
									"scan-all-documents",
									"Whether to show tasks for all open documents",
									TRUE,
									G_PARAM_WRITABLE));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_TASKS,
									g_param_spec_boolean(
									"enable-tasks",
									"enable-tasks",
									"Whether to show list of defined tasks",
									TRUE,
									G_PARAM_WRITABLE));

	g_object_class_install_property(g_object_class,
									PROP_TOKENS,
									g_param_spec_string(
									"tokens",
									"tokens",
									"The tokens to scan documents for",
									NULL,
									G_PARAM_WRITABLE));
}


static void ao_tasks_finalize(GObject *object)
{
	AoTasksPrivate *priv;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_AO_TASKS(object));

	priv = AO_TASKS_GET_PRIVATE(object);
	g_strfreev(priv->tokens);

	ao_tasks_hide(AO_TASKS(object));

	if (priv->selected_tasks != NULL)
		g_hash_table_destroy(priv->selected_tasks);

	G_OBJECT_CLASS(ao_tasks_parent_class)->finalize(object);
}


static gboolean ao_tasks_selection_changed_cb(gpointer t)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->tree));
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gint line;
		gchar *filename, *locale_filename;
		GeanyDocument *doc;

		gtk_tree_model_get(model, &iter,
			TLIST_COL_LINE, &line,
			TLIST_COL_FILENAME, &filename,
			-1);
		locale_filename = utils_get_locale_from_utf8(filename);
		doc = document_open_file(locale_filename, FALSE, NULL, NULL);
		if (doc != NULL)
		{
			sci_goto_line(doc->editor->sci, line - 1, TRUE);
			gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
		}

		/* remember the selected line for this document to restore the selection after an update */
		if (priv->scan_all_documents)
		{
			priv->selected_task_doc = doc;
			priv->selected_task_line = line;
		}
		else
			g_hash_table_insert(priv->selected_tasks, doc, GINT_TO_POINTER(line));

		g_free(filename);
		g_free(locale_filename);
	}
	return FALSE;
}


static gboolean ao_tasks_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button == 1)
	{	/* allow reclicking of a treeview item */
		g_idle_add(ao_tasks_selection_changed_cb, data);
	}
	else if (event->button == 3)
	{
		AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(data);
		gboolean has_selection = gtk_tree_selection_get_selected(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->tree)), NULL, NULL);
		gtk_widget_set_sensitive(priv->popup_menu_delete_button, has_selection);

		gtk_menu_popup(GTK_MENU(priv->popup_menu), NULL, NULL, NULL, NULL,
				event->button, event->time);
		/* don't return TRUE here, otherwise the selection won't be changed */
	}
	return FALSE;
}


static gboolean ao_tasks_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Return ||
		event->keyval == GDK_ISO_Enter ||
		event->keyval == GDK_KP_Enter ||
		event->keyval == GDK_space)
	{
		g_idle_add(ao_tasks_selection_changed_cb, data);
	}
	if ((event->keyval == GDK_F10 && event->state & GDK_SHIFT_MASK) || event->keyval == GDK_Menu)
	{
		GdkEventButton button_event;

		button_event.time = event->time;
		button_event.button = 3;

		ao_tasks_button_press_cb(widget, &button_event, data);
		return TRUE;
	}
	return FALSE;
}


static void ao_tasks_hide(AoTasks *t)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	if (priv->page)
	{
		gtk_widget_destroy(priv->page);
		priv->page = NULL;
	}
	if (priv->popup_menu)
	{
		g_object_unref(priv->popup_menu);
		priv->popup_menu = NULL;
	}
}


static void popup_delete_item_click_cb(GtkWidget *button, AoTasks *t)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->tree));
	gchar *filename;
	gint line, start, end;
	GeanyDocument *doc;

	if (! gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	/* get task information */
	gtk_tree_model_get(model, &iter,
		TLIST_COL_FILENAME, &filename,
		TLIST_COL_LINE, &line,
		-1);

	/* find the document of this task item */
	doc = document_find_by_filename(filename);
	g_free(filename);

	if (doc == NULL)
		return;

	line--; /* display line vs. document line */

	start = sci_get_position_from_line(doc->editor->sci, line);
	end = sci_get_position_from_line(doc->editor->sci, line + 1);
	if (end == -1)
		end = sci_get_length(doc->editor->sci);

	/* create a selection over this line and then delete it */
	scintilla_send_message(doc->editor->sci, SCI_SETSEL, start, end);
	scintilla_send_message(doc->editor->sci, SCI_CLEAR, 0, 0);
	/* update the task list */
	ao_tasks_update(t, doc);
}


static void popup_update_item_click_cb(GtkWidget *button, AoTasks *t)
{
	ao_tasks_update(t, NULL);
}


static void popup_hide_item_click_cb(GtkWidget *button, AoTasks *t)
{
	keybindings_send_command(GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_MESSAGEWINDOW);
}


static GtkWidget *create_popup_menu(AoTasks *t)
{
	GtkWidget *item, *menu;
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	menu = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
	priv->popup_menu_delete_button = item;
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_delete_item_click_cb), t);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_REFRESH, _("_Update"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_update_item_click_cb), t);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Hide Message Window"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_hide_item_click_cb), t);

	return menu;
}


static void ao_tasks_show(AoTasks *t)
{
	GtkCellRenderer *text_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeSortable *sortable;
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	priv->store = gtk_list_store_new(TLIST_COL_MAX,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	priv->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(priv->store));

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	/* selection handling */
	g_signal_connect(priv->tree, "button-press-event", G_CALLBACK(ao_tasks_button_press_cb), t);
	g_signal_connect(priv->tree, "key-press-event", G_CALLBACK(ao_tasks_key_press_cb), t);

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("File"));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text",
		TLIST_COL_DISPLAY_FILENAME, NULL);
	gtk_tree_view_column_set_sort_indicator(column, FALSE);
	gtk_tree_view_column_set_sort_column_id(column, TLIST_COL_DISPLAY_FILENAME);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree), column);

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Line"));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", TLIST_COL_LINE, NULL);
	gtk_tree_view_column_set_sort_indicator(column, FALSE);
	gtk_tree_view_column_set_sort_column_id(column, TLIST_COL_LINE);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree), column);

	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Type"));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", TLIST_COL_TOKEN, NULL);
	gtk_tree_view_column_set_sort_indicator(column, FALSE);
	gtk_tree_view_column_set_sort_column_id(column, TLIST_COL_TOKEN);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree), column);

	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Task"));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", TLIST_COL_NAME, NULL);
	gtk_tree_view_column_set_sort_indicator(column, FALSE);
	gtk_tree_view_column_set_sort_column_id(column, TLIST_COL_NAME);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree), column);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(priv->tree), TRUE);
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(priv->tree), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(priv->tree), TLIST_COL_DISPLAY_FILENAME);

	/* sorting */
	sortable = GTK_TREE_SORTABLE(GTK_TREE_MODEL(priv->store));
	gtk_tree_sortable_set_sort_column_id(sortable, TLIST_COL_DISPLAY_FILENAME, GTK_SORT_ASCENDING);

	ui_widget_modify_font_from_string(priv->tree, geany->interface_prefs->tagbar_font);

	/* GTK 2.12 tooltips */
	if (gtk_check_version(2, 12, 0) == NULL)
		g_object_set(priv->tree, "has-tooltip", TRUE, "tooltip-column", TLIST_COL_TOOLTIP, NULL);

	/* scrolled window */
	priv->page = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->page),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(priv->page), priv->tree);

	gtk_widget_show_all(priv->page);
	gtk_notebook_append_page(
		GTK_NOTEBOOK(ui_lookup_widget(geany->main_widgets->window, "notebook_info")),
		priv->page,
		gtk_label_new(_("Tasks")));

	priv->popup_menu = create_popup_menu(t);
	g_object_ref_sink(priv->popup_menu);
}


void ao_tasks_activate(AoTasks *t)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	if (priv->enable_tasks)
	{
		GtkNotebook *notebook = GTK_NOTEBOOK(geany->main_widgets->message_window_notebook);
		gint page_number = gtk_notebook_page_num(notebook, priv->page);

		gtk_notebook_set_current_page(notebook, page_number);
		gtk_widget_grab_focus(priv->tree);
	}
}


void ao_tasks_remove(AoTasks *t, GeanyDocument *cur_doc)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);
	GtkTreeModel *model = GTK_TREE_MODEL(priv->store);
	GtkTreeIter iter;
	gchar *filename;

	if (! priv->active || ! priv->enable_tasks)
		return;

	if (gtk_tree_model_get_iter_first(model, &iter))
	{
		gboolean has_next;

		do
		{
			gtk_tree_model_get(model, &iter, TLIST_COL_FILENAME, &filename, -1);

			if (utils_str_equal(filename, cur_doc->file_name))
			{	/* gtk_list_store_remove() manages the iter and set it to the next row */
				has_next = gtk_list_store_remove(priv->store, &iter);
			}
			else
			{	/* if we didn't delete the row, we need to manage the iter manually */
				has_next = gtk_tree_model_iter_next(model, &iter);
			}
			g_free(filename);
		}
		while (has_next);
	}
}


static void create_task(AoTasks *t, GeanyDocument *doc, gint line, const gchar *token,
						const gchar *line_buf, const gchar *task_start, const gchar *display_name)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);
	gchar *context, *tooltip;

	/* retrieve the following line and use it for the tooltip */
	context = g_strstrip(sci_get_line(doc->editor->sci, line + 1));
	setptr(context, g_strconcat(
		_("Context:"), "\n", line_buf, "\n", context, NULL));
	tooltip = g_markup_escape_text(context, -1);

	/* add the task into the list */
	gtk_list_store_insert_with_values(priv->store, NULL, -1,
		TLIST_COL_FILENAME, DOC_FILENAME(doc),
		TLIST_COL_DISPLAY_FILENAME, display_name,
		TLIST_COL_LINE, line + 1,
		TLIST_COL_TOKEN, token,
		TLIST_COL_NAME, task_start,
		TLIST_COL_TOOLTIP, tooltip,
		-1);
	g_free(context);
	g_free(tooltip);
}


static void update_tasks_for_doc(AoTasks *t, GeanyDocument *doc)
{
	gint lexer, lines, line, last_pos = 0, style;
	gchar *line_buf, *display_name, *task_start, *closing_comment;
	gchar **token;
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	if (doc->is_valid)
	{
		display_name = document_get_basename_for_display(doc, -1);
		lexer = sci_get_lexer(doc->editor->sci);
		lines = sci_get_line_count(doc->editor->sci);
		for (line = 0; line < lines; line++)
		{
			line_buf = sci_get_line(doc->editor->sci, line);
			for (token = priv->tokens; *token != NULL; ++token)
			{
				if (EMPTY(*token))
					continue;
				if ((task_start = strstr(line_buf, *token)) == NULL)
					continue;
				style = sci_get_style_at(doc->editor->sci, last_pos + (task_start - line_buf));
				if (!highlighting_is_comment_style(lexer, style))
					continue;

				/* skip the token and additional whitespace */
				task_start = strstr(g_strstrip(line_buf), *token) + strlen(*token);
				while (*task_start == ' ' || *task_start == ':')
					task_start++;
				/* reset task_start in case there is no text following */
				if (EMPTY(task_start))
					task_start = line_buf;
				else if ((EMPTY(doc->file_type->comment_single) ||
					strstr(line_buf, doc->file_type->comment_single) == NULL) &&
					!EMPTY(doc->file_type->comment_close) &&
					(closing_comment = strstr(task_start, doc->file_type->comment_close)) != NULL)
					*closing_comment = '\0';
				/* create the task */
				create_task(t, doc, line, *token, line_buf, task_start, display_name);
				/* if we found a token, continue on next line */
				break;
			}
			g_free(line_buf);
			last_pos = sci_get_line_end_position(doc->editor->sci, line) + 1;
		}
		g_free(display_name);
	}
}


void ao_tasks_update_single(AoTasks *t, GeanyDocument *cur_doc)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	if (! priv->active || ! priv->enable_tasks)
		return;

	if (! priv->scan_all_documents)
	{
		/* update */
		gtk_list_store_clear(priv->store);
		ao_tasks_update(t, cur_doc);
	}
}


static gboolean ao_tasks_select_task(GtkTreeModel *model, GtkTreePath *path,
									 GtkTreeIter *iter, gpointer data)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(data);
	gint line, selected_line;
	gchar *filename = NULL;
	const gchar *selected_filename = NULL;
	gboolean ret = FALSE;

	if (priv->scan_all_documents)
	{
		gtk_tree_model_get(model, iter, TLIST_COL_LINE, &line, TLIST_COL_FILENAME, &filename, -1);
		selected_line = priv->selected_task_line;
		selected_filename = DOC_FILENAME(priv->selected_task_doc);
	}
	else
	{
		gtk_tree_model_get(model, iter, TLIST_COL_LINE, &line, -1);
		selected_line = GPOINTER_TO_INT(g_hash_table_lookup(
							priv->selected_tasks, priv->selected_task_doc));
	}

	if (line == selected_line && utils_str_equal(filename, selected_filename))
	{
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->tree));

		gtk_tree_selection_select_iter(selection, iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(priv->tree), path, NULL, FALSE, 0.0, 0.0);

		ret = TRUE;
	}
	g_free(filename);
	return ret;
}


void ao_tasks_set_active(AoTasks *t)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	if (priv->enable_tasks)
	{
		priv->active = TRUE;
		ao_tasks_update(t, NULL);
	}
}


void ao_tasks_update(AoTasks *t, GeanyDocument *cur_doc)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	if (! priv->active || ! priv->enable_tasks)
		return;

	if (! priv->scan_all_documents && cur_doc == NULL)
	{
		/* clear all */
		gtk_list_store_clear(priv->store);
		/* get the current document */
		cur_doc = document_get_current();
	}

	if (cur_doc != NULL)
	{
		/* TODO handle renaming of files, probably we need a new signal for this */
		ao_tasks_remove(t, cur_doc);
		update_tasks_for_doc(t, cur_doc);
	}
	else
	{
		guint i;
		/* clear all */
		gtk_list_store_clear(priv->store);
		/* iterate over all docs */
		foreach_document(i)
		{
			update_tasks_for_doc(t, documents[i]);
		}
	}
	/* restore selection */
	priv->ignore_selection_changed = TRUE;
	if (priv->scan_all_documents && priv->selected_task_doc != NULL)
	{
		gtk_tree_model_foreach(GTK_TREE_MODEL(priv->store), ao_tasks_select_task, t);
	}
	else if (cur_doc != NULL && g_hash_table_lookup(priv->selected_tasks, cur_doc) != NULL)
	{
		priv->selected_task_doc = cur_doc;
		gtk_tree_model_foreach(GTK_TREE_MODEL(priv->store), ao_tasks_select_task, t);
	}
	priv->ignore_selection_changed = FALSE;
}


static void ao_tasks_init(AoTasks *self)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(self);

	priv->page = NULL;
	priv->popup_menu = NULL;
	priv->tokens = NULL;
	priv->active = FALSE;
	priv->ignore_selection_changed = FALSE;

	priv->selected_task_line = 0;
	priv->selected_task_doc = 0;
	if (priv->scan_all_documents)
		priv->selected_tasks = NULL;
	else
		priv->selected_tasks = g_hash_table_new(g_direct_hash, g_direct_equal);
}


AoTasks *ao_tasks_new(gboolean enable, const gchar *tokens, gboolean scan_all_documents)
{
	return g_object_new(AO_TASKS_TYPE,
		"scan-all-documents", scan_all_documents,
		"tokens", tokens,
		"enable-tasks", enable, NULL);
}
