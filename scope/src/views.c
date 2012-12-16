/*
 *  views.c
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "common.h"

typedef struct _ViewInfo
{
	gboolean dirty;
	void (*clear)(void);
	gboolean (*update)(void);
	gboolean flush;
	DebugState state;
} ViewInfo;

static ViewInfo views[VIEW_COUNT] =
{
	{ FALSE, NULL,           NULL,            FALSE, 0 },
	{ FALSE, threads_clear,  threads_update,  FALSE, DS_SENDABLE },
	{ FALSE, breaks_clear,   breaks_update,   FALSE, DS_SENDABLE },
	{ FALSE, stack_clear,    stack_update,    TRUE,  DS_DEBUG },
	{ FALSE, locals_clear,   locals_update,   TRUE,  DS_DEBUG },
	{ FALSE, watches_clear,  watches_update,  TRUE,  DS_DEBUG },
	{ FALSE, NULL,           dc_update,       FALSE, DS_DEBUG },
	{ FALSE, inspects_clear, inspects_update, FALSE, DS_DEBUG },
	{ FALSE, tooltip_clear,  tooltip_update,  FALSE, DS_SENDABLE },
	{ FALSE, menu_clear,     NULL,            FALSE, 0 }
};

void view_dirty(ViewIndex index)
{
	views[index].dirty = TRUE;
}

static void view_update_unconditional(ViewIndex index, DebugState state)
{
	ViewInfo *view = views + index;

	if (view->state & state)
	{
		if (view->update())
			view->dirty = FALSE;
	}
	else if (view->flush)
	{
		view->clear();
		view->dirty = FALSE;
	}
}

static void view_update(ViewIndex index, DebugState state)
{
	if (views[index].dirty)
		view_update_unconditional(index, state);
}

static ViewIndex view_current = 0;

void views_clear(void)
{
	ViewIndex i;
	ViewInfo *view = views;

	for (i = 0; i < VIEW_COUNT; i++, view++)
	{
		view->dirty = FALSE;

		if (view->clear)
			view->clear();
	}
}

void views_update(DebugState state)
{
	if (option_update_all_views)
	{
		ViewIndex i;

		if (thread_state == THREAD_QUERY_FRAME)
		{
			if (!views[VIEW_THREADS].dirty)
				thread_query_frame();

			thread_state = THREAD_STOPPED;
		}

		for (i = 0; i < VIEW_COUNT; i++)
		{
			if (views[i].dirty)
			{
				view_update_unconditional(i, state);

				if (i == VIEW_STACK && thread_state >= THREAD_STOPPED)
					i = VIEW_WATCHES;
			}
		}
	}
	else
	{
		if (thread_state == THREAD_QUERY_FRAME)
		{
			if (view_current != VIEW_THREADS || !views[VIEW_THREADS].dirty)
				thread_query_frame();

			thread_state = THREAD_STOPPED;
		}

		view_update(view_current, state);
		view_update(VIEW_TOOLTIP, state);

		if (inspects_current())
			view_update(VIEW_INSPECT, state);
	}
}

gboolean view_stack_update(void)
{
	if (views[VIEW_STACK].dirty)
	{
		DebugState state = thread_state >= THREAD_STOPPED ? DS_DEBUG : DS_READY;
		view_update_unconditional(VIEW_STACK, state);
		return state == DS_DEBUG;
	}

	return FALSE;
}

void view_inspect_update(void)
{
	view_update(VIEW_INSPECT, debug_state());
}

void on_view_changed(G_GNUC_UNUSED GtkNotebook *notebook, G_GNUC_UNUSED gpointer page,
	gint page_num, G_GNUC_UNUSED gpointer gdata)
{
	view_current = page_num;
	view_update(view_current, debug_state());
}

gboolean on_view_key_press(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event,
	ViewSeeker seeker)
{
	/* from msgwindow.c */
	gboolean enter_or_return = ui_is_keyval_enter_or_return(event->keyval);

	if (enter_or_return || event->keyval == GDK_space)
		seeker(enter_or_return);

	return FALSE;
}

gboolean on_view_button_1_press(GtkWidget *widget, GdkEventButton *event, ViewSeeker seeker)
{
	if (event->button == 1 && (pref_auto_view_source || event->type == GDK_2BUTTON_PRESS))
	{
		utils_handle_button_press(widget, event);
		seeker(event->type == GDK_2BUTTON_PRESS);
		return TRUE;
	}

	return FALSE;
}

