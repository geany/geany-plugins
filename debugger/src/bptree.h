/*
 *      bptree.h
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
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

gboolean	bptree_init(move_to_line_cb callback);
GtkWidget*	bptree_get_widget();
void 		bptree_add_breakpoint(breakpoint* bp);
void 		bptree_update_breakpoint(breakpoint* bp);
void 		bptree_remove_breakpoint(breakpoint* bp);
void 		bptree_set_condition(GtkTreeIter iter, gchar* condition);
void 		bptree_set_hitscount(GtkTreeIter iter, int hitscount);
void 		bptree_set_enabled(GtkTreeIter iter, gboolean enabled);
gchar* 		bptree_get_condition(GtkTreeIter iter);
void 		bptree_set_readonly(gboolean readonly);
