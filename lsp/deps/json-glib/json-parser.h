/* json-parser.h - JSON streams parser
 * 
 * This file is part of JSON-GLib
 * Copyright (C) 2007  OpenedHand Ltd.
 * Copyright (C) 2009  Intel Corp.
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

#include <gio/gio.h>
#include <json-glib/json-types.h>

G_BEGIN_DECLS

#define JSON_TYPE_PARSER                (json_parser_get_type ())
#define JSON_PARSER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), JSON_TYPE_PARSER, JsonParser))
#define JSON_IS_PARSER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JSON_TYPE_PARSER))
#define JSON_PARSER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), JSON_TYPE_PARSER, JsonParserClass))
#define JSON_IS_PARSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), JSON_TYPE_PARSER))
#define JSON_PARSER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), JSON_TYPE_PARSER, JsonParserClass))

/**
 * JSON_PARSER_ERROR:
 *
 * Error domain for `JsonParser`.
 */
#define JSON_PARSER_ERROR               (json_parser_error_quark ())

/**
 * JSON_PARSER_MAX_RECURSION_DEPTH:
 *
 * The maximum recursion depth for a JSON tree.
 *
 * Since: 1.10
 */
#define JSON_PARSER_MAX_RECURSION_DEPTH (1024)

typedef struct _JsonParser              JsonParser;
typedef struct _JsonParserPrivate       JsonParserPrivate;
typedef struct _JsonParserClass         JsonParserClass;

/**
 * JsonParserError:
 * @JSON_PARSER_ERROR_PARSE: parse error
 * @JSON_PARSER_ERROR_TRAILING_COMMA: unexpected trailing comma
 * @JSON_PARSER_ERROR_MISSING_COMMA: expected comma
 * @JSON_PARSER_ERROR_MISSING_COLON: expected colon
 * @JSON_PARSER_ERROR_INVALID_BAREWORD: invalid bareword
 * @JSON_PARSER_ERROR_UNKNOWN: unknown error
 *
 * Error codes for `JSON_PARSER_ERROR`.
 *
 * This enumeration can be extended at later date
 */
typedef enum {
  JSON_PARSER_ERROR_PARSE,
  JSON_PARSER_ERROR_TRAILING_COMMA,
  JSON_PARSER_ERROR_MISSING_COMMA,
  JSON_PARSER_ERROR_MISSING_COLON,
  JSON_PARSER_ERROR_INVALID_BAREWORD,
  /**
   * JSON_PARSER_ERROR_EMPTY_MEMBER_NAME:
   *
   * Empty member name.
   *
   * Since: 0.16
   */
  JSON_PARSER_ERROR_EMPTY_MEMBER_NAME,
  /**
   * JSON_PARSER_ERROR_INVALID_DATA:
   *
   * Invalid data.
   *
   * Since: 0.18
   */
  JSON_PARSER_ERROR_INVALID_DATA,
  JSON_PARSER_ERROR_UNKNOWN,
  /**
   * JSON_PARSER_ERROR_NESTING:
   *
   * Too many levels of nesting.
   *
   * Since: 1.10
   */
  JSON_PARSER_ERROR_NESTING,
  /**
   * JSON_PARSER_ERROR_INVALID_STRUCTURE:
   *
   * Invalid structure.
   *
   * Since: 1.10
   */
  JSON_PARSER_ERROR_INVALID_STRUCTURE,
  /**
   * JSON_PARSER_ERROR_INVALID_ASSIGNMENT:
   *
   * Invalid assignment.
   *
   * Since: 1.10
   */
  JSON_PARSER_ERROR_INVALID_ASSIGNMENT
} JsonParserError;

struct _JsonParser
{
  /*< private >*/
  GObject parent_instance;

  JsonParserPrivate *priv;
};

