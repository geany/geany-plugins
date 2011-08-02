/*
 *		calltip.c
 *      
 *      Copyright 2011 Alexander Petukhov <devel(at)apetukhov(dot)ru>
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
 *		Formatting calltip text.
 */

#include <gtk/gtk.h>

#include "breakpoint.h"
#include "debug_module.h"
#include "calltip.h"

#define FIRST_LINE "\002\t%s = (%s) %s"
#define FIRST_LINE_NO_CHILDERN "%s = (%s) %s"
#define REST_LINES "\t▸\t%s = (%s) %s"
#define REST_LINES_NO_CHILDERN "\t\t%s = (%s) %s"

/*
 * creates text for a tooltip taking list or variables   
 */
GString* get_calltip_line(variable *var, gboolean firstline)
{
	GString *calltip = NULL;
	if (var && var->evaluated)
	{
		calltip = g_string_new("");
		if (firstline)
		{
			g_string_append_printf(calltip,
				var->has_children ? FIRST_LINE : FIRST_LINE_NO_CHILDERN,
				var->name->str,
				var->type->str,
				var->value->str);
		}
		else
		{
			g_string_append_printf(calltip,
				var->has_children ? REST_LINES : REST_LINES_NO_CHILDERN,
				var->name->str,
				var->type->str,
				var->value->str);
		}

		if (calltip->len > MAX_CALLTIP_LENGTH)
		{
			g_string_truncate(calltip, MAX_CALLTIP_LENGTH);
			g_string_append(calltip, " ...");
		}	
	}

	return calltip;
}

