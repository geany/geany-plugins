/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2009-2010  Colomban Wendling <ban@herbesfolles.org>
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

#ifndef H_GGD
#define H_GGD

#include <glib.h>
#include <geanyplugin.h>

#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


gboolean        ggd_insert_comment            (GeanyDocument *doc,
                                               gint           line,
                                               const gchar   *doc_type);
gboolean        ggd_insert_all_comments       (GeanyDocument *doc,
                                               const gchar   *doc_type);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
