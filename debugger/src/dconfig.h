/*
 *		dconfig.h
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
 
gboolean 	dconfig_is_found_at(gchar *folder);
gboolean 	dconfig_load(gchar *folder);
gboolean	dconfig_save(gchar *folder);
void		dconfig_clear();

gchar*	dconfig_target_get();
int			dconfig_module_get();
gchar*	dconfig_args_get();
GList*		dconfig_env_get();
GList*		dconfig_breaks_get();
GList*		dconfig_watches_get();


