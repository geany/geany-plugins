/*
 *      breakpoint.c
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

/*
 * 		Functions for creating new breakpoints. 
 */

#include <gtk/gtk.h>
#include <memory.h>
#include "breakpoint.h"

/*
 * create new empty breakpoint
 */
breakpoint* break_new()
{
	breakpoint* bp = (breakpoint*)g_malloc(sizeof(breakpoint));
	memset(bp, 0 , sizeof(breakpoint));
	
	return bp;
}

/*
 * create new breakpoint with parameters
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 * 		condition - breakpoints line
 * 		enabled - is new breakpoint enabled
 * 		hitscount - breakpoints hitscount
*/
breakpoint* break_new_full(char* file, int line, char* condition, int enabled, int hitscount)
{
	breakpoint* bp = break_new();
	strcpy(bp->file, file);
	bp->line = line;
	if (condition)
		strcpy(bp->condition, condition);
	bp->enabled = enabled;
	bp->hitscount = hitscount;

	return bp;
}


