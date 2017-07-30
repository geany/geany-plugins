/*
 * Copyright 2017 LarsGit223
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

#ifndef __WB_MENU_H__
#define __WB_MENU_H__


gboolean menu_init(void);
void menu_cleanup (void);
void menu_item_new_activate (void);
void menu_item_new_deactivate (void);
void menu_item_open_activate (void);
void menu_item_open_deactivate (void);
void menu_item_save_activate (void);
void menu_item_save_deactivate (void);
void menu_item_settings_activate (void);
void menu_item_settings_deactivate (void);
void menu_item_close_activate (void);
void menu_item_close_deactivate (void);

#endif
