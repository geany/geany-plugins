/*
 *  memory.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "common.h"

enum
{
	MEMORY_ADDR,
	MEMORY_BYTES,
	MEMORY_ASCII
};

static ScpTreeStore *store;
static GtkTreeSelection *selection;

static void on_memory_bytes_edited(G_GNUC_UNUSED GtkCellRendererText *renderer, gchar *path_str,
	gchar *new_text, G_GNUC_UNUSED gpointer gdata)
{
	if (*new_text && (debug_state() & DS_VARIABLE))
	{
		GtkTreeIter iter;
		const char *addr, *bytes;
		guint i;

		scp_tree_store_get_iter_from_string(store, &iter, path_str);
		scp_tree_store_get(store, &iter, MEMORY_ADDR, &addr, MEMORY_BYTES, &bytes, -1);

		for (i = 0; bytes[i]; i++)
			if (!(isxdigit(bytes[i]) ? isxdigit(new_text[i]) : new_text[i] == ' '))
				break;

		if (bytes[i] || new_text[i])
			dc_error("memory: invalid format");
		else
		{
			utils_strchrepl(new_text, ' ', '\0');
			debug_send_format(T, "07-data-write-memory-bytes 0x%s%s", addr, new_text);
		}
	}
	else
		plugin_blink();
}

static gboolean on_memory_entry_key_press(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event,
	GtkEditable *editable)
{
	const char *text = gtk_entry_get_text(GTK_ENTRY(editable));
	gint pos = gtk_editable_get_position(editable);

	if (event->keyval <= 0x7F && ((isxdigit(event->keyval) && isxdigit(text[pos])) ||
		(event->keyval == ' ' && text[pos] == ' ')) && event->state <= GDK_SHIFT_MASK)
	{
		char c = event->keyval;

		gtk_editable_set_editable(editable, TRUE);
		gtk_editable_delete_text(editable, pos, pos + 1);
		gtk_editable_insert_text(editable, &c, 1, &pos);
		gtk_editable_set_position(editable, pos);
		gtk_editable_set_editable(editable, FALSE);
		return TRUE;
	}

	return event->keyval == GDK_Insert || event->keyval == GDK_KP_Insert ||
		event->keyval == GDK_space || event->keyval == GDK_KP_Space;
}

static const char *memory_font;

static void on_memory_bytes_editing_started(G_GNUC_UNUSED GtkCellRenderer *cell,
	GtkCellEditable *cell_editable, G_GNUC_UNUSED const gchar *path,
	G_GNUC_UNUSED gpointer gdata)
{
	iff (GTK_IS_ENTRY(cell_editable), "memory_bytes: not an entry")
	{
		GtkEditable *editable = GTK_EDITABLE(cell_editable);

		ui_widget_modify_font_from_string(GTK_WIDGET(editable), memory_font);
		gtk_entry_set_overwrite_mode(GTK_ENTRY(editable), TRUE);
		gtk_editable_set_editable(editable, FALSE);
		gtk_editable_set_position(editable, 0);
		g_signal_connect(editable, "key-press-event", G_CALLBACK(on_memory_entry_key_press),
			editable);
	}
}

static const TreeCell memory_cells[] =
{
	{ "memory_bytes", G_CALLBACK(on_memory_bytes_edited) },
	{ NULL, NULL }
};

static guint pointer_size;
static char *addr_format;
#define MAX_BYTES_PER_LINE 128
#define MAX_POINTER_SIZE 8

static gint back_bytes_per_line;
static gint bytes_per_line;
static gint bytes_per_group = 1;

static void memory_configure(void)
{
	gint groups_per_line;

	back_bytes_per_line = pref_memory_bytes_per_line;
	bytes_per_line = pref_memory_bytes_per_line;
	if ((unsigned) (bytes_per_line - 8) > MAX_BYTES_PER_LINE - 8)
		bytes_per_line = 16;

	groups_per_line = bytes_per_line / bytes_per_group;
	bytes_per_line = groups_per_line * bytes_per_group;
}

static guint64 memory_start;
static guint memory_count = 0;
#define MAX_BYTES (124 * MAX_BYTES_PER_LINE)

static void write_block(guint64 start, const char *contents, guint count, const char *maddr)
{
	if (!memory_count)
		memory_start = start;
	else if (memory_count < MAX_BYTES)
		memory_count = start - memory_start;

	while (memory_count < MAX_BYTES)
	{
		GtkTreeIter iter;
		char *addr = g_strdup_printf(addr_format, start);
		GString *bytes = g_string_sized_new(bytes_per_line * 3);
		GString *ascii = g_string_new(" ");
		gint n = 0;

		while (n < bytes_per_line)
		{
			char locale;
			gchar *utf8;

			g_string_append_len(bytes, contents, 2);
			contents += 2;
			memory_count++;
			locale = strtol(bytes->str + bytes->len - 2, NULL, 16);
			utf8 = locale >= 0x20 ? g_locale_to_utf8(&locale, 1, NULL, NULL, NULL) : NULL;

			if (utf8)
			{
				g_string_append(ascii, utf8);
				g_free(utf8);
			}
			else
				g_string_append_c(ascii, '.');  /* 0xfffd? */

			if (++n % bytes_per_group == 0)
				g_string_append_c(bytes, ' ');

			if (!--count)
				break;
		}

		while (n < bytes_per_line)
		{
			g_string_append(bytes, "  ");

			if (++n % bytes_per_group == 0)
				g_string_append_c(bytes, ' ');
		}

		scp_tree_store_append_with_values(store, &iter, NULL, MEMORY_ADDR, addr,
			MEMORY_BYTES, bytes->str, MEMORY_ASCII, ascii->str, -1);
		if (!g_strcmp0(addr, maddr))
			gtk_tree_selection_select_iter(selection, &iter);

		g_free(addr);
		g_string_free(bytes, TRUE);
		g_string_free(ascii, TRUE);

		if (!count)
			break;

		start += bytes_per_line;
	}

	if (count)
		dc_error("memory: too much data");
}

