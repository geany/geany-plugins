/*
 *      bptree.h
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

#ifndef BPTREE_H
#define BPTREE_H

gboolean		bptree_init(move_to_line_cb callback);
void			bptree_destroy(void);
void 			bptree_add_breakpoint(breakpoint* bp);
void 			bptree_update_breakpoint(breakpoint* bp);
void 			bptree_remove_breakpoint(breakpoint* bp);
void 			bptree_set_condition(breakpoint* bp);
void 			bptree_set_hitscount(breakpoint* bp);
void 			bptree_set_enabled(breakpoint* bp);
gchar*			bptree_get_condition(breakpoint* bp);
void 			bptree_set_readonly(gboolean readonly);
void			bptree_update_file_nodes(void);

#endif /* guard */
