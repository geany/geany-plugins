/*
 * Copyright 2023 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* This file contains mostly stolen code from the Colomban Wendling's Commander
 * plugin. Thanks! */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "prjorg-goto-panel.h"

#include <gtk/gtk.h>
#include <geanyplugin.h>


enum {
	COL_ICON,
	COL_LABEL,
	COL_PATH,
	COL_LINENO,
	COL_COUNT
};


//TODO: free on plugin unload
struct {
	GtkWidget *panel;
	GtkWidget *entry;
	GtkWidget *tree_view;
	GtkListStore *store;
} panel_data = {
	NULL, NULL, NULL, NULL
};


static PrjorgGotoPanelLookupFunction lookup_function;

extern GeanyData *geany_data;


void prjorg_goto_symbol_free(PrjorgGotoSymbol *symbol)
{
	g_free(symbol->name);
	g_free(symbol->file_name);
	g_free(symbol->scope);
	g_free(symbol->tooltip);
	g_free(symbol);
}


static void tree_view_set_cursor_from_iter(GtkTreeView *view, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path(gtk_tree_view_get_model(view), iter);
	gtk_tree_view_set_cursor(view, path, NULL, FALSE);
	gtk_tree_path_free(path);
}


static void tree_view_move_focus(GtkTreeView *view, GtkMovementStep step, gint amount)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	gboolean valid = FALSE;

	gtk_tree_view_get_cursor(view, &path, NULL);
	if (!path)
		valid = gtk_tree_model_get_iter_first(model, &iter);
	else
	{
		switch (step) {
			case GTK_MOVEMENT_BUFFER_ENDS:
				valid = gtk_tree_model_get_iter_first(model, &iter);
				if (valid && amount > 0)
				{
					GtkTreeIter prev;

					do {
						prev = iter;
					} while (gtk_tree_model_iter_next(model, &iter));
					iter = prev;
				}
				break;

			case GTK_MOVEMENT_PAGES:
				/* FIXME: move by page */
			case GTK_MOVEMENT_DISPLAY_LINES:
				gtk_tree_model_get_iter(model, &iter, path);
				if (amount > 0)
				{
					while ((valid = gtk_tree_model_iter_next(model, &iter)) && --amount > 0)
						;
				}
				else if (amount < 0)
				{
					while ((valid = gtk_tree_path_prev(path)) && --amount > 0)
						;

					if (valid)
						gtk_tree_model_get_iter(model, &iter, path);
				}
				break;

			default:
				g_assert_not_reached();
		}
		gtk_tree_path_free(path);
	}

	if (valid)
		tree_view_set_cursor_from_iter(view, &iter);
	else
		gtk_widget_error_bell(GTK_WIDGET(view));
}


static void tree_view_activate_focused_row(GtkTreeView *view)
{
	GtkTreePath *path;
	GtkTreeViewColumn *column;

	gtk_tree_view_get_cursor(view, &path, &column);
	if (path)
	{
		gtk_tree_view_row_activated(view, path, column);
		gtk_tree_path_free(path);
	}
}


void prjorg_goto_panel_fill(GPtrArray *symbols)
{
	GtkTreeView *view = GTK_TREE_VIEW(panel_data.tree_view);
	GtkTreeIter iter;
	PrjorgGotoSymbol *sym;
	guint i;

	gtk_list_store_clear(panel_data.store);

	foreach_ptr_array(sym, i, symbols)
	{
		gchar *label;

		if (!sym->file_name)
			continue;

		if (sym->file_name && sym->line > 0)
			label = g_markup_printf_escaped("%s\n<small><i>%s:%d</i></small>",
				sym->name, sym->file_name, sym->line);
		else if (sym->file_name)
			label = g_markup_printf_escaped("%s\n<small><i>%s</i></small>",
				sym->name, sym->file_name);
		else
			label = g_markup_printf_escaped("%s", sym->name);

		gtk_list_store_insert_with_values(panel_data.store, NULL, -1,
			COL_ICON, symbols_get_icon_pixbuf(sym->icon),
			COL_LABEL, label,
			COL_PATH, sym->file_name,
			COL_LINENO, sym->line,
			-1);

		g_free(label);
	}

	if (gtk_tree_model_get_iter_first(gtk_tree_view_get_model(view), &iter))
		tree_view_set_cursor_from_iter(GTK_TREE_VIEW(panel_data.tree_view), &iter);
}


