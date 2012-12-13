/*
 *  stack.c
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
	STACK_ID,
	STACK_FILE,
	STACK_LINE,
	STACK_BASE_NAME,
	STACK_FUNC,
	STACK_ARGS,
	STACK_ADDR,
	STACK_ENTRY
};

static GtkListStore *store;
static GtkTreeModel *model;
static GtkTreeSortable *sortable;
static GtkTreeSelection *selection;

static void stack_node_location(const ParseNode *node, const char *fid)
{
	iff (node->type == PT_ARRAY, "stack: contains value")
	{
		GArray *nodes = (GArray *) node->value;
		const char *id = parse_find_value(nodes, "level");

		iff (id, "no level")
		{
			ParseLocation loc;
			GtkTreeIter iter;

			parse_location(nodes, &loc);
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, STACK_ID, id, STACK_FILE, loc.file,
				STACK_LINE, loc.line, STACK_BASE_NAME, loc.base_name, STACK_FUNC,
				loc.func, STACK_ARGS, NULL, STACK_ADDR, loc.addr, STACK_ENTRY,
				loc.func ? parse_mode_find(loc.func)->entry : TRUE, -1);
			parse_location_free(&loc);

			if (!g_strcmp0(id, fid))
				gtk_tree_selection_select_iter(selection, &iter);
		}
	}
}

const char *frame_id = NULL;

void on_stack_frames(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);

	if (!g_strcmp0(token, thread_id))
	{
		char *fid = g_strdup(frame_id);

		stack_clear();
		array_foreach(parse_lead_array(nodes), (GFunc) stack_node_location, fid);
		g_free(fid);

		if (!frame_id)
		{
			GtkTreeIter iter;

			if (gtk_tree_model_get_iter_first(model, &iter))
				gtk_tree_selection_select_iter(selection, &iter);
		}
	}
}

typedef struct _StackData
{
	GString *string;
	gboolean entry;
} StackData;

static void append_argument_variable(const ParseNode *node, const StackData *sd)
{
	iff (node->type == PT_ARRAY, "args: contains value")
	{
		ParseVariable var;

		if (parse_variable((GArray *) node->value, &var, NULL) &&
			(sd->entry || !g_str_has_suffix(var.name, "@entry")))
		{
			if (sd->string->len)
				g_string_append(sd->string, ", ");

			if (option_argument_names)
			{
				g_string_append_printf(sd->string,
					option_long_mr_format ? "%s = " : "%s=", var.name);
			}
			g_string_append(sd->string, var.display);
			parse_variable_free(&var);
		}
	}
}

typedef struct _ArgsData
{
	gboolean sorted;
	GtkTreeIter iter;
	gboolean valid;
} ArgsData;

static void stack_node_arguments(const ParseNode *node, ArgsData *ad)
{
	iff (node->type == PT_ARRAY, "stack-args: contains value")
	{
		GArray *nodes = (GArray *) node->value;
		const char *id = parse_find_value(nodes, "level");

		nodes = parse_find_array(nodes, "args");

		iff (id && nodes, "no level or args")
		{
			if (ad->valid)
			{
				const char *fid;

				gtk_tree_model_get(model, &ad->iter, STACK_ID, &fid, -1);
				ad->valid = !strcmp(id, fid);
			}

			if (!ad->valid)
				ad->valid = model_find(model, &ad->iter, STACK_ID, id);

			iff (ad->valid, "%s: level not found", id)
			{
				StackData sd;

				sd.string = g_string_sized_new(0xFF);
				gtk_tree_model_get(model, &ad->iter, STACK_ENTRY, &sd.entry, -1);
				array_foreach(nodes, (GFunc) append_argument_variable, &sd);
				gtk_list_store_set(store, &ad->iter, STACK_ARGS, sd.string->str, -1);
				g_string_free(sd.string, TRUE);
				ad->valid = ad->sorted && gtk_tree_model_iter_next(model, &ad->iter);
			}
		}
	}
}

void on_stack_arguments(GArray *nodes)
{
	gint column_id;
	GtkSortType order;
	ArgsData ad;

	gtk_tree_sortable_get_sort_column_id(sortable, &column_id, &order);
	ad.sorted = column_id == GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID ||
		(column_id == STACK_ID && order == GTK_SORT_ASCENDING);
	ad.valid = ad.sorted && gtk_tree_model_get_iter_first(model, &ad.iter);

	if (!g_strcmp0(parse_grab_token(nodes), thread_id))
		array_foreach(parse_lead_array(nodes), (GFunc) stack_node_arguments, &ad);
}

gboolean stack_entry(void)
{
	GtkTreeIter iter;
	gboolean entry;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, STACK_ENTRY, &entry, -1);
	return entry;
}

void stack_clear(void)
{
	gtk_list_store_clear(store);
}

static void stack_send_update(char token)
{
	debug_send_format(T, "0%c%s-stack-list-frames", token, thread_id);
	debug_send_format(T, "0%c%s-stack-list-arguments 1", token, thread_id);
}

gboolean stack_update(void)
{
	stack_send_update('4');
	return TRUE;
}

static void on_stack_selection_changed(GtkTreeSelection *selection,
	G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		gtk_tree_model_get(model, &iter, STACK_ID, &frame_id, -1);
	else
		frame_id = NULL;

	view_dirty(VIEW_LOCALS);
	view_dirty(VIEW_WATCHES);
}

static void stack_seek_selected(gboolean focus)
{
	view_seek_selected(selection, focus, SK_DEFAULT);
}

static void on_stack_refresh(G_GNUC_UNUSED const MenuItem *menu_item)
{
	stack_send_update('2');
}

static void on_stack_unsorted(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gtk_tree_sortable_set_sort_column_id(sortable, GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
		GTK_SORT_ASCENDING);
}

static void on_stack_view_source(G_GNUC_UNUSED const MenuItem *menu_item)
{
	stack_seek_selected(FALSE);
}

gboolean stack_show_address;

static void on_stack_show_address(const MenuItem *menu_item)
{
	on_menu_update_boolean(menu_item);
	view_column_set_visible("stack_addr_column", stack_show_address);
}

static void on_stack_show_entry(const MenuItem *menu_item)
{
	gboolean entry = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_item->widget));
	GtkTreeIter iter;
	const char *func;
	guint count = 0;

	view_dirty(VIEW_LOCALS);
	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, STACK_FUNC, &func, -1);
	parse_mode_update(func, MODE_ENTRY, entry);
	gtk_tree_model_get_iter_first(model, &iter);
	do
	{
		const char *func1;
		
		gtk_tree_model_get(model, &iter, STACK_FUNC, &func1, -1);
		if (!strcmp(func, func1))
		{
			gtk_list_store_set(store, &iter, STACK_ENTRY, entry, -1);
			count++;
		}
	} while (gtk_tree_model_iter_next(model, &iter));

	if (count == 1)
	{
		debug_send_format(T, "04%s-stack-list-arguments 1 %s %s", thread_id, frame_id,
			frame_id);
	}
	else
		debug_send_format(T, "04%s-stack-list-arguments 1", thread_id);
}

#define DS_FRESHABLE (DS_DEBUG | DS_EXTRA_1)
#define DS_ENTRABLE (DS_ACTIVE | DS_EXTRA_2)
#define DS_VIEWABLE (DS_ACTIVE | DS_EXTRA_3)

static MenuItem stack_menu_items[] =
{
	{ "stack_refresh",      on_stack_refresh,      DS_FRESHABLE, NULL, NULL },
	{ "stack_unsorted",     on_stack_unsorted,     DS_SORTABLE,  NULL, NULL },
	{ "stack_view_source",  on_stack_view_source,  DS_VIEWABLE,  NULL, NULL },
	{ "stack_show_entry",   on_stack_show_entry,   DS_ENTRABLE,  NULL, NULL },
	{ "stack_show_address", on_stack_show_address, 0,            NULL, &stack_show_address },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint stack_menu_extra_state(void)
{
	GtkTreeIter iter;
	const char *func = NULL;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		gtk_tree_model_get(model, &iter, STACK_FUNC, &func, -1);

	return ((thread_id != NULL) << DS_INDEX_1) | ((func != NULL) << DS_INDEX_2) |
		((frame_id != NULL) << DS_INDEX_3);
}

static MenuInfo stack_menu_info = { stack_menu_items, stack_menu_extra_state, 0 };

static void on_stack_menu_show(G_GNUC_UNUSED GtkWidget *widget, const MenuItem *menu_item)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		gboolean entry;
		gtk_tree_model_get(model, &iter, STACK_ENTRY, &entry, -1);
		menu_item_set_active(menu_item, entry);
	}
	menu_item_set_active(menu_item + 1, stack_show_address);
}

void stack_init(void)
{
	GtkTreeView *tree = view_create("stack_view", &model, &selection);
	GtkWidget *menu = menu_select("stack_menu", &stack_menu_info, selection);

	store = GTK_LIST_STORE(model);
	sortable = GTK_TREE_SORTABLE(store);
	view_set_sort_func(sortable, STACK_ID, model_gint_compare);
	view_set_sort_func(sortable, STACK_FILE, model_seek_compare);
	view_set_line_data_func("stack_line_column", "stack_line", STACK_LINE);
	gtk_widget_set_has_tooltip(GTK_WIDGET(tree), TRUE);
	g_signal_connect(tree, "query-tooltip", G_CALLBACK(on_view_query_tooltip),
		GTK_TREE_VIEW_COLUMN(get_object("stack_base_name_column")));

	g_signal_connect(tree, "key-press-event", G_CALLBACK(on_view_key_press),
		stack_seek_selected);
	g_signal_connect(tree, "button-press-event", G_CALLBACK(on_view_button_1_press),
		stack_seek_selected);
	g_signal_connect(selection, "changed", G_CALLBACK(on_stack_selection_changed), NULL);
	g_signal_connect(menu, "show", G_CALLBACK(on_stack_menu_show),
		(gpointer) menu_item_find(stack_menu_items, "stack_show_entry"));
}
