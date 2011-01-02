/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
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

#ifndef H_GGD_TAG_UTILS
#define H_GGD_TAG_UTILS

#include <glib.h>
#include <geanyplugin.h>

#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


/**
 * GGD_SORT_ASC:
 * 
 * Constant representing ascending sort
 */
#define GGD_SORT_ASC  (1)
/**
 * GGD_SORT_DESC:
 * 
 * Constant representing descending sort
 */
#define GGD_SORT_DESC (-1)

void          ggd_tag_sort_by_line            (GPtrArray *tags,
                                               gint       direction);
GList        *ggd_tag_sort_by_line_to_list    (const GPtrArray  *tags,
                                               gint              direction);
TMTag        *ggd_tag_find_from_line          (const GPtrArray *tags,
                                               gulong           line);
TMTag        *ggd_tag_find_at_current_pos     (GeanyDocument *doc);
TMTag        *ggd_tag_find_parent             (const GPtrArray *tags,
                                               filetype_id      geany_ft,
                                               const TMTag     *child);
GList        *ggd_tag_find_children_filtered  (const GPtrArray *tags,
                                               const TMTag     *parent,
                                               filetype_id      geany_ft,
                                               gint             depth,
                                               TMTagType        filter);
GList        *ggd_tag_find_children           (const GPtrArray *tags,
                                               const TMTag     *parent,
                                               filetype_id      geany_ft,
                                               gint             depth);
gchar        *ggd_tag_resolve_type_hierarchy  (const GPtrArray *tags,
                                               filetype_id      geany_ft,
                                               const TMTag     *tag);
TMTag        *ggd_tag_find_from_name          (const GPtrArray *tags,
                                               const gchar     *name);
const gchar  *ggd_tag_get_type_name           (const TMTag *tag);
const gchar  *ggd_tag_type_get_name           (TMTagType  type);
TMTagType     ggd_tag_type_from_name          (const gchar *name);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
