/*
 *  local.c
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

#include <string.h>

#include "common.h"

enum
{
	LOCAL_NAME,
	LOCAL_DISPLAY,
	LOCAL_VALUE,
	LOCAL_HB_MODE,
	LOCAL_MR_MODE,
	LOCAL_ARG1
};

static GtkListStore *store;
static GtkTreeModel *model;
static GtkTreeSelection *selection;

static void on_local_display_edited(G_GNUC_UNUSED GtkCellRendererText *renderer,
	gchar *path_str, gchar *new_text, G_GNUC_UNUSED gpointer gdata)
{
	if (validate_column(new_text, TRUE))
	{
		if (thread_state >= THREAD_STOPPED && frame_id)
		{
			GtkTreeIter iter;
			const char *arg1;
			char *format;

			gtk_tree_model_get_iter_from_string(model, &iter, path_str);
			gtk_tree_model_get(model, &iter, LOCAL_ARG1, &arg1, -1);
			format = g_strdup_printf("08%s%s", arg1 ? thread_id : "",
				"-gdb-set var %s=%s");
			view_display_edited(model, &iter, new_text, format);
			g_free(format);
		}
		else
			plugin_beep();
	}
}

static const TreeCell local_cells[] =
{
	{ "local_display", G_CALLBACK(on_local_display_edited) },
	{ NULL, NULL }
};

typedef struct _LocalData
{
	char *name;
	gboolean entry;
} LocalData;

static void local_node_variable(const ParseNode *node, const LocalData *ld)
{
	iff (node->type == PT_ARRAY, "variables: contains value")
	{
		GArray *nodes = (GArray *) node->value;
		ParseVariable var;

		if (parse_variable(nodes, &var, NULL))
		{
			GtkTreeIter iter;
			const char *arg1 = parse_find_value(nodes, "arg");

			if (!arg1 || ld->entry || !g_str_has_suffix(var.name, "@entry"))
			{
				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter, LOCAL_NAME, var.name, LOCAL_DISPLAY,
					var.display, LOCAL_VALUE, var.value, LOCAL_HB_MODE, var.hb_mode,
					LOCAL_MR_MODE, var.mr_mode, LOCAL_ARG1, arg1, -1);

				if (!g_strcmp0(var.name, ld->name))
					gtk_tree_selection_select_iter(selection, &iter);
			}
			parse_variable_free(&var);
		}
	}
}

void on_local_variables(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);
	size_t len = *token - '0' + 1;

	if (thread_id && frame_id && len == strlen(thread_id) &&
		!memcmp(++token, thread_id, len) && !strcmp(token + len, frame_id))
	{
		char *name = NULL;
		GtkTreeIter iter;
		LocalData ld = { NULL, stack_entry() };

		if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		{
			gtk_tree_model_get(model, &iter, LOCAL_NAME, &ld.name, -1);
			name = g_strdup(name);
		}

		locals_clear();
		array_foreach(parse_lead_array(nodes), (GFunc) local_node_variable, &ld);
		g_free(ld.name);
	}
}

void on_local_modified(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);

	view_dirty(VIEW_LOCALS);
	if (!g_strcmp0(token, thread_id))
		debug_send_format(T, "04%s-stack-list-arguments 1", token);
}

void locals_clear(void)
{
	gtk_list_store_clear(store);
}

static void local_send_update(char token)
{
	debug_send_format(F, "0%c%c%s%s-stack-list-variables 1", token,
		'0' + (int) strlen(thread_id) - 1, thread_id, frame_id);
}

gboolean locals_update(void)
{
	if (view_stack_update())
		return FALSE;

	if (frame_id)
		local_send_update('4');
	else
		locals_clear();

	return TRUE;
}

static GObject *local_display;

void locals_update_state(DebugState state)
{
	g_object_set(local_display, "editable", (state & DS_DEBUG) != 0, NULL);
}

static void on_local_refresh(G_GNUC_UNUSED const MenuItem *menu_item)
{
	local_send_update('2');
}

static void on_local_unsorted(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
		GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
}

static void on_local_copy(const MenuItem *menu_item)
{
	menu_copy(selection, menu_item);
}

static void on_local_modify(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const char *arg1;
	char *prefix;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, LOCAL_ARG1, &arg1, -1);
	prefix = g_strdup_printf("08%s", arg1 ? thread_id : "");
	menu_modify(model, &iter, prefix, menu_item ? MR_MODIFY : MR_MODSTR);
	g_free(prefix);
}

static void on_local_watch(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const char *name;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, LOCAL_NAME, &name, -1);
	watch_add(name);
}

static void on_local_inspect(G_GNUC_UNUSED const MenuItem *menu_item)
{
	menu_inspect(selection);
}

static void on_local_hbit_display(const MenuItem *menu_item)
{
	menu_hbit_display(selection, menu_item);
}

static void on_local_hbit_update(const MenuItem *menu_item)
{
	menu_hbit_update(selection, menu_item);
}

static void on_local_mr_mode(const MenuItem *menu_item)
{
	menu_mber_update(selection, menu_item);
}

#define DS_FRESHABLE (DS_DEBUG | DS_EXTRA_2)
#define DS_COPYABLE (DS_BASICS | DS_EXTRA_1)
#define DS_MODIFYABLE (DS_DEBUG | DS_EXTRA_2)
#define DS_WATCHABLE (DS_BASICS | DS_EXTRA_1)
#define DS_INSPECTABLE (DS_NOT_BUSY | DS_EXTRA_1)
#define DS_REPARSABLE (DS_BASICS | DS_EXTRA_1)

static MenuItem local_menu_items[] =
{
	{ "local_refresh",    on_local_refresh,  DS_FRESHABLE,   NULL, NULL },
	{ "local_unsorted",   on_local_unsorted, DS_SORTABLE,    NULL, NULL },
	{ "local_copy",       on_local_copy,     DS_COPYABLE,    NULL, NULL },
	{ "local_modify",     on_local_modify,   DS_MODIFYABLE,  NULL, NULL },
	{ "local_watch",      on_local_watch,    DS_WATCHABLE,   NULL, NULL },
	{ "local_inspect",    on_local_inspect,  DS_INSPECTABLE, NULL, NULL },
	MENU_HBIT_ITEMS(local),
	{ "local_mr_mode",    on_local_mr_mode,  DS_REPARSABLE,  NULL, NULL },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint local_menu_extra_state(void)
{
	return (gtk_tree_selection_get_selected(selection, NULL, NULL) << DS_INDEX_1) |
		((frame_id != NULL) << DS_INDEX_2);
}

static MenuInfo local_menu_info = { local_menu_items, local_menu_extra_state, 0 };

static void on_local_menu_show(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gpointer gdata)
{
	menu_mber_display(selection, menu_item_find(local_menu_items, "local_mr_mode"));
}

static void on_local_modify_button_release(GtkWidget *widget, GdkEventButton *event,
	GtkWidget *menu)
{
	menu_shift_button_release(widget, event, menu, on_local_modify);
}

static void on_local_mr_mode_button_release(GtkWidget *widget, GdkEventButton *event,
	GtkWidget *menu)
{
	menu_mber_button_release(selection, widget, event, menu);
}

void local_init(void)
{
	GtkWidget *menu;

	view_connect("local_view", &model, &selection, local_cells, "local_window",
		&local_display);
	menu = menu_select("local_menu", &local_menu_info, selection);
	store = GTK_LIST_STORE(model);

	g_signal_connect(menu, "show", G_CALLBACK(on_local_menu_show), NULL);
	g_signal_connect(get_widget("local_modify"), "button-release-event",
		G_CALLBACK(on_local_modify_button_release), menu);
	g_signal_connect(get_widget("local_mr_mode"), "button-release-event",
		G_CALLBACK(on_local_mr_mode_button_release), menu);
}
