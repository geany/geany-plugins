/* json-path.h - JSONPath implementation
 *
 * This file is part of JSON-GLib
 * Copyright © 2011  Intel Corp.
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
 *
 * Author:
 *   Emmanuele Bassi  <ebassi@linux.intel.com>
 */

#pragma once

#if !defined(__JSON_GLIB_INSIDE__) && !defined(JSON_COMPILATION)
#error "Only <json-glib/json-glib.h> can be included directly."
#endif

#include <json-glib/json-types.h>

G_BEGIN_DECLS

#define JSON_TYPE_PATH          (json_path_get_type ())
#define JSON_PATH(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), JSON_TYPE_PATH, JsonPath))
#define JSON_IS_PATH(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JSON_TYPE_PATH))

/**
 * JSON_PATH_ERROR:
 *
 * Error domain for `JsonPath`.
 *
 * Since: 0.14
 */
#define JSON_PATH_ERROR         (json_path_error_quark ())

/**
 * JsonPathError:
 * @JSON_PATH_ERROR_INVALID_QUERY: Invalid query
 *
 * Error codes for `JSON_PATH_ERROR`.
 *
 * This enumeration can be extended at later date
 *
 * Since: 0.14
 */
typedef enum {
  JSON_PATH_ERROR_INVALID_QUERY
} JsonPathError;

typedef struct _JsonPath        JsonPath;
typedef struct _JsonPathClass   JsonPathClass;

JSON_AVAILABLE_IN_1_0
GType json_path_get_type (void) G_GNUC_CONST;
JSON_AVAILABLE_IN_1_0
GQuark json_path_error_quark (void);

JSON_AVAILABLE_IN_1_0
JsonPath *      json_path_new           (void);

JSON_AVAILABLE_IN_1_0
gboolean        json_path_compile       (JsonPath    *path,
                                         const char  *expression,
                                         GError     **error);
JSON_AVAILABLE_IN_1_0
JsonNode *      json_path_match         (JsonPath    *path,
                                         JsonNode    *root);

JSON_AVAILABLE_IN_1_0
JsonNode *      json_path_query         (const char  *expression,
                                         JsonNode    *root,
                                         GError     **error);

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JsonPath, g_object_unref)
#endif

G_END_DECLS