gboolean on_view_query_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_tip,
	GtkTooltip *tooltip, GtkTreeViewColumn *base_name_column)
{
	GtkTreeView *tree = GTK_TREE_VIEW(widget);
	GtkTreeIter iter;

	if (gtk_tree_view_get_tooltip_context(tree, &x, &y, keyboard_tip, NULL, NULL, &iter))
	{
		const char *file;

		gtk_tree_view_set_tooltip_cell(tree, tooltip, NULL, base_name_column, NULL);
		gtk_tree_model_get(gtk_tree_view_get_model(tree), &iter, COLUMN_FILE, &file, -1);

		if (file)
		{
			gchar *utf8 = utils_get_utf8_from_locale(file);

			gtk_tooltip_set_text(tooltip, utf8);
			g_free(utf8);
			return TRUE;
		}
	}

	return FALSE;
}

GtkTreeView *view_create(const char *name, GtkTreeModel **model, GtkTreeSelection **selection)
{
	GtkTreeView *tree = GTK_TREE_VIEW(get_widget(name));

	*model = gtk_tree_view_get_model(tree);
	*selection = gtk_tree_view_get_selection(tree);
	return tree;
}

static void on_editing_started(G_GNUC_UNUSED GtkCellRenderer *cell, GtkCellEditable *editable,
	G_GNUC_UNUSED const gchar *path, GtkAdjustment *hadjustment)
{
	if (GTK_IS_ENTRY(editable))
		gtk_entry_set_cursor_hadjustment(GTK_ENTRY(editable), hadjustment);
}

static gboolean on_display_editable_map_event(GtkWidget *widget, G_GNUC_UNUSED GdkEvent *event,
	gchar *display)
{
	gint position = 0;
	GtkEditable *editable = GTK_EDITABLE(widget);

	gtk_editable_delete_text(editable, 0, -1);
	gtk_editable_insert_text(editable, display ? display : "", -1, &position);
	gtk_editable_select_region(editable, -1, 0);
	g_free(display);
	return FALSE;
}

static void on_display_editing_started(G_GNUC_UNUSED GtkCellRenderer *cell,
	GtkCellEditable *editable, const gchar *path_str, GtkTreeModel *model)
{
	GtkTreeIter iter;
	const char *value;
	gint hb_mode;

	g_assert(GTK_IS_EDITABLE(editable));
	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter, COLUMN_VALUE, &value, COLUMN_HB_MODE, &hb_mode, -1);
	/* scrolling editable to the proper position is left as an exercise for the reader */
	g_signal_connect(editable, "map-event", G_CALLBACK(on_display_editable_map_event),
		parse_get_display_from_7bit(value, hb_mode, MR_EDITVC));
}

GtkTreeView *view_connect(const char *name, GtkTreeModel **model, GtkTreeSelection **selection,
	const TreeCell *cell_info, const char *window, GObject **display)
{
	guint i;
	GtkScrolledWindow *scrolled = GTK_SCROLLED_WINDOW(get_widget(window));
	GtkAdjustment *hadjustment = gtk_scrolled_window_get_hadjustment(scrolled);
	GtkTreeView *tree = view_create(name, model, selection);

	for (i = 0; cell_info->name; cell_info++, i++)
	{
		GtkCellRenderer *cell = GTK_CELL_RENDERER(get_object(cell_info->name));
		const char *signame;
		const char *property;

		if (GTK_IS_CELL_RENDERER_TEXT(cell))
		{
			signame = "edited";
			property = "editable";

			g_signal_connect(cell, "editing-started", G_CALLBACK(on_editing_started),
				hadjustment);

			if (display && i == 0)
			{
				g_signal_connect(cell, "editing-started",
					G_CALLBACK(on_display_editing_started), *model);
				*display = G_OBJECT(cell);
			}
		}
		else
		{
			g_assert(GTK_IS_CELL_RENDERER_TOGGLE(cell));
			signame = "toggled";
			property = "activatable";
		}

		g_signal_connect(cell, signame, cell_info->callback, GINT_TO_POINTER(i));
		g_object_set(cell, property, TRUE, NULL);
	}

	return tree;
}

static void view_line_cell_data_func(G_GNUC_UNUSED GtkTreeViewColumn *column,
	GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, gpointer gdata)
{
	gint line;
	gchar *s;

	gtk_tree_model_get(model, iter, GPOINTER_TO_INT(gdata), &line, -1);
	s = line ? g_strdup_printf("%d", line) : NULL;
	g_object_set(cell, "text", s, NULL);
	g_free(s);
}

