/*
 *  register.c
 *
 *  Copyright 2013 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
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
#include <stdlib.h>
#include <string.h>

#include "common.h"

enum
{
	REGISTER_PATH,
	REGISTER_DISPLAY,
	REGISTER_VALUE,
	REGISTER_HB_MODE,  /* natural register as char(s)?.. */
	REGISTER_NAME,
	REGISTER_ID,
	REGISTER_FORMAT
};

static ScpTreeStore *store;
static GtkTreeSelection *selection;

enum
{
	FORMAT_NATURAL,
	FORMAT_HEX,
	FORMAT_DECIMAL,
	FORMAT_OCTAL,
	FORMAT_BINARY,
	FORMAT_RAW,
	FORMAT_COUNT
};

static const char register_formats[FORMAT_COUNT] = { 'N', 'x', 'd', 'o', 't', 'r' };

static void register_iter_update(GtkTreeIter *iter, GString *commands[])
{
	gint id, format;

	scp_tree_store_get(store, iter, REGISTER_ID, &id, REGISTER_FORMAT, &format, -1);
	g_string_append_printf(commands[format], " %d", id);
}

static void register_node_update(const ParseNode *node, GString *commands[])
{
	iff (node->type == PT_VALUE, "changed-registers: contains array")
	{
		const char *id = (const char *) node->value;
		GtkTreeIter iter;

		iff (store_find(store, &iter, REGISTER_ID, id), "%s: rid not found", id)
			register_iter_update(&iter, commands);
	}
}

static gboolean query_all_registers = TRUE;

static void registers_send_update(GArray *nodes, char token)
{
	GString *commands[FORMAT_COUNT];
	guint format;
	gsize empty;

	for (format = 0; format < FORMAT_COUNT; format++)
	{
		commands[format] = g_string_sized_new(0x7F);
		g_string_append_printf(commands[format], "0%c9%c%s%s-data-list-register-values %c",
			token, FRAME_ARGS, register_formats[format]);
	}

	empty = commands[FORMAT_NATURAL]->len;

	if (nodes)
		parse_foreach(nodes, (GFunc) register_node_update, commands);
	else
	{
		store_foreach(store, (GFunc) register_iter_update, commands);
		query_all_registers = FALSE;
	}

	for (format = 0; format < FORMAT_COUNT; format++)
	{
		if (commands[format]->len > empty)
			debug_send_command(F, commands[format]->str);

		g_string_free(commands[format], TRUE);
	}
}

typedef struct _IndexData
{
	guint index;
	guint count;
} IndexData;

static void register_node_name(const ParseNode *node, IndexData *id)
{
	iff (node->type == PT_VALUE, "register-names: contains array")
	{
		const char *name = (const char *) node->value;

		if (*name)
		{
			GtkTreeIter iter, iter1;

			if (store_find(store, &iter1, REGISTER_NAME, name))
			{
				scp_tree_store_iter_nth_child(store, &iter, NULL, id->count);
				scp_tree_store_swap(store, &iter, &iter1);
			}
			else
			{
				scp_tree_store_insert_with_values(store, &iter, NULL, id->count,
					REGISTER_PATH, name, REGISTER_NAME, name, REGISTER_HB_MODE,
					HB_DEFAULT, REGISTER_FORMAT, FORMAT_NATURAL, -1);
			}

			scp_tree_store_set(store, &iter, REGISTER_DISPLAY, NULL, REGISTER_VALUE,
				NULL, REGISTER_ID, id->index, -1);
			id->count++;
		}

		id->index++;
	}
}

void on_register_names(GArray *nodes)
{
	IndexData id = { 0, 0 };
	gboolean valid;
	GtkTreeIter iter;
	const char *token = parse_grab_token(nodes);

	parse_foreach(parse_lead_array(nodes), (GFunc) register_node_name, &id);
	valid = scp_tree_store_iter_nth_child(store, &iter, NULL, id.count);
	while (valid)
		valid = scp_tree_store_remove(store, &iter);

	if (token)
		registers_send_update(NULL, '2');
}

void on_register_changes(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);
	GArray *changes = parse_lead_array(nodes);

	if (token)
	{
		if (utils_matches_frame(token))
			registers_send_update(changes, '4');
	}
	else if (changes->len)
		query_all_registers = TRUE;  /* external changes */
}

