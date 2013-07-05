/*
 *  inspect.h
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

#ifndef INSPECT_H

void on_inspect_variable(GArray *nodes);
void on_inspect_format(GArray *nodes);
void on_inspect_evaluate(GArray *nodes);
void on_inspect_assign(GArray *nodes);
void on_inspect_children(GArray *nodes);
void on_inspect_ndeleted(GArray *nodes);
void on_inspect_path_expr(GArray *nodes);
void on_inspect_changelist(GArray *nodes);
void on_inspect_signal(const char *name);

void inspects_clear(void);
gboolean inspects_update(void);
void inspects_apply(void);

#define EXPAND_MAX 99999

void inspect_add(const char *text);
void inspects_update_state(DebugState state);
void inspects_delete_all(void);
void inspects_load(GKeyFile *config);
void inspects_save(GKeyFile *config);

void inspect_init(void);
void inspect_finalize(void);

#define INSPECT_H 1
#endif
