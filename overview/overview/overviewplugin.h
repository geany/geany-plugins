/*
 * overviewplugin.h - This file is part of the Geany Overview plugin
 *
 * Copyright (c) 2015 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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
 *
 */

#ifndef OVERVIEW_PLUGIN_H
#define OVERVIEW_PLUGIN_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <geanyplugin.h>

extern GeanyData      *geany_data;
extern GeanyPlugin    *geany_plugin;
extern GeanyFunctions *geany_functions;

void     overview_plugin_queue_update (void);
gboolean overview_geany_supports_left_position (void);

#endif // OVERVIEW_PLUGIN_H