static void register_set_value(GtkTreeIter *iter, char *value)
{
	if (*value == '{')
	{
		const char *parent;
		GtkTreeIter child;
		gboolean valid = scp_tree_store_iter_children(store, &child, iter);
		gboolean next;

		scp_tree_store_set(store, iter, REGISTER_DISPLAY, NULL, REGISTER_VALUE, NULL, -1);
		scp_tree_store_get(store, iter, REGISTER_NAME, &parent, -1);

		do
		{
			const char *name = ++value;
			char *path, *end;

			if ((value = strchr(name, '=')) == NULL)
			{
				dc_error("= expected");
				break;
			}

			value[-(isspace(value[-1]) != 0)] = '\0';

			if (!*name)
			{
				dc_error("name expected");
				break;
			}

			if (isspace(*++value))
				value++;

			if (*value == '{')
			{
				if ((end = strchr(value, '}')) != NULL)
					end++;
			}
			else if ((end = strchr(value, ',')) == NULL)
				end = strchr(value, '}');

			if (!end)
			{
				dc_error(", or } expected");
				break;
			}

			next = *end == ',';
			*end = '\0';
			path = g_strdup_printf("%s.%s", parent, name);

			if (!valid)
				scp_tree_store_append(store, &child, iter);
			scp_tree_store_set(store, &child, REGISTER_PATH, path, REGISTER_NAME, name,
				REGISTER_DISPLAY, value, REGISTER_VALUE, value, -1);
			valid &= scp_tree_store_iter_next(store, &child);

			g_free(path);
			value = end;
			if (isspace(value[1]))
				value++;

		} while (next);

		while (valid)
			valid = scp_tree_store_remove(store, &child);
	}
	else
	{
		scp_tree_store_clear_children(store, iter, FALSE);
		scp_tree_store_set(store, iter, REGISTER_DISPLAY, value, REGISTER_VALUE, value, -1);
	}
}

typedef struct _ValueAction
{
	gint format;
	gboolean assign;
} ValueAction;

static void register_node_value(const ParseNode *node, const ValueAction *va)
{
	iff (node->type == PT_ARRAY, "register-values: contains value")
	{
		GArray *nodes = (GArray *) node->value;
		const char *number = parse_find_value(nodes, "number");
		char *value = parse_find_value(nodes, "value");

		iff (number && value, "no number or value")
		{
			GtkTreeIter iter;

			if (store_find(store, &iter, REGISTER_ID, number), "%s: rid not found")
			{
				if (va->format < FORMAT_COUNT)
					scp_tree_store_set(store, &iter, REGISTER_FORMAT, va->format, -1);

				if (va->assign)
					register_set_value(&iter, value);
			}
		}
	}
}

void on_register_values(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);
	ValueAction va = { *token - '0', utils_matches_frame(token + 1) };

	if (va.format < FORMAT_COUNT || va.assign)
		parse_foreach(parse_lead_array(nodes), (GFunc) register_node_value, &va);
}

static void on_register_display_edited(G_GNUC_UNUSED GtkCellRendererText *renderer,
	gchar *path_str, gchar *new_text, G_GNUC_UNUSED gpointer gdata)
{
	view_display_edited(store, debug_state() & DS_DEBUG, path_str, "07-gdb-set var $%s=%s",
		new_text);
}

static const TreeCell register_cells[] =
{
	{ "register_display", G_CALLBACK(on_register_display_edited) },
	{ NULL, NULL }
};

static void register_iter_clear(GtkTreeIter *iter, G_GNUC_UNUSED gpointer gdata)
{
	if (scp_tree_store_iter_has_child(store, iter))
		scp_tree_store_clear_children(store, iter, FALSE);
	else
		scp_tree_store_set(store, iter, REGISTER_DISPLAY, NULL, REGISTER_VALUE, NULL, -1);
}

void registers_clear(void)
{
	store_foreach(store, (GFunc) register_iter_clear, NULL);
	query_all_registers = TRUE;
}

gboolean registers_update(void)
{
	if (view_frame_update())
		return FALSE;

	if (frame_id)
	{
		if (query_all_registers)
			registers_send_update(NULL, '4');
		else
			debug_send_format(F, "04%c%s%s-data-list-changed-registers", FRAME_ARGS);
	}
	else
		registers_clear();

	return TRUE;
}

static char *last_gdb_executable = NULL;

void registers_query_names(void)
{
	if (g_strcmp0(pref_gdb_executable, last_gdb_executable))
	{
		g_free(last_gdb_executable);
		last_gdb_executable = g_strdup(pref_gdb_executable);
		debug_send_command(N, "-data-list-register-names");
	}
}

static GtkWidget *tree;

void registers_show(gboolean show)
{
	gtk_widget_set_visible(tree, show);
}

static GObject *register_display;

void registers_update_state(DebugState state)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		GtkTreeIter parent;

		g_object_set(register_display, "editable", (state & DS_DEBUG) &&
			(scp_tree_store_iter_parent(store, &parent, &iter) ||
			!scp_tree_store_iter_has_child(store, &iter)), NULL);
	}
}

void registers_delete_all(void)
{
	store_clear(store);
}

static gboolean register_load(GKeyFile *config, const char *section)
{
	char *name = utils_key_file_get_string(config, section, "name");
	gint format = utils_get_setting_integer(config, section, "format", FORMAT_NATURAL);
	gboolean valid = FALSE;

	if (name && (unsigned) format < FORMAT_COUNT)
	{
		scp_tree_store_append_with_values(store, NULL, NULL, REGISTER_PATH, name,
			REGISTER_NAME, name, REGISTER_HB_MODE, HB_DEFAULT, REGISTER_FORMAT,
			format, -1);
		valid = TRUE;
	}

	g_free(name);
	return valid;
}

void registers_load(GKeyFile *config)
{
	registers_delete_all();
	g_free(last_gdb_executable);
	last_gdb_executable = NULL;
	utils_load(config, "register", register_load);
}