void view_set_line_data_func(const char *column, const char *cell, gint column_id)
{
	gtk_tree_view_column_set_cell_data_func(GTK_TREE_VIEW_COLUMN(get_object(column)),
		GTK_CELL_RENDERER(get_object(cell)), view_line_cell_data_func,
		GINT_TO_POINTER(column_id), NULL);
}

void view_display_edited(GtkTreeModel *model, GtkTreeIter *iter, const gchar *new_text,
	const char *format)
{
	const char *name;
	gint hb_mode;
	char *locale;

	gtk_tree_model_get(model, iter, COLUMN_NAME, &name, COLUMN_HB_MODE, &hb_mode, -1);
	locale = utils_get_locale_from_display(new_text, hb_mode);
	utils_str_replace_all(&locale, "\n", " ");
	debug_send_format(F, format, name, locale);
	g_free(locale);
}

void view_column_set_visible(const char *name, gboolean visible)
{
	gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(get_object(name)), visible);
}

void view_seek_selected(GtkTreeSelection *selection, gboolean focus, SeekerType seeker)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	const char *file;
	gint line;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, COLUMN_FILE, &file, COLUMN_LINE, &line, -1);
		if (file)
			utils_seek(file, line, focus, seeker);
	}
}

enum
{
	COMMAND_DISPLAY,
	COMMAND_TEXT,
	COMMAND_LOCALE
};

static GtkWidget *command_dialog;
static GtkWidget *command_view;
static GObject *command_cell;
static GtkTextBuffer *command_text;
static GtkComboBox *command_history;
static GtkTreeModel *command_model;
static GtkListStore *command_store;
static GtkToggleButton *command_locale;
static GtkWidget *command_send;

static void on_command_text_changed(GtkTextBuffer *command_text, G_GNUC_UNUSED gpointer gdata)
{
	gchar *text = utils_text_buffer_get_text(command_text, -1);
	const gchar *start = utils_skip_spaces(text);

	gtk_widget_set_sensitive(command_send, *start != '0' || !isdigit(start[1]));
	g_free(text);
}

gboolean on_command_dialog_configure(G_GNUC_UNUSED GtkWidget *widget,
	G_GNUC_UNUSED GdkEventButton *event, G_GNUC_UNUSED gpointer gdata)
{
	gint width;

#if GTK_CHECK_VERSION(2, 24, 0)
	width = gdk_window_get_width(command_view->window);
#else
	gint height;
	gdk_drawable_get_size(GDK_DRAWABLE(command_view->window), &width, &height);
#endif
	g_object_set(command_cell, "wrap-width", width, NULL);
	return FALSE;
}

static void on_command_history_size_request(G_GNUC_UNUSED GtkWidget *widget,
	GtkRequisition *requisition, G_GNUC_UNUSED gpointer gdata)
{
	static gint empty_height;
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_first(command_model, &iter))
		requisition->height = empty_height;
	else
		empty_height = requisition->height;
}

static void on_command_history_changed(GtkComboBox *command_history,
	G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(command_history, &iter))
	{
		const gchar *text;
		gboolean locale;

		gtk_tree_model_get(command_model, &iter, COMMAND_TEXT, &text, COMMAND_LOCALE,
			&locale, -1);
		gtk_text_buffer_set_text(command_text, text, -1);
		gtk_toggle_button_set_active(command_locale, locale);
		gtk_widget_grab_focus(command_view);
		gtk_combo_box_set_active_iter(command_history, NULL);
	}
}

static void on_command_insert_button_clicked(G_GNUC_UNUSED GtkButton *button, gpointer gdata)
{
	const char *prefix, *id;
	GString *text = g_string_new("--");

	switch (GPOINTER_TO_INT(gdata))
	{
		case 't' : prefix = "thread"; id = thread_id; break;
		case 'g' : prefix = "group"; id = thread_group_id(); break;
		default : prefix = "frame"; id = frame_id;
	}

	g_string_append_printf(text, "%s ", prefix);
	if (id)
		g_string_append_printf(text, "%s ", id);
	gtk_text_buffer_delete_selection(command_text, FALSE, TRUE);
	gtk_text_buffer_insert_at_cursor(command_text, text->str, -1);
	g_string_free(text, TRUE);
	gtk_widget_grab_focus(command_view);

}

