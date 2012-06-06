/*
 *		stree.h
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

GtkWidget*		stree_init(move_to_line_cb ml, select_frame_cb sf);
void			stree_destroy(void);

void 			stree_add(frame *f);
void 			stree_clear(void);

void 			stree_add_thread(int thread_id);
void 			stree_remove_thread(int thread_id);

void 			stree_select_first_frame(gboolean make_active);
void 			stree_remove_frames(void);

void			stree_set_active_thread_id(int thread_id);
