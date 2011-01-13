/*
 *  
 *  Copyright (C) 2010-2011  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#ifndef H_GWH_PLUGIN
#define H_GWH_PLUGIN

#include <glib.h>

#include <geanyplugin.h>
#include <geany.h>

G_BEGIN_DECLS


#define GWH_PLUGIN_NAME    "Web Helper"
#define GWH_PLUGIN_TARNAME "web-helper"
#define GWH_PLUGIN_VERSION "0.1"


extern GeanyPlugin     *geany_plugin;
extern GeanyData       *geany_data;
extern GeanyFunctions  *geany_functions;


G_END_DECLS

#endif /* guard */
