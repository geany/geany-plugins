/*
 * plugin.h - Part of the Geany Devhelp Plugin
 * 
 * Copyright 2010 Matthew Brush <mbrush@leftclick.ca>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
 
#ifndef PLUGIN_COMMON_H
#define PLUGIN_COMMON_H

#include <gtk/gtk.h>
#include <geanyplugin.h>
#include "devhelpplugin.h"

extern GeanyPlugin	 	*geany_plugin;
extern GeanyData	   	*geany_data;
extern GeanyFunctions 	*geany_functions;

extern DevhelpPlugin *dev_help_plugin;

gint plugin_load_preferences(void);
gint plugin_store_preferences(void);
gboolean plugin_config_init(void);

#endif