static void memory_node_read(const ParseNode *node, const char *maddr)
{
	iff (node->type == PT_ARRAY, "memory: contains value")
	{
		GArray *nodes = (GArray *) node->value;
		const char *begin = parse_find_value(nodes, "begin");
		const char *offset = parse_find_value(nodes, "offset");
		const char *contents = parse_find_value(nodes, "contents");

		iff (begin && contents, "memory: no begin or contents")
		{
			guint64 start = g_ascii_strtoull(begin, NULL, 0);
			guint64 count = strlen(contents) / 2;

			if (offset)
				start += g_ascii_strtoull(offset, NULL, 0);

			iff (count, "memory: contents too short")
				write_block(start, contents, count, maddr);
		}
	}
}

void on_memory_read_bytes(GArray *nodes)
{
	if (pointer_size <= MAX_POINTER_SIZE)
	{
		GtkTreeIter iter;
		char *maddr = NULL;

		if (gtk_tree_selection_get_selected(selection, NULL, &iter))
			gtk_tree_model_get((GtkTreeModel *) store, &iter, MEMORY_ADDR, &maddr, -1);

		store_clear(store);
		memory_count = 0;

		if (pref_memory_bytes_per_line != back_bytes_per_line)
		{
			memory_configure();
			gtk_tree_view_column_queue_resize(get_column("memory_bytes_column"));
			gtk_tree_view_column_queue_resize(get_column("memory_ascii_column"));
		}

		parse_foreach(parse_lead_array(nodes), (GFunc) memory_node_read, maddr);
		g_free(maddr);
	}
}

void memory_clear(void)
{
	store_clear(store);
}

gboolean memory_update(void)
{
	if (memory_count)
	{
		debug_send_format(T, "04-data-read-memory-bytes 0x%" G_GINT64_MODIFIER "x %u",
			memory_start, memory_count);
	}
	return TRUE;
}

static void on_memory_refresh(G_GNUC_UNUSED const MenuItem *menu_item)
{
	debug_send_format(T, "-data-read-memory-bytes 0x%" G_GINT64_MODIFIER "x %u",
		memory_start, memory_count);
}

static void on_memory_read(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GString *command = g_string_new("-data-read-memory-bytes ");
	gchar *expr = utils_get_default_selection();

	if (expr)
	{
		g_string_append(command, expr);
		g_free(expr);
	}
	else if (memory_count)
	{
		g_string_append_printf(command, "0x%" G_GINT64_MODIFIER "x %u", memory_start,
			memory_count);
	}

	view_command_line(command->str, _("Read Memory"), " ", TRUE);
	g_string_free(command, TRUE);
}

