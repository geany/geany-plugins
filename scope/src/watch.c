/*
 *  watch.c
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

#include <stdlib.h>
#include <string.h>

#include "common.h"

enum
{
	WATCH_EXPR,
	WATCH_DISPLAY,
	WATCH_VALUE,
	WATCH_HB_MODE,
	WATCH_MR_MODE,
	WATCH_SCID,
	WATCH_ENABLED
};

static GtkListStore *store;
static GtkTreeModel *model;
static GtkTreeSelection *selection;
static gint scid_gen = 0;

static void on_watch_enabled_toggled(G_GNUC_UNUSED GtkCellRendererToggle *renderer,
	gchar *path_str, G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter iter;
	gboolean enabled;

	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter, WATCH_ENABLED, &enabled, -1);
	gtk_list_store_set(store, &iter, WATCH_ENABLED, !enabled, -1);
}

static void watch_iter_update(GtkTreeIter *iter, gpointer gdata)
{
	const gchar *expr;
	gint scid;
	gboolean enabled;

	gtk_tree_model_get(model, iter, WATCH_EXPR, &expr, WATCH_SCID, &scid, WATCH_ENABLED,
		&enabled, -1);

	if (enabled || GPOINTER_TO_INT(gdata))
		g_free(debug_send_evaluate('6', scid, expr));
}

static void watch_fetch(GtkTreeIter *iter)
{
	watch_iter_update(iter, GINT_TO_POINTER(TRUE));
}

static void on_watch_expr_edited(G_GNUC_UNUSED GtkCellRendererText *renderer,
	gchar *path_str, gchar *new_text, G_GNUC_UNUSED gpointer gdata)
{
	if (validate_column(new_text, TRUE))
	{
		GtkTreeIter iter;
		const gchar *expr;
		gboolean enabled;

		gtk_tree_model_get_iter_from_string(model, &iter, path_str);
		gtk_tree_model_get(model, &iter, WATCH_EXPR, &expr, WATCH_ENABLED, &enabled, -1);

		if (strcmp(new_text, expr))
		{
			const ParseMode *pm = parse_mode_find(new_text);

			gtk_list_store_set(store, &iter, WATCH_EXPR, new_text, WATCH_DISPLAY, NULL,
				WATCH_VALUE, NULL, WATCH_HB_MODE, pm->hb_mode, WATCH_MR_MODE,
				pm->mr_mode, -1);

			if (enabled && (debug_state() & DS_DEBUG))
				watch_fetch(&iter);
		}
	}
}

static void on_watch_display_edited(G_GNUC_UNUSED GtkCellRendererText *renderer,
	gchar *path_str, gchar *new_text, G_GNUC_UNUSED gpointer gdata)
{
	if (validate_column(new_text, TRUE))
	{
		if (debug_state() & DS_SENDABLE)
		{
			GtkTreeIter iter;
			gint scid;
			char *format;

			gtk_tree_model_get_iter_from_string(model, &iter, path_str);
			gtk_tree_model_get(model, &iter, WATCH_SCID, &scid, -1);
			format = g_strdup_printf("09%d%s", scid, "-gdb-set var %s=%s");
			view_display_edited(model, &iter, new_text, format);
			g_free(format);
		}
		else
			plugin_beep();
	}
}

static const TreeCell watch_cells[] =
{
	{ "watch_display", G_CALLBACK(on_watch_display_edited)  },
	{ "watch_enabled", G_CALLBACK(on_watch_enabled_toggled) },
	{ "watch_expr",    G_CALLBACK(on_watch_expr_edited)     },
	{ NULL, NULL }
};

static GtkTreeIter wd_iter;
static gboolean wd_valid;

static void watch_set(GArray *nodes, gchar *display, const char *value)
{
	const char *token = parse_grab_token(nodes);

	if (wd_valid)
	{
		gint scid;
		gtk_tree_model_get(model, &wd_iter, WATCH_SCID, &scid, -1);
		wd_valid = atoi(token) == scid;
	}

	if (!wd_valid)
		wd_valid = model_find(model, &wd_iter, WATCH_SCID, token);

	iff (wd_valid, "%s: w_scid not found", token)
	{
		if (!display)
		{
			gint hb_mode, mr_mode;

			gtk_tree_model_get(model, &wd_iter, WATCH_HB_MODE, &hb_mode, WATCH_MR_MODE,
				&mr_mode, -1);
			display = parse_get_display_from_7bit(value, hb_mode, mr_mode);
		}

		gtk_list_store_set(store, &wd_iter, WATCH_DISPLAY, display, WATCH_VALUE, value,
			-1);
		wd_valid = gtk_tree_model_iter_next(model, &wd_iter);
	}

	g_free(display);
}

void on_watch_value(GArray *nodes)
{
	watch_set(nodes, NULL, parse_lead_value(nodes));
}

void on_watch_error(GArray *nodes)
{
	watch_set(nodes, parse_find_error(nodes), NULL);
}

void on_watch_modified(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);
	GtkTreeIter iter;

	iff (model_find(model, &iter, WATCH_SCID, token), "%s: w_scid not found", token)
		watch_fetch(&iter);
}

static void watch_iter_clear(GtkTreeIter *iter, G_GNUC_UNUSED gpointer gdata)
{
	gtk_list_store_set(store, iter, WATCH_DISPLAY, NULL, WATCH_VALUE, NULL, -1);
}

void watches_clear(void)
{
	model_foreach(model, (GFunc) watch_iter_clear, NULL);
}

gboolean watches_update(void)
{
	if (view_stack_update())
		return FALSE;

	model_foreach(model, (GFunc) watch_iter_update, GPOINTER_TO_INT(FALSE));
	return TRUE;
}

void watch_add(const gchar *text)
{
	gchar *expr = dialogs_show_input("Add Watch", GTK_WINDOW(geany->main_widgets->window),
		"Watch expression:", text);

	if (validate_column(expr, TRUE))
	{
		const ParseMode *pm = parse_mode_find(expr);
		GtkTreeIter iter;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, WATCH_EXPR, expr, WATCH_HB_MODE, pm->hb_mode,
			WATCH_MR_MODE, pm->mr_mode, WATCH_SCID, ++scid_gen, WATCH_ENABLED, TRUE, -1);
		gtk_tree_selection_select_iter(selection, &iter);

		if (debug_state() & DS_DEBUG)
			watch_fetch(&iter);
	}

	g_free(expr);
}

static GObject *watch_display;

void watches_update_state(DebugState state)
{
	g_object_set(watch_display, "editable", (state & DS_SENDABLE) != 0, NULL);
}

void watches_delete_all(void)
{
	gtk_list_store_clear(store);
	wd_valid = FALSE;
	scid_gen = 0;
}

static gboolean watch_load(GKeyFile *config, const char *section)
{
	gchar *expr = utils_key_file_get_string(config, section, "expr");
	gint hb_mode = g_key_file_get_integer(config, section, "hbit", NULL);
	gint mr_mode = g_key_file_get_integer(config, section, "member", NULL);
	gboolean enabled = g_key_file_get_boolean(config, section, "enabled", NULL);
	gboolean valid = FALSE;

	if (expr && (unsigned) hb_mode < HB_COUNT && (unsigned) mr_mode < MR_MODIFY)
	{
		GtkTreeIter iter;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, WATCH_EXPR, expr, WATCH_HB_MODE, hb_mode,
			WATCH_MR_MODE, mr_mode, WATCH_SCID, ++scid_gen, WATCH_ENABLED, enabled, -1);
		valid = TRUE;
	}

	g_free(expr);
	return valid;
}

void watches_load(GKeyFile *config)
{
	watches_delete_all();
	utils_load(config, "watch", watch_load);
}

static gboolean watch_save(GKeyFile *config, const char *section, GtkTreeIter *iter)
{
	gchar *expr;
	gint hb_mode;
	gint mr_mode;
	gboolean enabled;

	gtk_tree_model_get(model, iter, WATCH_EXPR, &expr, WATCH_HB_MODE, &hb_mode,
		WATCH_MR_MODE, &mr_mode, WATCH_ENABLED, &enabled, -1);
	g_key_file_set_string(config, section, "expr", expr);
	g_key_file_set_integer(config, section, "hbit", hb_mode);
	g_key_file_set_integer(config, section, "member", mr_mode);
	g_key_file_set_boolean(config, section, "enabled", enabled);
	return TRUE;
}

void watches_save(GKeyFile *config)
{
	model_save(model, config, "watch", watch_save);
}

static void on_watch_refresh(G_GNUC_UNUSED const MenuItem *menu_item)
{
	watches_update();
}

static void on_watch_unsorted(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), WATCH_SCID,
		GTK_SORT_ASCENDING);
}

static void on_watch_add(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const gchar *expr = NULL;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		gtk_tree_model_get(model, &iter, WATCH_EXPR, &expr, -1);

	watch_add(expr);
}

static void on_watch_copy(const MenuItem *menu_item)
{
	menu_copy(selection, menu_item);
}

static void on_watch_modify(const MenuItem *menu_item)
{
	GtkTreeIter iter;
	gint scid;
	char *prefix;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, WATCH_SCID, &scid, -1);
	prefix = g_strdup_printf("09%d", scid);
	menu_modify(model, &iter, prefix, menu_item ? MR_MODIFY : MR_MODSTR);
	g_free(prefix);
}

static void on_watch_inspect(G_GNUC_UNUSED const MenuItem *menu_item)
{
	menu_inspect(selection);
}

static void on_watch_hbit_display(const MenuItem *menu_item)
{
	menu_hbit_display(selection, menu_item);
}

static void on_watch_hbit_update(const MenuItem *menu_item)
{
	menu_hbit_update(selection, menu_item);
}

static void on_watch_mr_mode(const MenuItem *menu_item)
{
	menu_mber_update(selection, menu_item);
}

static void on_watch_delete(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		gtk_list_store_remove(store, &iter);
		wd_valid = FALSE;
	}
}

#define DS_FRESHABLE DS_SENDABLE
#define DS_WATCHABLE 0
#define DS_COPYABLE (DS_BASICS | DS_EXTRA_1)
#define DS_MODIFYABLE (DS_SENDABLE | DS_EXTRA_1)
#define DS_INSPECTABLE (DS_NOT_BUSY | DS_EXTRA_1)
#define DS_REPARSABLE (DS_BASICS | DS_EXTRA_1)
#define DS_DELETABLE (DS_BASICS | DS_EXTRA_1)

static MenuItem watch_menu_items[] =
{
	{ "watch_refresh",    on_watch_refresh,  DS_FRESHABLE,   NULL, NULL },
	{ "watch_unsorted",   on_watch_unsorted, DS_SORTABLE,    NULL, NULL },
	{ "watch_add",        on_watch_add,      DS_WATCHABLE,   NULL, NULL },
	{ "watch_copy",       on_watch_copy,     DS_COPYABLE,    NULL, NULL },
	{ "watch_modify",     on_watch_modify,   DS_MODIFYABLE,  NULL, NULL },
	{ "watch_inspect",    on_watch_inspect,  DS_INSPECTABLE, NULL, NULL },
	MENU_HBIT_ITEMS(watch),
	{ "watch_mr_mode",    on_watch_mr_mode,  DS_REPARSABLE,  NULL, NULL },
	{ "watch_delete",     on_watch_delete,   DS_DELETABLE,   NULL, NULL },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint watch_menu_extra_state(void)
{
	return gtk_tree_selection_get_selected(selection, NULL, NULL) << DS_INDEX_1;
}

static MenuInfo watch_menu_info = { watch_menu_items, watch_menu_extra_state, 0 };

static gboolean on_watch_key_press(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event,
	G_GNUC_UNUSED gpointer gdata)
{
	return menu_insert_delete(event, &watch_menu_info, "watch_add", "watch_delete");
}

static void on_watch_menu_show(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gpointer gdata)
{
	menu_mber_display(selection, menu_item_find(watch_menu_items, "watch_mr_mode"));
}

static void on_watch_modify_button_release(GtkWidget *widget, GdkEventButton *event,
	GtkWidget *menu)
{
	menu_shift_button_release(widget, event, menu, on_watch_modify);
}

static void on_watch_mr_mode_button_release(GtkWidget *widget, GdkEventButton *event,
	GtkWidget *menu)
{
	menu_mber_button_release(selection, widget, event, menu);
}

void watch_init(void)
{
	GtkTreeView *tree = view_connect("watch_view", &model, &selection, watch_cells,
		"watch_window", &watch_display);
	GtkWidget *menu = menu_select("watch_menu", &watch_menu_info, selection);

	store = GTK_LIST_STORE(model);
	g_signal_connect(tree, "key-press-event", G_CALLBACK(on_watch_key_press), NULL);
	wd_valid = FALSE;

	g_signal_connect(menu, "show", G_CALLBACK(on_watch_menu_show), NULL);
	g_signal_connect(get_widget("watch_modify"), "button-release-event",
		G_CALLBACK(on_watch_modify_button_release), menu);
	g_signal_connect(get_widget("watch_mr_mode"), "button-release-event",
		G_CALLBACK(on_watch_mr_mode_button_release), menu);
}