/**
 * JsonParserClass:
 * @parse_start: class handler for the JsonParser::parse-start signal
 * @object_start: class handler for the JsonParser::object-start signal
 * @object_member: class handler for the JsonParser::object-member signal
 * @object_end: class handler for the JsonParser::object-end signal
 * @array_start: class handler for the JsonParser::array-start signal
 * @array_element: class handler for the JsonParser::array-element signal
 * @array_end: class handler for the JsonParser::array-end signal
 * @parse_end: class handler for the JsonParser::parse-end signal
 * @error: class handler for the JsonParser::error signal
 *
 * The class structure for the JsonParser type.
 */
struct _JsonParserClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public  >*/
  void (* parse_start)   (JsonParser   *parser);

  void (* object_start)  (JsonParser   *parser);
  void (* object_member) (JsonParser   *parser,
                          JsonObject   *object,
                          const gchar  *member_name);
  void (* object_end)    (JsonParser   *parser,
                          JsonObject   *object);

  void (* array_start)   (JsonParser   *parser);
  void (* array_element) (JsonParser   *parser,
                          JsonArray    *array,
                          gint          index_);
  void (* array_end)     (JsonParser   *parser,
                          JsonArray    *array);

  void (* parse_end)     (JsonParser   *parser);
  
  void (* error)         (JsonParser   *parser,
                          const GError *error);

  /*< private >*/
  /* padding for future expansion */
  void (* _json_reserved1) (void);
  void (* _json_reserved2) (void);
  void (* _json_reserved3) (void);
  void (* _json_reserved4) (void);
  void (* _json_reserved5) (void);
  void (* _json_reserved6) (void);
  void (* _json_reserved7) (void);
  void (* _json_reserved8) (void);
};

JSON_AVAILABLE_IN_1_0
GQuark json_parser_error_quark (void);
JSON_AVAILABLE_IN_1_0
GType json_parser_get_type (void) G_GNUC_CONST;

JSON_AVAILABLE_IN_1_0
JsonParser *json_parser_new                     (void);
JSON_AVAILABLE_IN_1_2
JsonParser *json_parser_new_immutable           (void);
JSON_AVAILABLE_IN_1_10
void        json_parser_set_strict              (JsonParser           *parser,
                                                 gboolean              strict);
JSON_AVAILABLE_IN_1_10
gboolean    json_parser_get_strict              (JsonParser           *parser);
JSON_AVAILABLE_IN_1_0
gboolean    json_parser_load_from_file          (JsonParser           *parser,
                                                 const gchar          *filename,
                                                 GError              **error);
JSON_AVAILABLE_IN_1_6
gboolean    json_parser_load_from_mapped_file   (JsonParser           *parser,
                                                 const gchar          *filename,
                                                 GError              **error);
JSON_AVAILABLE_IN_1_0
gboolean    json_parser_load_from_data          (JsonParser           *parser,
                                                 const gchar          *data,
                                                 gssize                length,
                                                 GError              **error);
JSON_AVAILABLE_IN_1_0
gboolean    json_parser_load_from_stream        (JsonParser           *parser,
                                                 GInputStream         *stream,
                                                 GCancellable         *cancellable,
                                                 GError              **error);
JSON_AVAILABLE_IN_1_0
void        json_parser_load_from_stream_async  (JsonParser           *parser,
                                                 GInputStream         *stream,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
JSON_AVAILABLE_IN_1_0
gboolean    json_parser_load_from_stream_finish (JsonParser           *parser,
                                                 GAsyncResult         *result,
                                                 GError              **error);

JSON_AVAILABLE_IN_1_0
JsonNode *  json_parser_get_root                (JsonParser           *parser);
JSON_AVAILABLE_IN_1_4
JsonNode *  json_parser_steal_root              (JsonParser           *parser);

JSON_AVAILABLE_IN_1_0
guint       json_parser_get_current_line        (JsonParser           *parser);
JSON_AVAILABLE_IN_1_0
guint       json_parser_get_current_pos         (JsonParser           *parser);
JSON_AVAILABLE_IN_1_0
gboolean    json_parser_has_assignment          (JsonParser           *parser,
                                                 gchar               **variable_name);

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JsonParser, g_object_unref)
#endif

G_END_DECLS
