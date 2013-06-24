/*
 *  register.h
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

#ifndef REGISTER_H

void on_register_names(GArray *nodes);
void on_register_changes(GArray *nodes);
void on_register_values(GArray *nodes);

void registers_clear(void);
gboolean registers_update(void);
void registers_query_names(void);
void registers_show(gboolean show);

void registers_update_state(DebugState state);
void registers_delete_all(void);
void registers_load(GKeyFile *config);
void registers_save(GKeyFile *config);

void register_init(void);
void registers_finalize(void);

#define REGISTER_H 1
#endif
