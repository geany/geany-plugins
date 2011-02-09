/*
 *		utils.h
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
 
void					editor_open_position(char* file, int line);
int						get_char_width(GtkWidget *widget);
int						get_header_string_width(gchar *header, int minchars, int char_width);
GtkTreeViewColumn*		create_column(gchar *name, GtkCellRenderer *renderer, gboolean expandable, gint minwidth, gchar *arg, int value);
gboolean 				tree_foreach_set_marker(gpointer key, gpointer value, gpointer data);
GString*				get_word_at_position(ScintillaObject *sci, int position);
