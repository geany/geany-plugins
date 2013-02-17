/*
 *  plugme.h
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

#ifndef PLUGME_H

#ifndef ui_add_config_file_menu_item
GtkWidget *plugme_ui_add_config_file_menu_item(const gchar *real_path, const gchar *label,
	GtkContainer *parent);
#define ui_add_config_file_menu_item plugme_ui_add_config_file_menu_item
#endif  /* ui_add_config_file_menu_item */

#define PLUGME_H 1
#endif
