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

typedef enum _ViewContext
{
	VC_NONE,
	VC_DATA,
	VC_FRAME
} ViewContext;

typedef struct _ViewInfo
{
	gboolean dirty;
	ViewContext context;
	void (*clear)(void);
	gboolean (*update)(void);
	gboolean flush;
	DebugState state;
} ViewInfo;

static ViewInfo views[VIEW_COUNT] =
{
	{ FALSE, VC_NONE,  NULL,            NULL,             FALSE, 0 },
	{ FALSE, VC_NONE,  threads_clear,   threads_update,   FALSE, DS_SENDABLE },
	{ FALSE, VC_NONE,  breaks_clear,    breaks_update,    FALSE, DS_SENDABLE },
	{ FALSE, VC_DATA,  stack_clear,     stack_update,     TRUE,  DS_DEBUG },
	{ FALSE, VC_FRAME, locals_clear,    locals_update,    TRUE,  DS_DEBUG },
	{ FALSE, VC_FRAME, watches_clear,   watches_update,   FALSE, DS_VARIABLE },
	{ FALSE, VC_DATA,  memory_clear,    memory_update,    FALSE, DS_VARIABLE },
	{ FALSE, VC_NONE,  NULL,            dc_update,        FALSE, DS_DEBUG },
	{ FALSE, VC_FRAME, inspects_clear,  inspects_update,  FALSE, DS_VARIABLE },
	{ FALSE, VC_FRAME, registers_clear, registers_update, TRUE,  DS_DEBUG },
	{ FALSE, VC_DATA,  tooltip_clear,   tooltip_update,   FALSE, DS_SENDABLE },
	{ FALSE, VC_NONE,  menu_clear,      NULL,             FALSE, 0 }
};

static void view_update_dirty(ViewIndex index, DebugState state)
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
		view_update_dirty(index, state);
}

static GtkNotebook *geany_sidebar;
static GtkWidget *inspect_page;
static GtkWidget *register_page;

static void views_sidebar_update(gint page_num, DebugState state)
{
	GtkWidget *page = gtk_notebook_get_nth_page(geany_sidebar, page_num);

	if (page == inspect_page)
		view_update(VIEW_INSPECT, state);
	else if (page == register_page)
		view_update(VIEW_REGISTERS, state);
}

void view_dirty(ViewIndex index)
{
	views[index].dirty = TRUE;
}

void views_context_dirty(DebugState state, gboolean frame_only)
{
	ViewIndex i;

	for (i = 0; i < VIEW_COUNT; i++)
		if (views[i].context >= (frame_only ? VC_FRAME : VC_DATA))
			view_dirty(i);

	if (state != DS_BUSY)
	{
		if (option_update_all_views)
			views_update(state);
		else
			views_sidebar_update(gtk_notebook_get_current_page(geany_sidebar), state);
	}
}

#ifdef G_OS_UNIX
static ViewIndex view_current = VIEW_TERMINAL;
#else
static ViewIndex view_current = VIEW_THREADS;
#endif

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
		gboolean skip_frame = FALSE;

		if (thread_state == THREAD_QUERY_FRAME)
		{
			if (!views[VIEW_THREADS].dirty)
				thread_query_frame('4');

			thread_state = THREAD_STOPPED;
		}

		for (i = 0; i < VIEW_COUNT; i++)
		{
			if (views[i].dirty && (!skip_frame || views[i].context != VC_FRAME))
			{
				view_update_dirty(i, state);

				if (i == VIEW_STACK && thread_state >= THREAD_STOPPED)
					skip_frame = TRUE;
			}
		}
	}
	else
	{
		if (thread_state == THREAD_QUERY_FRAME)
		{
			if (view_current != VIEW_THREADS || !views[VIEW_THREADS].dirty)
				thread_query_frame('4');

			thread_state = THREAD_STOPPED;
		}

		view_update(view_current, state);
		view_update(VIEW_TOOLTIP, state);
		views_sidebar_update(gtk_notebook_get_current_page(geany_sidebar), state);
	}
}

