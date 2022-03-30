/*
 *		tpage.h
 *      
 *	    Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
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

#ifndef TPAGE_H
#define TPAGE_H

#include <glib.h>
#include <gtk/gtk.h>

void			tpage_init(void);

gchar*		tpage_get_target(void);
void			tpage_set_target(const gchar *newvalue);

gchar*		tpage_get_debugger(void);
void			tpage_set_debugger(const gchar *newvalue);

gchar*		tpage_get_debugger_mode(void);
void			tpage_set_debugger_mode(const gchar *newvalue);

int				tpage_get_debug_module_index(void);
int             tpage_get_debug_mode_index(void);

gchar*		tpage_get_commandline(void);
void			tpage_set_commandline(const gchar *newvalue);

GList*			tpage_get_environment(void);
void			tpage_add_environment(const gchar *name, const gchar *value);

void			tpage_set_readonly(gboolean readonly);
void			tpage_clear(void);

void			tpage_pack_widgets(gboolean tabbed);

#endif /* guard */
