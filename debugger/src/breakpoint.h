/*
 *      breakpoint.h
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
 
/* maximum condition length */
#define CONDITION_MAX_LENGTH 1024

/* structure describing a breakpoint */
typedef struct _breakpoint {
	gboolean enabled;
	gchar file[FILENAME_MAX];
	gint line;
	gchar condition[CONDITION_MAX_LENGTH + 1];
	gint hitscount;
	GtkTreeIter iter;
} breakpoint;

breakpoint*	break_new();
breakpoint*	break_new_full(char* file, int line, char* condition, int enabled, int hitscount);