static gboolean on_panel_key_press_event(GtkWidget *widget, GdkEventKey *event,
	gpointer dummy)
{
	switch (event->keyval) {
		case GDK_KEY_Escape:
			gtk_widget_hide(widget);
			return TRUE;

		case GDK_KEY_Tab:
			/* avoid leaving the entry */
			return TRUE;

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
			tree_view_activate_focused_row(GTK_TREE_VIEW(panel_data.tree_view));
			return TRUE;

		case GDK_KEY_Page_Up:
		case GDK_KEY_Page_Down:
		case GDK_KEY_KP_Page_Up:
		case GDK_KEY_KP_Page_Down:
		{
			gboolean up = event->keyval == GDK_KEY_Page_Up || event->keyval == GDK_KEY_KP_Page_Up;
			tree_view_move_focus(GTK_TREE_VIEW(panel_data.tree_view),
				GTK_MOVEMENT_PAGES, up ? -1 : 1);
			return TRUE;
		}

		case GDK_KEY_Up:
		case GDK_KEY_Down:
		case GDK_KEY_KP_Up:
		case GDK_KEY_KP_Down:
		{
			gboolean up = event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up;
			tree_view_move_focus(GTK_TREE_VIEW(panel_data.tree_view),
				GTK_MOVEMENT_DISPLAY_LINES, up ? -1 : 1);
			return TRUE;
		}
	}

	return FALSE;
}


static void on_entry_text_notify(GObject *object, GParamSpec *pspec, gpointer dummy)
{
	GtkTreeIter iter;
	GtkTreeView *view  = GTK_TREE_VIEW(panel_data.tree_view);
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(panel_data.entry));

	lookup_function(text);

	if (gtk_tree_model_get_iter_first(model, &iter))
		tree_view_set_cursor_from_iter(view, &iter);
}


static void on_entry_activate(GtkEntry *entry, gpointer dummy)
{
	tree_view_activate_focused_row(GTK_TREE_VIEW(panel_data.tree_view));
}


static void on_panel_hide(GtkWidget *widget, gpointer dummy)
{
	gtk_list_store_clear(panel_data.store);
}


static void on_panel_show(GtkWidget *widget, gpointer dummy)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(panel_data.entry));
	gboolean select_first = TRUE;

	if (text && (text[0] == ':' || text[0] == '#' || text[0] == '@'))
		select_first = FALSE;

	gtk_widget_grab_focus(panel_data.entry);
	gtk_editable_select_region(GTK_EDITABLE(panel_data.entry), select_first ? 0 : 1, -1);
}


static void on_view_row_activated(GtkTreeView *view, GtkTreePath *path,
	GtkTreeViewColumn *column, gpointer dummy)
{
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		GeanyDocument *doc;
		gchar *file_path;
		gint line;

		gtk_tree_model_get(model, &iter,
			COL_PATH, &file_path,
			COL_LINENO, &line,
			-1);

		SETPTR(file_path, utils_get_locale_from_utf8(file_path));
		doc = document_open_file(file_path, FALSE, NULL, NULL);

		if (doc && line > 0)
			navqueue_goto_line(document_get_current(), doc, line);

		g_free(file_path);
	}

	gtk_widget_hide(panel_data.panel);
}


