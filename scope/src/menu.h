/*
 *  menu.h
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

#ifndef MENU_H

struct _MenuItem
{
	const char *name;
	void (*callback)(const MenuItem *menu_item);
	guint state;
	GtkWidget *widget;  /* automatic */
	gpointer gdata;
};

typedef struct _MenuInfo
{
	MenuItem *items;
	guint (*extra_state)(void);
	guint last_state;  /* must be 0 */
} MenuInfo;

const MenuItem *menu_item_find(const MenuItem *menu_items, const char *name);
gboolean menu_item_matches_state(const MenuItem *menu_item, guint state);
void menu_item_execute(const MenuInfo *menu_info, const MenuItem *menu_item, gboolean beep);
void menu_item_set_active(const MenuItem *menu_item, gboolean active);  /* block execute */

gboolean menu_insert_delete(const GdkEventKey *event, const MenuInfo *menu_info,
	const char *insert_name, const char *delete_name);
void menu_shift_button_release(GtkWidget *widget, GdkEventButton *event, GtkWidget *menu,
	void (action)(const MenuItem *menu_item));

GtkWidget *menu_connect(const char *name, MenuInfo *menu_info, GtkWidget *widget);
GtkWidget *menu_select(const char *name, MenuInfo *menu_info, GtkTreeSelection *selection);

enum
{
	COLUMN_NAME,
	COLUMN_DISPLAY,
	COLUMN_VALUE,
	COLUMN_HB_MODE,
	COLUMN_MR_MODE
};

#define MENU_HBIT_ITEM(prefix, mode, MODE) \
	{ (#prefix"_hb_"mode), (on_##prefix##_hbit_update), DS_REPARSABLE, NULL, \
		GINT_TO_POINTER(MODE) }

/* note: N references to prefix */
#define MENU_HBIT_ITEMS(prefix) \
	{ (#prefix"_hb_mode"), (on_##prefix##_hbit_display), DS_REPARSABLE, NULL, NULL }, \
	MENU_HBIT_ITEM(prefix, "default", HB_DEFAULT), \
	MENU_HBIT_ITEM(prefix, "7bit",    HB_7BIT), \
	MENU_HBIT_ITEM(prefix, "locale",  HB_LOCALE), \
	MENU_HBIT_ITEM(prefix, "utf8",    HB_UTF8)

void menu_mode_display(GtkTreeSelection *selection, const MenuItem *menu_item, gint column);
void menu_mode_update(GtkTreeSelection *selection, gint new_mode, gboolean hbit);
#define menu_hbit_display(selection, menu_item) menu_mode_display((selection), (menu_item), \
	COLUMN_HB_MODE)
#define menu_hbit_update(selection, menu_item) menu_mode_update((selection), \
	GPOINTER_TO_INT((menu_item)->gdata), TRUE)

void menu_mber_display(GtkTreeSelection *selection, const MenuItem *menu_item);
void menu_mber_update(GtkTreeSelection *selection, const MenuItem *menu_item);
void menu_mber_button_release(GtkTreeSelection *selection, GtkWidget *item,
	GdkEventButton *event, GtkWidget *menu);

void menu_copy(GtkTreeSelection *selection, const MenuItem *menu_item);
void menu_modify(GtkTreeSelection *selection, const MenuItem *menu_item);
void menu_inspect(GtkTreeSelection *selection);

void on_menu_display_booleans(const MenuItem *menu_item);
void on_menu_update_boolean(const MenuItem *menu_item);
void on_menu_evaluate_value(GArray *nodes);

typedef struct _MenuKey
{
	const char *name;
	const gchar *label;
} MenuKey;

void menu_set_popup_keybindings(GeanyKeyGroup *scope_key_group, guint item);
void menu_clear(void);
void menu_update_state(DebugState state);

void menu_init(void);
void menu_finalize(void);

#define MENU_H 1
#endif
