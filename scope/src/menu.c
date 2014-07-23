/*
 *  menu.c
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
#include <gdk/gdkkeysyms.h>

#include "common.h"

const MenuItem *menu_item_find(const MenuItem *menu_items, const char *name)
{
	const MenuItem *menu_item;

	for (menu_item = menu_items; menu_item->name; menu_item++)
		if (!strcmp(menu_item->name, name))
			break;

	g_assert(menu_item->name);
	return menu_item;
}

gboolean menu_item_matches_state(const MenuItem *menu_item, guint state)
{
	return (menu_item->state & DS_BASICS & state) &&
		(menu_item->state & DS_EXTRAS) == (menu_item->state & DS_EXTRAS & state);
}

void menu_item_execute(const MenuInfo *menu_info, const MenuItem *menu_item, gboolean beep)
{
	guint state = debug_state() | menu_info->extra_state();

	if (!menu_item->state || menu_item_matches_state(menu_item, state))
		menu_item->callback(menu_item);
	else if (beep)
		plugin_beep();
}

static gboolean block_execute = FALSE;

void menu_item_set_active(const MenuItem *menu_item, gboolean active)
{
	block_execute = TRUE;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item->widget), active);
	block_execute = FALSE;
}

gboolean menu_insert_delete(const GdkEventKey *event, const MenuInfo *menu_info,
	const char *insert_name, const char *delete_name)
{
	const char *name;

	if (event->keyval == GDK_Insert || event->keyval == GDK_KP_Insert)
		name = insert_name;
	else if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete)
		name = delete_name;
	else
		return FALSE;

	menu_item_execute(menu_info, menu_item_find(menu_info->items, name), FALSE);
	return TRUE;
}

void menu_shift_button_release(GtkWidget *widget, GdkEventButton *event, GtkWidget *menu,
	void (*action)(const MenuItem *menu_item))
{
	if (event->state & GDK_SHIFT_MASK)
	{
		gtk_menu_popdown(GTK_MENU(menu));
		action(NULL);
	}
	else
		utils_handle_button_release(widget, event);
}

static void on_menu_item_activate(GtkMenuItem *menuitem, MenuInfo *menu_info)
{
	if (!block_execute)
	{
		const MenuItem *menu_item;
		GtkWidget *widget = GTK_WIDGET(menuitem);

		for (menu_item = menu_info->items; menu_item->widget; menu_item++)
			if (menu_item->widget == widget)
				break;

		g_assert(menu_item->widget);

		if (!GTK_IS_RADIO_MENU_ITEM(menuitem) ||
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)))
		{
			menu_item_execute(menu_info, menu_item, TRUE);
		}
	}
}

static MenuInfo *active_menu = NULL;

static void update_active_menu(guint state)
{
	state |= active_menu->extra_state();

	if (state != active_menu->last_state)
	{
		const MenuItem *menu_item;

		for (menu_item = active_menu->items; menu_item->name; menu_item++)
		{
			if (menu_item->state)
			{
				gtk_widget_set_sensitive(menu_item->widget,
					menu_item_matches_state(menu_item, state));
			}
		}

		active_menu->last_state = state;
	}
}

static void on_menu_show(G_GNUC_UNUSED GtkWidget *widget, MenuInfo *menu_info)
{
	active_menu = menu_info;
	update_active_menu(debug_state());
}

static void on_menu_hide(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gpointer gdata)
{
	active_menu = NULL;
}

static gboolean on_button_3_press(GtkWidget *widget, GdkEventButton *event, GtkMenu *menu)
{
	if (event->button == 3)
	{
		utils_handle_button_press(widget, event);
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

GtkWidget *menu_connect(const char *name, MenuInfo *menu_info, GtkWidget *widget)
{
	MenuItem *menu_item;
	GtkWidget *menu = get_widget(name);

	g_signal_connect(menu, "show", G_CALLBACK(on_menu_show), menu_info);
	g_signal_connect(menu, "hide", G_CALLBACK(on_menu_hide), NULL);

	for (menu_item = menu_info->items; menu_item->name; menu_item++)
	{
		menu_item->widget = get_widget(menu_item->name);

		g_signal_connect(menu_item->widget,
			GTK_IS_CHECK_MENU_ITEM(menu_item->widget) ? "toggled" : "activate",
			G_CALLBACK(on_menu_item_activate), menu_info);
	}

	if (widget)
		g_signal_connect(widget, "button-press-event", G_CALLBACK(on_button_3_press), menu);

	return menu;
}

static void on_selection_changed(G_GNUC_UNUSED GtkTreeSelection *selection, GtkWidget *menu)
{
	if (gtk_widget_get_visible(menu))
		gtk_menu_popdown(GTK_MENU(menu));
}

GtkWidget *menu_select(const char *name, MenuInfo *menu_info, GtkTreeSelection *selection)
{
	GtkTreeView *tree = gtk_tree_selection_get_tree_view(selection);
	GtkWidget *menu = menu_connect(name, menu_info, GTK_WIDGET(tree));

	g_signal_connect(selection, "changed", G_CALLBACK(on_selection_changed), menu);
	return menu;
}

void menu_mode_display(GtkTreeSelection *selection, const MenuItem *menu_item, gint column)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint mode;

	gtk_tree_selection_get_selected(selection, &model, &iter);
	gtk_tree_model_get(model, &iter, column, &mode, -1);
	menu_item_set_active(menu_item + mode + 1, TRUE);
}

static void menu_mode_update_iter(ScpTreeStore *store, GtkTreeIter *iter, gint new_mode,
	gboolean hbit)
{
	gint hb_mode, mr_mode;
	const char *value;
	gchar *display;

	scp_tree_store_get(store, iter, COLUMN_VALUE, &value, COLUMN_HB_MODE, &hb_mode,
		COLUMN_MR_MODE, &mr_mode, -1);

	if (hbit)
		hb_mode = new_mode;
	else
		mr_mode = new_mode;

	display = parse_get_display_from_7bit(value, hb_mode, mr_mode);
	scp_tree_store_set(store, iter, COLUMN_HB_MODE, hb_mode, COLUMN_MR_MODE, mr_mode,
		value ? COLUMN_DISPLAY : -1, display, -1);
	g_free(display);
}

void menu_mode_update(GtkTreeSelection *selection, gint new_mode, gboolean hbit)
{
	ScpTreeStore *store;
	GtkTreeIter iter;
	const char *name;

	scp_tree_selection_get_selected(selection, &store, &iter);
	scp_tree_store_get(store, &iter, COLUMN_NAME, &name, -1);
	menu_mode_update_iter(store, &iter, new_mode, hbit);
	parse_mode_update(name, hbit ? MODE_HBIT : MODE_MEMBER, new_mode);

	if (hbit)
	{
		char *reverse = parse_mode_reentry(name);

		if (store_find(store, &iter, COLUMN_NAME, reverse))
			menu_mode_update_iter(store, &iter, new_mode, TRUE);
		g_free(reverse);
	}
}

void menu_mber_display(GtkTreeSelection *selection, const MenuItem *menu_item)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM(menu_item->widget);
		gint mr_mode;

		gtk_tree_model_get(model, &iter, COLUMN_MR_MODE, &mr_mode, -1);

		if (mr_mode == MR_DEFAULT)
			gtk_check_menu_item_set_inconsistent(item, TRUE);
		else
		{
			gtk_check_menu_item_set_inconsistent(item, FALSE);
			menu_item_set_active(menu_item, mr_mode);
		}
	}
}

void menu_mber_update(GtkTreeSelection *selection, const MenuItem *menu_item)
{
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM(menu_item->widget);

	if (gtk_check_menu_item_get_inconsistent(item))
	{
		gtk_check_menu_item_set_inconsistent(item, FALSE);
		menu_item_set_active(menu_item, !option_member_names);
	}

	menu_mode_update(selection, gtk_check_menu_item_get_active(item), FALSE);
}

void menu_mber_button_release(GtkTreeSelection *selection, GtkWidget *item,
	GdkEventButton *event, GtkWidget *menu)
{
	if (event->state & GDK_SHIFT_MASK)
	{
		gtk_check_menu_item_set_inconsistent(GTK_CHECK_MENU_ITEM(item), TRUE);
		menu_mode_update(selection, MR_DEFAULT, FALSE);
		gtk_menu_popdown(GTK_MENU(menu));
	}
	else
		utils_handle_button_release(item, event);
}

void menu_copy(GtkTreeSelection *selection, const MenuItem *menu_item)
{
	ScpTreeStore *store;
	GtkTreeIter iter;
	const gchar *name, *display;
	const char *value;
	GString *string;

	scp_tree_selection_get_selected(selection, &store, &iter);
	scp_tree_store_get(store, &iter, COLUMN_NAME, &name, COLUMN_DISPLAY, &display,
		COLUMN_VALUE, &value, -1);
	string = g_string_new(name);

	if (value)
		g_string_append_printf(string, " = %s", display);

	gtk_clipboard_set_text(gtk_widget_get_clipboard(menu_item->widget,
		GDK_SELECTION_CLIPBOARD), string->str, string->len);

	g_string_free(string, TRUE);
}

static GtkWidget *modify_dialog;
static GtkLabel *modify_value_label;
static GtkWidget *modify_value;
static GtkTextBuffer *modify_text;
static GtkWidget *modify_ok;

static void modify_dialog_update_state(DebugState state)
{
	if (state == DS_INACTIVE)
		gtk_widget_hide(modify_dialog);
	else
		gtk_widget_set_sensitive(modify_ok, (state & DS_SENDABLE) != 0);
}

static void menu_evaluate_modify(const gchar *expr, const char *value, const gchar *title,
	gint hb_mode, gint mr_mode, const char *prefix)
{
	gchar *display = parse_get_display_from_7bit(value, hb_mode, mr_mode);
	gchar *text = g_strdup_printf("%s = %s", expr, display ? display : "");
	GtkTextIter iter;

	g_free(display);
	gtk_window_set_title(GTK_WINDOW(modify_dialog), title);
	gtk_widget_grab_focus(modify_value);
	gtk_text_buffer_set_text(modify_text, text, -1);
	g_free(text);
	gtk_text_buffer_get_iter_at_offset(modify_text, &iter, g_utf8_strlen(expr, -1) + 3);
	gtk_text_buffer_place_cursor(modify_text, &iter);
	modify_dialog_update_state(debug_state());

	if (gtk_dialog_run(GTK_DIALOG(modify_dialog)) == GTK_RESPONSE_ACCEPT)
	{
		text = utils_text_buffer_get_text(modify_text, -1);
		utils_strchrepl(text, '\n', ' ');

		if (validate_column(text, TRUE))
		{
			char *locale = utils_get_locale_from_display(text, hb_mode);
			debug_send_format(F, "%s-gdb-set var %s", prefix ? prefix : "", locale);
			g_free(locale);
		}
		g_free(text);
	}
}

void menu_modify(GtkTreeSelection *selection, const MenuItem *menu_item)
{
	ScpTreeStore *store;
	GtkTreeIter iter;
	const gchar *name;
	const char *value;
	gint hb_mode;

	scp_tree_selection_get_selected(selection, &store, &iter);
	scp_tree_store_get(store, &iter, COLUMN_NAME, &name, COLUMN_VALUE, &value, COLUMN_HB_MODE,
		&hb_mode, -1);
	menu_evaluate_modify(name, value, _("Modify"), hb_mode, menu_item ? MR_MODIFY : MR_MODSTR,
		"07");
}

void menu_inspect(GtkTreeSelection *selection)
{
	ScpTreeStore *store;
	GtkTreeIter iter;
	const char *name;

	scp_tree_selection_get_selected(selection, &store, &iter);
	scp_tree_store_get(store, &iter, COLUMN_NAME, &name, -1);
	inspect_add(name);
}

void on_menu_display_booleans(const MenuItem *menu_item)
{
	gint i, count = GPOINTER_TO_INT(menu_item->gdata);

	for (i = 0; i < count; i++)
	{
		menu_item++;
		menu_item_set_active(menu_item, *(gboolean *) menu_item->gdata);
	}
}

void on_menu_update_boolean(const MenuItem *menu_item)
{
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM(menu_item->widget);
	*(gboolean *) menu_item->gdata = gtk_check_menu_item_get_active(item);
}

static char *input = NULL;
static gint eval_mr_mode;
static gint scid_gen = 0;

void on_menu_evaluate_value(GArray *nodes)
{
	if (atoi(parse_grab_token(nodes)) == scid_gen && !gtk_widget_get_visible(modify_dialog))
	{
		gchar *expr = utils_get_utf8_from_locale(input);

		menu_evaluate_modify(expr, parse_lead_value(nodes), "Evaluate/Modify",
			parse_mode_get(input, MODE_HBIT), eval_mr_mode, NULL);
		g_free(expr);
	}
}

static void on_popup_evaluate(const MenuItem *menu_item)
{
	gchar *expr = utils_get_default_selection();

	g_free(input);
	eval_mr_mode = menu_item ? MR_MODIFY : MR_MODSTR;
	input = debug_send_evaluate('8', ++scid_gen, expr);
	g_free(expr);
}

static void on_popup_watch(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gchar *expr = utils_get_default_selection();
	watch_add(expr);
	g_free(expr);
}

static void on_popup_inspect(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gchar *expr = utils_get_default_selection();
	inspect_add(expr);
	g_free(expr);
}

#define DS_EVALUATE (DS_SENDABLE | DS_EXTRA_1)

static MenuItem popup_menu_items[] =
{
	{ "popup_evaluate", on_popup_evaluate, DS_EVALUATE, NULL, NULL },
	{ "popup_watch",    on_popup_watch,    0,           NULL, NULL },
	{ "popup_inspect",  on_popup_inspect,  DS_NOT_BUSY, NULL, NULL },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint popup_menu_extra_state(void)
{
	gchar *expr = utils_get_default_selection();
	g_free(expr);
	return (expr != NULL) << DS_INDEX_1;
}

static MenuInfo popup_menu_info = { popup_menu_items, popup_menu_extra_state, 0 };

static guint popup_start;

static void on_popup_key(guint key_id)
{
	menu_item_execute(&popup_menu_info, popup_menu_items + key_id - popup_start, FALSE);
}

static void on_popup_evaluate_button_release(GtkWidget *widget, GdkEventButton *event,
	GtkWidget *menu)
{
	menu_shift_button_release(widget, event, menu, on_popup_evaluate);
}

static MenuKey popup_menu_keys[] =
{
	{ "evaluate", N_("Evaluate/modify") },
	{ "watch",    N_("Watch expression") },
	{ "inspect",  N_("Inspect variable") }
};

void menu_set_popup_keybindings(GeanyKeyGroup *scope_key_group, guint item)
{
	const MenuKey *menu_key = popup_menu_keys;
	const MenuItem *menu_item;

	popup_start = item;

	for (menu_item = popup_menu_items; menu_item->name; menu_item++, menu_key++, item++)
	{
		keybindings_set_item(scope_key_group, item, on_popup_key, 0, 0, menu_key->name,
			_(menu_key->label), popup_menu_items[item].widget);
	}
}

void menu_clear(void)
{
	scid_gen = 0;
}

void menu_update_state(DebugState state)
{
	if (active_menu)
		update_active_menu(state);

	if (gtk_widget_get_visible(modify_dialog))
		modify_dialog_update_state(state);
}

static GtkWidget *popup_item;

void menu_init(void)
{
	GtkMenuShell *shell = GTK_MENU_SHELL(geany->main_widgets->editor_menu);
	GList *children = gtk_container_get_children(GTK_CONTAINER(shell));
	GtkWidget *search2 = ui_lookup_widget(GTK_WIDGET(shell), "search2");

	popup_item = get_widget("popup_item");
	menu_connect("popup_menu", &popup_menu_info, NULL);
	g_signal_connect(get_widget("popup_evaluate"), "button-release-event",
		G_CALLBACK(on_popup_evaluate_button_release), geany->main_widgets->editor_menu);

	if (search2)
		gtk_menu_shell_insert(shell, popup_item, g_list_index(children, search2) + 1);
	else
		gtk_menu_shell_append(shell, popup_item);

	modify_dialog = dialog_connect("modify_dialog");
	modify_value_label = GTK_LABEL(get_widget("modify_value_label"));
	modify_value = get_widget("modify_value");
	modify_text = gtk_text_view_get_buffer(GTK_TEXT_VIEW(modify_value));
	modify_ok = get_widget("modify_ok");
	utils_enter_to_clicked(modify_value, modify_ok);
}

void menu_finalize(void)
{
	gtk_widget_destroy(modify_dialog);
	gtk_widget_destroy(popup_item);
	g_free(input);
}
