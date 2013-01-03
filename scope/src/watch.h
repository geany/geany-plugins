/*
 *  watch.h
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

#ifndef WATCH_H

void on_watch_value(GArray *nodes);
void on_watch_error(GArray *nodes);

void watches_clear(void);
gboolean watches_update(void);

void watch_add(const gchar *text);
void watches_update_state(DebugState state);
void watches_delete_all(void);
void watches_load(GKeyFile *config);
void watches_save(GKeyFile *config);

void watch_init(void);

#define WATCH_H 1
#endif