static void create_panel(void)
{
	GtkWidget *frame, *box, *scroll;
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;

	panel_data.panel = g_object_new(GTK_TYPE_WINDOW,
		"decorated", FALSE,
		"default-width", 500,
		"default-height", 350,
		"transient-for", geany_data->main_widgets->window,
		"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
		"type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
		"skip-taskbar-hint", TRUE,
		"skip-pager-hint", TRUE,
		NULL);
	g_signal_connect(panel_data.panel, "focus-out-event",
		G_CALLBACK(gtk_widget_hide), NULL);
	g_signal_connect(panel_data.panel, "show",
		G_CALLBACK(on_panel_show), NULL);
	g_signal_connect(panel_data.panel, "hide",
		G_CALLBACK(on_panel_hide), NULL);
	g_signal_connect(panel_data.panel, "key-press-event",
		G_CALLBACK(on_panel_key_press_event), NULL);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(panel_data.panel), frame);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(frame), box);

	panel_data.entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), panel_data.entry, FALSE, TRUE, 0);

	scroll = g_object_new(GTK_TYPE_SCROLLED_WINDOW,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

	panel_data.tree_view = gtk_tree_view_new();
	gtk_widget_set_can_focus(panel_data.tree_view, FALSE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(panel_data.tree_view), FALSE);

	panel_data.store = gtk_list_store_new(COL_COUNT,
		GDK_TYPE_PIXBUF,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(panel_data.tree_view), GTK_TREE_MODEL(panel_data.store));
	g_object_unref(panel_data.store);

	renderer = gtk_cell_renderer_pixbuf_new();
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(col, renderer, FALSE);
	gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", COL_ICON, NULL);
	g_object_set(renderer, "xalign", 0.0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(panel_data.tree_view), col);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, renderer,
		"markup", COL_LABEL,
		NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(panel_data.tree_view), col);

	g_signal_connect(panel_data.tree_view, "row-activated",
		G_CALLBACK(on_view_row_activated), NULL);
	gtk_container_add(GTK_CONTAINER(scroll), panel_data.tree_view);

	/* connect entry signals after the view is created as they use it */
	g_signal_connect(panel_data.entry, "notify::text",
		G_CALLBACK(on_entry_text_notify), NULL);
	g_signal_connect(panel_data.entry, "activate",
		G_CALLBACK(on_entry_activate), NULL);

	gtk_widget_show_all(frame);
}


void prjorg_goto_panel_show(const gchar *query, PrjorgGotoPanelLookupFunction func)
{
	if (!panel_data.panel)
		create_panel();

	lookup_function = func;

	gtk_entry_set_text(GTK_ENTRY(panel_data.entry), query);
	gtk_list_store_clear(panel_data.store);
	gtk_widget_show(panel_data.panel);

	lookup_function(query);
}


GPtrArray *prjorg_goto_panel_filter(GPtrArray *symbols, const gchar *filter)
{
	GPtrArray *ret = g_ptr_array_new();
	gchar *case_normalized_filter;
	gchar **tf_strv;
	guint i;
	guint j = 0;

	if (!symbols)
		return ret;

	case_normalized_filter = g_utf8_normalize(filter, -1, G_NORMALIZE_ALL);
	SETPTR(case_normalized_filter, g_utf8_casefold(case_normalized_filter, -1));

	tf_strv = g_strsplit_set(case_normalized_filter, " ", -1);

	for (i = 0; i < symbols->len && j < 20; i++)
	{
		PrjorgGotoSymbol *symbol = symbols->pdata[i];
		gboolean filtered = FALSE;
		gchar *case_normalized_name;
		gchar **val;

		case_normalized_name = g_utf8_normalize(symbol->name, -1, G_NORMALIZE_ALL);
		SETPTR(case_normalized_name, g_utf8_casefold(case_normalized_name, -1));

		foreach_strv(val, tf_strv)
		{
			if (case_normalized_name != NULL && *val != NULL)
				filtered = strstr(case_normalized_name, *val) == NULL;

			if (filtered)
				break;
		}
		if (!filtered)
		{
			g_ptr_array_add(ret, symbol);
			j++;
		}

		g_free(case_normalized_name);
	}

	g_strfreev(tf_strv);
	g_free(case_normalized_filter);

	return ret;
}