static void on_command_send_button_clicked(G_GNUC_UNUSED GtkButton *button,
	G_GNUC_UNUSED gpointer gdata)
{
	gchar *text = utils_text_buffer_get_text(command_text, -1);
	const gchar *start;
	char *locale;

	utils_str_replace_all(&text, "\n", " ");
	start = utils_skip_spaces(text);
	locale = gtk_toggle_button_get_active(command_locale) ?
		utils_get_locale_from_utf8(start) : strdup(start);
	debug_send_command(N, locale);
	g_free(locale);
	gtk_widget_hide(command_dialog);

	if (*start)
	{
		GtkTreePath *path;
		GtkTreeIter iter;
		gchar *display = strdup(start);

		/* from ui_combo_box_add_to_history() */
		if (model_find(command_model, &iter, COMMAND_TEXT, start))
			gtk_list_store_remove(command_store, &iter);

		if (strlen(display) >= 273)
			strcpy(display + 270, _("\342\200\246"));  /* For translators: ellipsis */

		gtk_list_store_prepend(command_store, &iter);
		gtk_list_store_set(command_store, &iter, COMMAND_DISPLAY, display, COMMAND_TEXT,
			start, COMMAND_LOCALE, gtk_toggle_button_get_active(command_locale), -1);
		g_free(display);

		path = gtk_tree_path_new_from_indices(15, -1);
		if (gtk_tree_model_get_iter(command_model, &iter, path))
			gtk_list_store_remove(command_store, &iter);
		gtk_tree_path_free(path);
	}

	g_free(text);
}

static void command_line_update_state(DebugState state)
{
	if (state == DS_INACTIVE)
		gtk_widget_hide(command_dialog);
	else
	{
		gtk_button_set_label(GTK_BUTTON(command_send),
			state & DS_SENDABLE ? _("_Send") : _("_Busy"));
	}
}

void view_command_line(const gchar *text, const gchar *title, const gchar *seek,
	gboolean seek_after)
{
	gtk_window_set_title(GTK_WINDOW(command_dialog), title ? title : _("GDB Command"));
	gtk_widget_grab_focus(command_view);

	if (text)
	{
		const gchar *pos = seek ? strstr(text, seek) : NULL;
		GtkTextIter iter;

		gtk_text_buffer_set_text(command_text, text, -1);
		gtk_text_buffer_get_iter_at_offset(command_text, &iter,
			g_utf8_strlen(text, pos ? pos + strlen(seek) * seek_after - text : -1));
		gtk_text_buffer_place_cursor(command_text, &iter);
	}

	on_command_text_changed(command_text, NULL);
	command_line_update_state(debug_state());
	gtk_combo_box_set_active_iter(command_history, NULL);
	gtk_dialog_run(GTK_DIALOG(command_dialog));
}

gboolean view_command_active(void)
{
	return gtk_widget_get_visible(command_dialog);
}

void views_update_state(DebugState state)
{
	static DebugState last_state = 0;

	if (state != last_state)
	{
		if (gtk_widget_get_visible(command_dialog))
			command_line_update_state(state);
		locals_update_state(state);
		watches_update_state(state);
		inspects_update_state(state);
		last_state = state;
	}
}

void views_init(void)
{
	command_dialog = dialog_connect("command_dialog");
	command_view = get_widget("command_view");
	command_text = gtk_text_view_get_buffer(GTK_TEXT_VIEW(command_view));
	g_signal_connect(command_text, "changed", G_CALLBACK(on_command_text_changed), NULL);
	command_history = GTK_COMBO_BOX(get_widget("command_history"));
	command_model = gtk_combo_box_get_model(command_history);
	command_store = GTK_LIST_STORE(command_model);
	command_cell = get_object("command_cell");
	g_signal_connect(command_dialog, "configure-event",
		G_CALLBACK(on_command_dialog_configure), NULL);
	g_signal_connect(command_history, "size-request",
		G_CALLBACK(on_command_history_size_request), NULL);
	g_signal_connect(command_history, "changed", G_CALLBACK(on_command_history_changed),
		NULL);
	command_locale = GTK_TOGGLE_BUTTON(get_widget("command_locale"));

	g_signal_connect(get_widget("command_thread"), "clicked",
		G_CALLBACK(on_command_insert_button_clicked), GINT_TO_POINTER('t'));
	g_signal_connect(get_widget("command_group"), "clicked",
		G_CALLBACK(on_command_insert_button_clicked), GINT_TO_POINTER('g'));
	g_signal_connect(get_widget("command_frame"), "clicked",
		G_CALLBACK(on_command_insert_button_clicked), GINT_TO_POINTER('f'));
	command_send = get_widget("command_send");
	gtk_widget_grab_default(command_send);
	g_signal_connect(command_send, "clicked", G_CALLBACK(on_command_send_button_clicked),
		NULL);
	utils_enter_to_clicked(command_view, command_send);
}

void views_finalize(void)
{
	gtk_widget_destroy(GTK_WIDGET(command_dialog));
}
