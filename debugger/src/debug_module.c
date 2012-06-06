/*
 *      debug_module.c
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

/*
 * 		Contains variable structure constructors and destructors.
 */

#include <stdlib.h>
#include <memory.h>

#include <gtk/gtk.h>

#include "breakpoint.h"
#include "debug_module.h"

/* creates new variable */
variable *variable_new(gchar *name, variable_type vt)
{
	variable *var = g_malloc(sizeof(variable));
	var->name = g_string_new(name);
	var->internal = g_string_new("");
	var->expression = g_string_new("");
	var->type = g_string_new("");
	var->value = g_string_new("");
	var->has_children = var->evaluated = FALSE;
	var->vt = vt;
	
	return var;
}

/* creates new variable with internal name */
variable *variable_new2(gchar *name, gchar *internal, variable_type vt)
{
	variable *var = variable_new(name, vt);
	g_string_assign(var->internal, internal);
	
	return var;
}

/* frees variable */
void variable_free(variable *var)
{
	g_string_free(var->name, TRUE);
	g_string_free(var->internal, TRUE);
	g_string_free(var->expression, TRUE);
	g_string_free(var->type, TRUE);
	g_string_free(var->value, TRUE);
	g_free(var);
}

/* reset all variable fields (except name) */
void variable_reset(variable *var)
{
	g_string_assign(var->internal, "");
	g_string_assign(var->expression, "");
	g_string_assign(var->type, "");
	g_string_assign(var->value, "");
	var->has_children = var->evaluated = FALSE;
}

/* creates new frame */
frame* frame_new(void)
{
	frame *f = (frame*)malloc(sizeof(frame));
	memset((void*)f, 0, sizeof(frame));
	return f;
}

/* frees a frame */
void frame_free(frame* f)
{
	if (f->address)
	{
		g_free(f->address);
	}
	if (f->function)
	{
		g_free(f->function);
	}
	if (f->file)
	{
		g_free(f->file);
	}
	
	g_free(f);
}