static void on_memory_copy(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const char *addr, *bytes;
	const gchar *ascii;
	gchar *string;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	scp_tree_store_get(store, &iter, MEMORY_ADDR, &addr, MEMORY_BYTES, &bytes,
		MEMORY_ASCII, &ascii, -1);
	string = g_strdup_printf("%s%s%s", addr, bytes, ascii);
	gtk_clipboard_set_text(gtk_widget_get_clipboard(menu_item->widget,
		GDK_SELECTION_CLIPBOARD), string, -1);
	g_free(string);
}

static void on_memory_clear(G_GNUC_UNUSED const MenuItem *menu_item)
{
	store_clear(store);
	memory_count = 0;
}

static void on_memory_group_display(const MenuItem *menu_item)
{
	guint i;

	for (i = 0; (1 << i) < bytes_per_group; i++);
	menu_item_set_active(menu_item + i + 1, TRUE);
}

static void on_memory_group_update(const MenuItem *menu_item)
{
	bytes_per_group = 1 << GPOINTER_TO_INT(menu_item->gdata);
	back_bytes_per_line = 0;

	if (memory_count)
		on_memory_refresh(menu_item);
}

#define DS_FRESHABLE (DS_VRIABLE | DS_EXTRA_2)
#define DS_COPYABLE (DS_BASICS | DS_EXTRA_1)
#define DS_CLEARABLE (DS_ACTIVE | DS_EXTRA_2)

#define GROUP_ITEM(count, POWER) \
	{ ("memory_group_"count), on_memory_group_update, DS_VARIABLE, NULL, \
		GINT_TO_POINTER(POWER) }

static MenuItem memory_menu_items[] =
{
	{ "memory_refresh", on_memory_refresh,       DS_VARIABLE,  NULL, NULL },
	{ "memory_read",    on_memory_read,          DS_VARIABLE,  NULL, NULL },
	{ "memory_copy",    on_memory_copy,          DS_COPYABLE,  NULL, NULL },
	{ "memory_clear",   on_memory_clear,         DS_CLEARABLE, NULL, NULL },
	{ "memory_group",   on_memory_group_display, DS_VARIABLE,  NULL, NULL },
	GROUP_ITEM("1", 0),
	GROUP_ITEM("2", 1),
	GROUP_ITEM("4", 2),
	GROUP_ITEM("8", 3),
	{ NULL, NULL, 0, NULL, NULL }
};

static guint memory_menu_extra_state(void)
{
	return (gtk_tree_selection_get_selected(selection, NULL, NULL) << DS_INDEX_1) |
		(memory_count != 0) << DS_INDEX_2;
}

static MenuInfo memory_menu_info = { memory_menu_items, memory_menu_extra_state, 0 };

static gboolean on_memory_key_press(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event,
	gpointer gdata)
{
	if (event->keyval == GDK_Insert || event->keyval == GDK_KP_Insert)
	{
		menu_item_execute(&memory_menu_info, (const MenuItem *) gdata, FALSE);
		return TRUE;
	}

	return FALSE;
}

void memory_init(void)
{
	GtkWidget *tree = GTK_WIDGET(view_connect("memory_view", &store, &selection,
		memory_cells, "memory_window", NULL));

	memory_font = *pref_memory_font ? pref_memory_font : pref_vte_font;
	ui_widget_modify_font_from_string(tree, memory_font);
	g_signal_connect(get_object("memory_bytes"), "editing-started",
		G_CALLBACK(on_memory_bytes_editing_started), NULL);
	g_signal_connect(tree, "key-press-event", G_CALLBACK(on_memory_key_press),
		(gpointer) menu_item_find(memory_menu_items, "memory_read"));

	pointer_size = MAX(sizeof(void *), sizeof(&memory_init));
	addr_format = g_strdup_printf("%%0%u" G_GINT64_MODIFIER "x  ", pointer_size * 2);
	memory_configure();

	if (pointer_size > MAX_POINTER_SIZE)
	{
		msgwin_status_add(_("Scope: pointer size > %d, Data disabled."), MAX_POINTER_SIZE);
		gtk_widget_hide(tree);
	}
	else
		menu_connect("memory_menu", &memory_menu_info, tree);
}

void memory_finalize(void)
{
	g_free(addr_format);
}