gboolean view_stack_update(void)
{
	if (views[VIEW_STACK].dirty)
	{
		DebugState state = thread_state >= THREAD_STOPPED ? DS_DEBUG : DS_READY;
		view_update_dirty(VIEW_STACK, state);
		return state == DS_DEBUG;
	}

	return FALSE;
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

	if (enter_or_return || event->keyval == GDK_space || event->keyval == GDK_KP_Space)
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

gboolean on_view_query_base_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_tip,
	GtkTooltip *tooltip, GtkTreeViewColumn *base_name_column)
{
	GtkTreeView *tree = GTK_TREE_VIEW(widget);
	GtkTreeIter iter;

	if (gtk_tree_view_get_tooltip_context(tree, &x, &y, keyboard_tip, NULL, NULL, &iter))
	{
		const char *file;

		gtk_tree_view_set_tooltip_cell(tree, tooltip, NULL, base_name_column, NULL);
		scp_tree_store_get((ScpTreeStore *) gtk_tree_view_get_model(tree), &iter,
			COLUMN_FILE, &file, -1);

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

gboolean on_view_editable_map(GtkWidget *widget, gchar *replace)
{
	iff (GTK_IS_EDITABLE(widget), "cell editable: not an editable")
	{
		gint position = 0;
		GtkEditable *editable = GTK_EDITABLE(widget);

		gtk_editable_delete_text(editable, 0, -1);
		gtk_editable_insert_text(editable, replace ? replace : "", -1, &position);
		gtk_editable_select_region(editable, -1, 0);
		g_free(replace);
	}

	return FALSE;
}

GtkTreeView *view_create(const char *name, ScpTreeStore **store, GtkTreeSelection **selection)
{
	GtkTreeView *tree = GTK_TREE_VIEW(get_widget(name));

	*store = SCP_TREE_STORE(gtk_tree_view_get_model(tree));
	*selection = gtk_tree_view_get_selection(tree);
	return tree;
}

static void on_editing_started(G_GNUC_UNUSED GtkCellRenderer *cell, GtkCellEditable *editable,
	G_GNUC_UNUSED const gchar *path, GtkAdjustment *hadjustment)
{
	if (GTK_IS_ENTRY(editable))
		gtk_entry_set_cursor_hadjustment(GTK_ENTRY(editable), hadjustment);
}

static void on_display_editing_started(G_GNUC_UNUSED GtkCellRenderer *cell,
	GtkCellEditable *editable, const gchar *path_str, ScpTreeStore *store)
{
	GtkTreeIter iter;
	const char *value;
	gint hb_mode;

	scp_tree_store_get_iter_from_string(store, &iter, path_str);
	scp_tree_store_get(store, &iter, COLUMN_VALUE, &value, COLUMN_HB_MODE, &hb_mode, -1);
	/* scrolling editable to the proper position is left as an exercise for the reader */
	g_signal_connect(editable, "map", G_CALLBACK(on_view_editable_map),
		parse_get_display_from_7bit(value, hb_mode, MR_EDITVC));
}

GtkTreeView *view_connect(const char *name, ScpTreeStore **store, GtkTreeSelection **selection,
	const TreeCell *cell_info, const char *window, GObject **display_cell)
{
	guint i;
	GtkScrolledWindow *scrolled = GTK_SCROLLED_WINDOW(get_widget(window));
	GtkAdjustment *hadjustment = gtk_scrolled_window_get_hadjustment(scrolled);
	GtkTreeView *tree = view_create(name, store, selection);

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

			if (display_cell && i == 0)
			{
				g_signal_connect(cell, "editing-started",
					G_CALLBACK(on_display_editing_started), *store);
				*display_cell = G_OBJECT(cell);
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
	gtk_tree_view_column_set_cell_data_func(get_column(column),
		GTK_CELL_RENDERER(get_object(cell)), view_line_cell_data_func,
		GINT_TO_POINTER(column_id), NULL);
}

void view_display_edited(ScpTreeStore *store, gboolean condition, const gchar *path_str,
	const char *format, gchar *new_text)
{
	if (validate_column(new_text, TRUE))
	{
		if (condition)
		{
			GtkTreeIter iter;
			const char *name;
			gint hb_mode;
			char *locale;

			scp_tree_store_get_iter_from_string(store, &iter, path_str);
			scp_tree_store_get(store, &iter, COLUMN_NAME, &name, COLUMN_HB_MODE,
				&hb_mode, -1);
			locale = utils_get_locale_from_display(new_text, hb_mode);
			utils_strchrepl(locale, '\n', ' ');
			debug_send_format(F, format, name, locale);
			g_free(locale);
		}
		else
			plugin_blink();
	}
}

void view_column_set_visible(const char *name, gboolean visible)
{
	gtk_tree_view_column_set_visible(get_column(name), visible);
}

void view_seek_selected(GtkTreeSelection *selection, gboolean focus, SeekerType seeker)
{
	ScpTreeStore *store;
	GtkTreeIter iter;

	if (scp_tree_selection_get_selected(selection, &store, &iter))
	{
		const char *file;
		gint line;

		scp_tree_store_get(store, &iter, COLUMN_FILE, &file, COLUMN_LINE, &line, -1);

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
static ScpTreeStore *command_store;
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
	width = gdk_window_get_width(gtk_widget_get_window(command_view));
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

	if (scp_tree_store_get_iter_first(command_store, &iter))
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

		scp_tree_store_get(command_store, &iter, COMMAND_TEXT, &text, COMMAND_LOCALE,
			&locale, -1);
		gtk_text_buffer_set_text(command_text, text, -1);
		gtk_toggle_button_set_active(command_locale, locale);
		gtk_widget_grab_focus(command_view);
		gtk_combo_box_set_active_iter(command_history, NULL);
	}
}

static void on_command_insert_button_clicked(G_GNUC_UNUSED GtkButton *button, gpointer gdata)
{
	const char *prefix;
	const char *id;
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

	thread_synchronize();
	utils_strchrepl(text, '\n', ' ');
	start = utils_skip_spaces(text);
	locale = gtk_toggle_button_get_active(command_locale) ?
		utils_get_locale_from_utf8(start) : g_strdup(start);
	debug_send_command(N, locale);
	g_free(locale);
	gtk_text_buffer_set_text(command_text, "", -1);
	gtk_widget_hide(command_dialog);

	if (*start)
	{
		GtkTreePath *path;
		GtkTreeIter iter;
		gchar *display = g_strdup(start);

		/* from ui_combo_box_add_to_history() */
		if (store_find(command_store, &iter, COMMAND_TEXT, start))
			scp_tree_store_remove(command_store, &iter);

		if (strlen(display) >= 273)
			strcpy(display + 270, _("\342\200\246"));  /* For translators: ellipsis */

		scp_tree_store_prepend(command_store, &iter, NULL);
		scp_tree_store_set(command_store, &iter, COMMAND_DISPLAY, display, COMMAND_TEXT,
			start, COMMAND_LOCALE, gtk_toggle_button_get_active(command_locale), -1);
		g_free(display);

		path = gtk_tree_path_new_from_indices(15, -1);
		if (scp_tree_store_get_iter(command_store, &iter, path))
			scp_tree_store_remove(command_store, &iter);
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
	GtkTextIter start, end;

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
	else
	{
		gtk_text_buffer_get_start_iter(command_text, &start);
		gtk_text_buffer_get_end_iter(command_text, &end);
		gtk_text_buffer_select_range(command_text, &start, &end);
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

static void on_geany_sidebar_switch_page(G_GNUC_UNUSED GtkNotebook *notebook,
	G_GNUC_UNUSED gpointer page, gint page_num, G_GNUC_UNUSED gpointer gdata)
{
	views_sidebar_update(page_num, debug_state());
}

static gulong switch_sidebar_page_id;

void views_init(void)
{
	if (pref_var_update_bug)
		views[VIEW_INSPECT].state = DS_DEBUG;

	command_dialog = dialog_connect("command_dialog");
	command_view = get_widget("command_view");
	command_text = gtk_text_view_get_buffer(GTK_TEXT_VIEW(command_view));
	g_signal_connect(command_text, "changed", G_CALLBACK(on_command_text_changed), NULL);
	command_history = GTK_COMBO_BOX(get_widget("command_history"));
	command_store = SCP_TREE_STORE(gtk_combo_box_get_model(command_history));
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

	geany_sidebar = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
	switch_sidebar_page_id = g_signal_connect(geany_sidebar, "switch-page",
		G_CALLBACK(on_geany_sidebar_switch_page), NULL);
	inspect_page = get_widget("inspect_page");
	gtk_notebook_append_page(geany_sidebar, inspect_page, get_widget("inspect_label"));
	register_page = get_widget("register_page");
	gtk_notebook_append_page(geany_sidebar, register_page, get_widget("register_label"));
}

void views_finalize(void)
{
	g_signal_handler_disconnect(geany_sidebar, switch_sidebar_page_id);
	gtk_widget_destroy(GTK_WIDGET(command_dialog));
	gtk_widget_destroy(inspect_page);
	gtk_widget_destroy(register_page);
}
