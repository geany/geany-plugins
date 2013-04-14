/*
 *  stack.h
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

#ifndef STACK_H

extern const char *frame_id;

void on_stack_frames(GArray *nodes);
void on_stack_arguments(GArray *nodes);
void on_stack_follow(GArray *nodes);

gboolean stack_entry(void);
void stack_clear(void);
gboolean stack_update(void);

void stack_init(void);

#define STACK_H 1
#endif
