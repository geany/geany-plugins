/* json-utils.h - JSON utility API
 * 
 * This file is part of JSON-GLib
 * Copyright 2015  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined(__JSON_GLIB_INSIDE__) && !defined(JSON_COMPILATION)
#error "Only <json-glib/json-glib.h> can be included directly."
#endif

#include <json-glib/json-types.h>

G_BEGIN_DECLS

JSON_AVAILABLE_IN_1_2
JsonNode *      json_from_string        (const char  *str,
                                         GError     **error);
JSON_AVAILABLE_IN_1_2
char *          json_to_string          (JsonNode    *node,
                                         gboolean     pretty);

G_END_DECLS