static gboolean register_save(GKeyFile *config, const char *section, GtkTreeIter *iter)
{
	const char *name;
	gint format;

	scp_tree_store_get(store, iter, REGISTER_NAME, &name, REGISTER_FORMAT, &format, -1);

	if (format == FORMAT_NATURAL)
		return FALSE;

	g_key_file_set_string(config, section, "name", name);
	g_key_file_set_integer(config, section, "format", format);
	return TRUE;
}

void registers_save(GKeyFile *config)
{
	store_save(store, config, "register", register_save);
}

static gboolean on_register_query_tooltip(G_GNUC_UNUSED GtkWidget *widget, gint x, gint y,
	gboolean keyboard_tip, GtkTooltip *tooltip, GtkTreeViewColumn *register_name_column)
{
	GtkTreeView *tree = GTK_TREE_VIEW(widget);
	GtkTreeIter iter;

	if (gtk_tree_view_get_tooltip_context(tree, &x, &y, keyboard_tip, NULL, NULL, &iter))
	{
		gint id;
		char *text;

		gtk_tree_view_set_tooltip_cell(tree, tooltip, NULL, register_name_column, NULL);
		scp_tree_store_get(store, &iter, REGISTER_ID, &id, -1);
		text = g_strdup_printf("register %d", id);
		gtk_tooltip_set_text(tooltip, text);
		g_free(text);
		return TRUE;
	}

	return FALSE;
}

static void on_register_refresh(G_GNUC_UNUSED const MenuItem *menu_item)
{
	registers_send_update(NULL, '2');
}

static void on_register_copy(const MenuItem *menu_item)
{
	menu_copy(selection, menu_item);
}

static void on_register_format_display(const MenuItem *menu_item)
{
	menu_mode_display(selection, menu_item, REGISTER_FORMAT);
}

static void on_register_format_update(const MenuItem *menu_item)
{
	GtkTreeIter iter;
	gint format = GPOINTER_TO_INT(menu_item->gdata);
	gint id;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	scp_tree_store_get(store, &iter, REGISTER_ID, &id, -1);

	if (debug_state() & DS_DEBUG)
	{
		debug_send_format(N, "02%d%c%s%s-data-list-register-values %c %d", format,
			FRAME_ARGS, register_formats[format], id);
	}
	else
		scp_tree_store_set(store, &iter, REGISTER_FORMAT, format, -1);
}

static void on_register_query(G_GNUC_UNUSED const MenuItem *menu_item)
{
	debug_send_command(N, debug_state() & DS_DEBUG ? "02-data-list-register-names" :
		"-data-list-register-names");
}

#define DS_FRESHABLE DS_DEBUG
#define DS_COPYABLE (DS_BASICS | DS_EXTRA_1)
#define DS_FORMATABLE (DS_INACTIVE | DS_DEBUG | DS_EXTRA_2)
#define DS_QUERABLE DS_SENDABLE

#define FORMAT_ITEM(format, FORMAT) \
	{ ("register_format_"format), on_register_format_update, DS_FORMATABLE, NULL, \
		GINT_TO_POINTER(FORMAT) }

static MenuItem register_menu_items[] =
{
	{ "register_refresh",  on_register_refresh,        DS_FRESHABLE,  NULL, NULL },
	{ "register_copy",     on_register_copy,           DS_COPYABLE,   NULL, NULL },
	{ "register_format",   on_register_format_display, DS_FORMATABLE, NULL, NULL },
	FORMAT_ITEM("natural", FORMAT_NATURAL),
	FORMAT_ITEM("hex",     FORMAT_HEX),
	FORMAT_ITEM("decimal", FORMAT_DECIMAL),
	FORMAT_ITEM("octal",   FORMAT_OCTAL),
	FORMAT_ITEM("binary",  FORMAT_BINARY),
	FORMAT_ITEM("raw",     FORMAT_RAW),
	{ "register_query",    on_register_query,          DS_QUERABLE,   NULL, NULL },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint register_menu_extra_state(void)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		GtkTreeIter parent;

		return (1 << DS_INDEX_1) |
			(!scp_tree_store_iter_parent(store, &parent, &iter) << DS_INDEX_2);
	}

	return 0;
}

static MenuInfo register_menu_info = { register_menu_items, register_menu_extra_state, 0 };

static void on_register_selection_changed(G_GNUC_UNUSED GtkTreeSelection *selection,
	G_GNUC_UNUSED gpointer gdata)
{
	registers_update_state(debug_state());
}

void register_init(void)
{
	tree = GTK_WIDGET(view_connect("register_view", &store, &selection, register_cells,
		"register_window", &register_display));
	gtk_widget_set_has_tooltip(tree, TRUE);
	g_signal_connect(tree, "query-tooltip", G_CALLBACK(on_register_query_tooltip),
		get_column("register_name_column"));
	menu_select("register_menu", &register_menu_info, selection);
	g_signal_connect(selection, "changed", G_CALLBACK(on_register_selection_changed), NULL);
}

void registers_finalize(void)
{
	g_free(last_gdb_executable);
}
