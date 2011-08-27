/*
 *      breakpoints.h
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

typedef enum _break_state {
	BS_NOT_SET,
	BS_ENABLED,
	BS_DISABLED
} break_state;

typedef void	(*move_to_line_cb)(char* file, int line);

gboolean		breaks_init(move_to_line_cb callback);
void			breaks_destroy();
void			breaks_add(const char* file, int line, char* condition, int enable, int hitscount);
void			breaks_remove(const char* file, int line);
void			breaks_remove_all();
void			breaks_switch(const char* file, int line);
void			breaks_set_hits_count(char* file, int line, int count);
void			breaks_set_condition(char* file, int line, char* condition);
void			breaks_move_to_line(char* file, int line_from, int line_to);
break_state	breaks_get_state(const char* file, int line);
GtkWidget*	breaks_get_widget();
GList*			breaks_get_for_document(const char* file);
GList*			breaks_get_all();
void			breaks_read_config();

