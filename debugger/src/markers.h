/*
 *		markers.h
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
 
void markers_init(void);
void markers_set_for_document(ScintillaObject *sci);
void markers_add_breakpoint(breakpoint* bp);
void markers_remove_breakpoint(breakpoint* bp);
void markers_add_current_instruction(char* file, int line);
void markers_remove_current_instruction(char* file, int line);
void markers_add_frame(char* file, int line);
void markers_remove_frame(char* file, int line);
void markers_remove_all(GeanyDocument *doc);


