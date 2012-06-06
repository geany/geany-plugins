/*
 *      breakpoints.h
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

#include "breakpoint.h"

typedef enum _break_state {
	BS_NOT_SET,
	BS_ENABLED,
	BS_DISABLED
} break_state;

typedef void	(*move_to_line_cb)(const char* file, int line);
typedef void	(*select_frame_cb)(int frame_number);

gboolean		breaks_init(move_to_line_cb callback);
void			breaks_destroy(void);
void			breaks_add(const char* file, int line, char* condition, int enable, int hitscount);
void			breaks_remove(const char* file, int line);
void			breaks_remove_list(GList *list);
void			breaks_remove_all(void);
void			breaks_switch(const char *file, int line);
void			breaks_set_hits_count(const char *file, int line, int count);
void			breaks_set_condition(const char *file, int line, const char* condition);
void			breaks_set_enabled_for_file(const const char *file, gboolean enabled);
void			breaks_move_to_line(const char* file, int line_from, int line_to);
break_state		breaks_get_state(const char* file, int line);
GList*			breaks_get_for_document(const char* file);
GList*			breaks_get_all(void);
breakpoint*		breaks_lookup_breakpoint(const gchar* file, int line);
