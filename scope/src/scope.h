/*
 *  scope.h
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

#ifndef SCOPE_H

void plugin_blink(void);
void plugin_beep(void);
void statusbar_update_state(DebugState state);
void update_state(DebugState state);
GObject *get_object(const char *name);
GtkWidget *get_widget(const char *name);
#define get_column(name) GTK_TREE_VIEW_COLUMN(get_object(name))
void configure_toolbar(void);
void open_debug_panel(void);
void configure_panel(void);

#define SCOPE_H 1
#endif
