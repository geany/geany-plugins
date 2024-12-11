/* json-parser.c - JSON streams parser
 * 
 * This file is part of JSON-GLib
 *
 * Copyright © 2007, 2008, 2009 OpenedHand Ltd
 * Copyright © 2009, 2010 Intel Corp.
 * Copyright © 2015 Collabora Ltd.
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
 *   Philip Withnall  <philip.withnall@collabora.co.uk>
 */

/**
 * JsonParser:
 *
 * `JsonParser` provides an object for parsing a JSON data stream, either
 * inside a file or inside a static buffer.
 *
 * ## Using `JsonParser`
 *
 * The `JsonParser` API is fairly simple:
 *
 * ```c
 * gboolean
 * parse_json (const char *filename)
 * {
 *   g_autoptr(JsonParser) parser = json_parser_new ();
 *   g_autoptr(GError) error = NULL
 *
 *   json_parser_load_from_file (parser, filename, &error);
 *   if (error != NULL)
 *     {
 *       g_critical ("Unable to parse '%s': %s", filename, error->message);
 *       return FALSE;
 *     }
 *
 *   g_autoptr(JsonNode) root = json_parser_get_root (parser);
 *
 *   // manipulate the object tree from the root node
 *
 *   return TRUE
 * }
 * ```
 *
 * By default, the entire process of loading the data and parsing it is
 * synchronous; the [method@Json.Parser.load_from_stream_async] API will
 * load the data asynchronously, but parse it in the main context as the
 * signals of the parser must be emitted in the same thread. If you do
 * not use signals, and you wish to also parse the JSON data without blocking,
 * you should use a `GTask` and the synchronous `JsonParser` API inside the
 * task itself.
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "json-types-private.h"

#include "json-debug.h"
#include "json-parser.h"
#include "json-scanner.h"

struct _JsonParserPrivate
{
  JsonNode *root;
  JsonNode *current_node;

  JsonScanner *scanner;

  JsonParserError error_code;
  GError *last_error;

  gchar *variable_name;
  gchar *filename;

  guint has_assignment : 1;
  guint is_filename    : 1;
  guint is_immutable   : 1;
  guint is_strict      : 1;
};

enum
{
  PARSE_START,
  OBJECT_START,
  OBJECT_MEMBER,
  OBJECT_END,
  ARRAY_START,
  ARRAY_ELEMENT,
  ARRAY_END,
  PARSE_END,
  ERROR,

  LAST_SIGNAL
};

static guint parser_signals[LAST_SIGNAL] = { 0, };

enum
{
  PROP_IMMUTABLE = 1,
  PROP_STRICT,
  PROP_LAST
};

static GParamSpec *parser_props[PROP_LAST] = { NULL, };

G_DEFINE_QUARK (json-parser-error-quark, json_parser_error)

G_DEFINE_TYPE_WITH_PRIVATE (JsonParser, json_parser, G_TYPE_OBJECT)

static guint json_parse_array  (JsonParser    *parser,
                                JsonScanner   *scanner,
                                JsonNode     **node,
                                unsigned int   nesting);
static guint json_parse_object (JsonParser    *parser,
                                JsonScanner   *scanner,
                                JsonNode     **node,
                                unsigned int   nesting);

static inline void
json_parser_clear (JsonParser *parser)
{
  JsonParserPrivate *priv = parser->priv;

  g_clear_pointer (&priv->variable_name, g_free);
  g_clear_pointer (&priv->last_error, g_error_free);
  g_clear_pointer (&priv->root, json_node_unref);
}

static void
json_parser_dispose (GObject *gobject)
{
  json_parser_clear (JSON_PARSER (gobject));

  G_OBJECT_CLASS (json_parser_parent_class)->dispose (gobject);
}

static void
json_parser_finalize (GObject *gobject)
{
  JsonParserPrivate *priv = JSON_PARSER (gobject)->priv;

  g_free (priv->variable_name);
  g_free (priv->filename);

  G_OBJECT_CLASS (json_parser_parent_class)->finalize (gobject);
}

static void
json_parser_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  JsonParserPrivate *priv = JSON_PARSER (gobject)->priv;

  switch (prop_id)
    {
    case PROP_IMMUTABLE:
      /* Construct-only. */
      priv->is_immutable = g_value_get_boolean (value);
      break;

    case PROP_STRICT:
      json_parser_set_strict (JSON_PARSER (gobject),
                              g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
json_parser_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  JsonParserPrivate *priv = JSON_PARSER (gobject)->priv;

  switch (prop_id)
    {
    case PROP_IMMUTABLE:
      g_value_set_boolean (value, priv->is_immutable);
      break;

    case PROP_STRICT:
      g_value_set_boolean (value, priv->is_strict);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
json_parser_class_init (JsonParserClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = json_parser_set_property;
  gobject_class->get_property = json_parser_get_property;
  gobject_class->dispose = json_parser_dispose;
  gobject_class->finalize = json_parser_finalize;

  /**
   * JsonParser:immutable:
   *
   * Whether the tree built by the parser should be immutable
   * when created.
   *
   * Making the output immutable on creation avoids the expense
   * of traversing it to make it immutable later.
   *
   * Since: 1.2
   */
  parser_props[PROP_IMMUTABLE] =
    g_param_spec_boolean ("immutable", NULL, NULL,
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  /**
   * JsonParser:strict:
   *
   * Whether the parser should be strictly conforming to the
   * JSON format, or allow custom extensions like comments.
   *
   * Since: 1.10
   */
  parser_props[PROP_STRICT] =
    g_param_spec_boolean ("strict", NULL, NULL, FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, PROP_LAST, parser_props);

  /**
   * JsonParser::parse-start:
   * @parser: the parser that emitted the signal
   * 
   * This signal is emitted when a parser starts parsing a JSON data stream.
   */
  parser_signals[PARSE_START] =
    g_signal_new ("parse-start",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, parse_start),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  /**
   * JsonParser::parse-end:
   * @parser: the parser that emitted the signal
   *
   * This signal is emitted when a parser successfully finished parsing a
   * JSON data stream
   */
  parser_signals[PARSE_END] =
    g_signal_new ("parse-end",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, parse_end),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  /**
   * JsonParser::object-start:
   * @parser: the parser that emitted the signal
   *
   * This signal is emitted each time a parser starts parsing a JSON object.
   */
  parser_signals[OBJECT_START] =
    g_signal_new ("object-start",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, object_start),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  /**
   * JsonParser::object-member:
   * @parser: the parser that emitted the signal
   * @object: the JSON object being parsed
   * @member_name: the name of the newly parsed member
   *
   * The `::object-member` signal is emitted each time a parser
   * has successfully parsed a single member of a JSON object
   */
  parser_signals[OBJECT_MEMBER] =
    g_signal_new ("object-member",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, object_member),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  JSON_TYPE_OBJECT,
                  G_TYPE_STRING);
  /**
   * JsonParser::object-end:
   * @parser: the parser that emitted the signal
   * @object: the parsed JSON object
   *
   * The `::object-end` signal is emitted each time a parser
   * has successfully parsed an entire JSON object.
   */
  parser_signals[OBJECT_END] =
    g_signal_new ("object-end",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, object_end),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  JSON_TYPE_OBJECT);
  /**
   * JsonParser::array-start:
   * @parser: the parser that emitted the signal
   *
   * The `::array-start` signal is emitted each time a parser
   * starts parsing a JSON array.
   */
  parser_signals[ARRAY_START] =
    g_signal_new ("array-start",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, array_start),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  /**
   * JsonParser::array-element:
   * @parser: the parser that emitted the signal
   * @array: a JSON array
   * @index_: the index of the newly parsed array element
   *
   * The `::array-element` signal is emitted each time a parser
   * has successfully parsed a single element of a JSON array.
   */
  parser_signals[ARRAY_ELEMENT] =
    g_signal_new ("array-element",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, array_element),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  JSON_TYPE_ARRAY,
                  G_TYPE_INT);
  /**
   * JsonParser::array-end:
   * @parser: the parser that emitted the signal
   * @array: the parsed JSON array
   *
   * The `::array-end` signal is emitted each time a parser
   * has successfully parsed an entire JSON array.
   */
  parser_signals[ARRAY_END] =
    g_signal_new ("array-end",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, array_end),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  JSON_TYPE_ARRAY);
  /**
   * JsonParser::error:
   * @parser: the parser that emitted the signal
   * @error: the error
   *
   * The `::error` signal is emitted each time a parser encounters
   * an error in a JSON stream.
   */
  parser_signals[ERROR] =
    g_signal_new ("error",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, error),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);
}

static void
json_parser_init (JsonParser *parser)
{
  JsonParserPrivate *priv = json_parser_get_instance_private (parser);

  parser->priv = priv;

  priv->root = NULL;
  priv->current_node = NULL;

  priv->error_code = JSON_PARSER_ERROR_PARSE;
  priv->last_error = NULL;

  priv->has_assignment = FALSE;
  priv->variable_name = NULL;

  priv->is_filename = FALSE;
  priv->filename = FALSE;
}

static guint
json_parse_value (JsonParser   *parser,
                  JsonScanner  *scanner,
                  guint         token,
                  JsonNode    **node)
{
  JsonParserPrivate *priv = parser->priv;
  JsonNode *current_node = priv->current_node;

  switch (token)
    {
    case JSON_TOKEN_INT:
      {
        gint64 value = json_scanner_get_int64_value (scanner);

        JSON_NOTE (PARSER, "node: %" G_GINT64_FORMAT, value);
        *node = json_node_init_int (json_node_alloc (), value);
      }
      break;

    case JSON_TOKEN_FLOAT:
      {
        double value = json_scanner_get_float_value (scanner);

        JSON_NOTE (PARSER, "abs(node): %.6f", value);
        *node = json_node_init_double (json_node_alloc (), value);
      }
      break;

    case JSON_TOKEN_STRING:
      {
        const char *value = json_scanner_get_string_value (scanner);

        JSON_NOTE (PARSER, "node: '%s'", value);
        *node = json_node_init_string (json_node_alloc (), value);
      }
      break;

    case JSON_TOKEN_TRUE:
    case JSON_TOKEN_FALSE:
      JSON_NOTE (PARSER, "node: '%s'",
                 JSON_TOKEN_TRUE ? "<true>" : "<false>");
      *node = json_node_init_boolean (json_node_alloc (), token == JSON_TOKEN_TRUE ? TRUE : FALSE);
      break;

    case JSON_TOKEN_NULL:
      JSON_NOTE (PARSER, "node: <null>");
      *node = json_node_init_null (json_node_alloc ());
      break;

    case JSON_TOKEN_IDENTIFIER:
      JSON_NOTE (PARSER, "node: identifier '%s'", json_scanner_get_identifier (scanner));
      priv->error_code = JSON_PARSER_ERROR_INVALID_BAREWORD;
      *node = NULL;
      return JSON_TOKEN_SYMBOL;

    default:
      {
        JsonNodeType cur_type;

        *node = NULL;

        JSON_NOTE (PARSER, "node: invalid token");

        cur_type = json_node_get_node_type (current_node);
        if (cur_type == JSON_NODE_ARRAY)
          {
            priv->error_code = JSON_PARSER_ERROR_PARSE;
            return JSON_TOKEN_RIGHT_BRACE;
          }
        else if (cur_type == JSON_NODE_OBJECT)
          {
            priv->error_code = JSON_PARSER_ERROR_PARSE;
            return JSON_TOKEN_RIGHT_CURLY;
          }
        else
          {
            priv->error_code = JSON_PARSER_ERROR_INVALID_BAREWORD;
            return JSON_TOKEN_SYMBOL;
          }
      }
      break;
    }

  if (priv->is_immutable && *node != NULL)
    json_node_seal (*node);

  return JSON_TOKEN_NONE;
}

static guint
json_parse_array (JsonParser    *parser,
                  JsonScanner   *scanner,
                  JsonNode     **node,
                  unsigned int   nesting_level)
{
  JsonParserPrivate *priv = parser->priv;
  JsonNode *old_current;
  JsonArray *array;
  guint token;
  gint idx;

  if (nesting_level >= JSON_PARSER_MAX_RECURSION_DEPTH)
    {
      priv->error_code = JSON_PARSER_ERROR_NESTING;
      return JSON_TOKEN_RIGHT_BRACE;
    }

  old_current = priv->current_node;
  priv->current_node = json_node_init_array (json_node_alloc (), NULL);

  array = json_array_new ();

  token = json_scanner_get_next_token (scanner);
  g_assert (token == JSON_TOKEN_LEFT_BRACE);

  g_signal_emit (parser, parser_signals[ARRAY_START], 0);

  idx = 0;
  while (token != JSON_TOKEN_RIGHT_BRACE)
    {
      guint next_token = json_scanner_peek_next_token (scanner);
      JsonNode *element = NULL;

      /* parse the element */
      switch (next_token)
        {
        case JSON_TOKEN_LEFT_BRACE:
          JSON_NOTE (PARSER, "Nested array at index %d", idx);
          token = json_parse_array (parser, scanner, &element, nesting_level + 1);
          break;

        case JSON_TOKEN_LEFT_CURLY:
          JSON_NOTE (PARSER, "Nested object at index %d", idx);
          token = json_parse_object (parser, scanner, &element, nesting_level + 1);
          break;

        case JSON_TOKEN_RIGHT_BRACE:
          goto array_done;

        default:
          token = json_scanner_get_next_token (scanner);
          token = json_parse_value (parser, scanner, token, &element);
          break;
        }

      if (token != JSON_TOKEN_NONE || element == NULL)
        {
          /* the json_parse_* functions will have set the error code */
          json_array_unref (array);
          json_node_unref (priv->current_node);
          priv->current_node = old_current;

          return token;
        }

      next_token = json_scanner_peek_next_token (scanner);

      /* look for missing commas */
      if (next_token != JSON_TOKEN_COMMA && next_token != JSON_TOKEN_RIGHT_BRACE)
        {
          priv->error_code = JSON_PARSER_ERROR_MISSING_COMMA;

          json_array_unref (array);
          json_node_free (priv->current_node);
          json_node_free (element);
          priv->current_node = old_current;

          return JSON_TOKEN_COMMA;
        }

      /* look for trailing commas */
      if (next_token == JSON_TOKEN_COMMA)
        {
          token = json_scanner_get_next_token (scanner);
          next_token = json_scanner_peek_next_token (scanner);

          if (next_token == JSON_TOKEN_RIGHT_BRACE)
            {
              priv->error_code = JSON_PARSER_ERROR_TRAILING_COMMA;

              json_array_unref (array);
              json_node_unref (priv->current_node);
              json_node_unref (element);
              priv->current_node = old_current;

              return JSON_TOKEN_RIGHT_BRACE;
            }
        }

      JSON_NOTE (PARSER, "Array element %d completed", idx);
      json_node_set_parent (element, priv->current_node);
      if (priv->is_immutable)
        json_node_seal (element);
      json_array_add_element (array, element);

      g_signal_emit (parser, parser_signals[ARRAY_ELEMENT], 0,
                     array,
                     idx);

      idx += 1;
      token = next_token;
    }

array_done:
  json_scanner_get_next_token (scanner);

  if (priv->is_immutable)
    json_array_seal (array);

  json_node_take_array (priv->current_node, array);
  if (priv->is_immutable)
    json_node_seal (priv->current_node);
  json_node_set_parent (priv->current_node, old_current);

  g_signal_emit (parser, parser_signals[ARRAY_END], 0, array);

  if (node != NULL && *node == NULL)
    *node = priv->current_node;

  priv->current_node = old_current;

  return JSON_TOKEN_NONE;
}

static guint
json_parse_object (JsonParser    *parser,
                   JsonScanner   *scanner,
                   JsonNode     **node,
                   unsigned int   nesting)
{
  JsonParserPrivate *priv = parser->priv;
  JsonObject *object;
  JsonNode *old_current;
  guint token;

  if (nesting >= JSON_PARSER_MAX_RECURSION_DEPTH)
    {
      priv->error_code = JSON_PARSER_ERROR_NESTING;
      return JSON_TOKEN_RIGHT_CURLY;
    }

  old_current = priv->current_node;
  priv->current_node = json_node_init_object (json_node_alloc (), NULL);

  object = json_object_new ();

  token = json_scanner_get_next_token (scanner);
  g_assert (token == JSON_TOKEN_LEFT_CURLY);

  g_signal_emit (parser, parser_signals[OBJECT_START], 0);

  while (token != JSON_TOKEN_RIGHT_CURLY)
    {
      guint next_token = json_scanner_peek_next_token (scanner);
      JsonNode *member = NULL;
      gchar *name;

      /* we need to abort here because empty objects do not
       * have member names
       */
      if (next_token == JSON_TOKEN_RIGHT_CURLY)
        break;

      /* parse the member's name */
      if (next_token != JSON_TOKEN_STRING)
        {
          JSON_NOTE (PARSER, "Missing object member name");

          priv->error_code = JSON_PARSER_ERROR_INVALID_BAREWORD;

          json_object_unref (object);
          json_node_unref (priv->current_node);
          priv->current_node = old_current;

          return JSON_TOKEN_STRING;
        }

      /* member name */
      token = json_scanner_get_next_token (scanner);
      name = json_scanner_dup_string_value (scanner);
      if (name == NULL)
        {
          JSON_NOTE (PARSER, "Empty object member name");

          priv->error_code = JSON_PARSER_ERROR_EMPTY_MEMBER_NAME;

          json_object_unref (object);
          json_node_unref (priv->current_node);
          priv->current_node = old_current;

          return JSON_TOKEN_STRING;
        }

      JSON_NOTE (PARSER, "Object member '%s'", name);

      /* a colon separates names from values */
      next_token = json_scanner_peek_next_token (scanner);
      if (next_token != JSON_TOKEN_COLON)
        {
          JSON_NOTE (PARSER, "Missing object member name separator");

          priv->error_code = JSON_PARSER_ERROR_MISSING_COLON;

          g_free (name);
          json_object_unref (object);
          json_node_unref (priv->current_node);
          priv->current_node = old_current;

          return JSON_TOKEN_COLON;
        }

      /* we swallow the ':' */
      token = json_scanner_get_next_token (scanner);
      g_assert (token == JSON_TOKEN_COLON);
      next_token = json_scanner_peek_next_token (scanner);

      /* parse the member's value */
      switch (next_token)
        {
        case JSON_TOKEN_LEFT_BRACE:
          JSON_NOTE (PARSER, "Nested array at member %s", name);
          token = json_parse_array (parser, scanner, &member, nesting + 1);
          break;

        case JSON_TOKEN_LEFT_CURLY:
          JSON_NOTE (PARSER, "Nested object at member %s", name);
          token = json_parse_object (parser, scanner, &member, nesting + 1);
          break;

        default:
          /* once a member name is defined we need a value */
          token = json_scanner_get_next_token (scanner);
          token = json_parse_value (parser, scanner, token, &member);
          break;
        }

      if (token != JSON_TOKEN_NONE || member == NULL)
        {
          /* the json_parse_* functions will have set the error code */
          g_free (name);
          json_object_unref (object);
          json_node_unref (priv->current_node);
          priv->current_node = old_current;

          return token;
        }

      next_token = json_scanner_peek_next_token (scanner);
      if (next_token == JSON_TOKEN_COMMA)
        {
          token = json_scanner_get_next_token (scanner);
          next_token = json_scanner_peek_next_token (scanner);

          /* look for trailing commas */
          if (next_token == JSON_TOKEN_RIGHT_CURLY)
            {
              priv->error_code = JSON_PARSER_ERROR_TRAILING_COMMA;

              g_free (name);
              json_object_unref (object);
              json_node_unref (member);
              json_node_unref (priv->current_node);
              priv->current_node = old_current;

              return JSON_TOKEN_RIGHT_BRACE;
            }
        }
      else if (next_token == JSON_TOKEN_STRING)
        {
          priv->error_code = JSON_PARSER_ERROR_MISSING_COMMA;

          g_free (name);
          json_object_unref (object);
          json_node_unref (member);
          json_node_unref (priv->current_node);
          priv->current_node = old_current;

          return JSON_TOKEN_COMMA;
        }

      JSON_NOTE (PARSER, "Object member '%s' completed", name);
      json_node_set_parent (member, priv->current_node);
      if (priv->is_immutable)
        json_node_seal (member);
      json_object_set_member (object, name, member);

      g_signal_emit (parser, parser_signals[OBJECT_MEMBER], 0,
                     object,
                     name);

      g_free (name);

      token = next_token;
    }

  json_scanner_get_next_token (scanner);

  if (priv->is_immutable)
    json_object_seal (object);

  json_node_take_object (priv->current_node, object);
  if (priv->is_immutable)
    json_node_seal (priv->current_node);
  json_node_set_parent (priv->current_node, old_current);

  g_signal_emit (parser, parser_signals[OBJECT_END], 0, object);

  if (node != NULL && *node == NULL)
    *node = priv->current_node;

  priv->current_node = old_current;

  return JSON_TOKEN_NONE;
}

static guint
json_parse_statement (JsonParser  *parser,
                      JsonScanner *scanner)
{
  JsonParserPrivate *priv = parser->priv;
  guint token;

  token = json_scanner_peek_next_token (scanner);
  switch (token)
    {
    case JSON_TOKEN_LEFT_CURLY:
      if (priv->is_strict && priv->root != NULL)
        {
          JSON_NOTE (PARSER, "Only one top level object is possible");
          json_scanner_get_next_token (scanner);
          priv->error_code = JSON_PARSER_ERROR_INVALID_STRUCTURE;
          return JSON_TOKEN_EOF;
        }
      JSON_NOTE (PARSER, "Statement is object declaration");
      return json_parse_object (parser, scanner, &priv->root, 0);

    case JSON_TOKEN_LEFT_BRACE:
      if (priv->is_strict && priv->root != NULL)
        {
          JSON_NOTE (PARSER, "Only one top level array is possible");
          json_scanner_get_next_token (scanner);
          priv->error_code = JSON_PARSER_ERROR_INVALID_STRUCTURE;
          return JSON_TOKEN_EOF;
        }
      JSON_NOTE (PARSER, "Statement is array declaration");
      return json_parse_array (parser, scanner, &priv->root, 0);

    /* some web APIs are not only passing the data structures: they are
     * also passing an assigment, which makes parsing horribly complicated
     * only because web developers are lazy, and writing "var foo = " is
     * evidently too much to request from them.
     */
    case JSON_TOKEN_VAR:
      {
        guint next_token;
        gchar *name;

        JSON_NOTE (PARSER, "Statement is an assignment");

        if (priv->is_strict)
          {
            json_scanner_get_next_token (scanner);
            priv->error_code = JSON_PARSER_ERROR_INVALID_ASSIGNMENT;
            return JSON_TOKEN_EOF;
          }

        /* swallow the 'var' token... */
        token = json_scanner_get_next_token (scanner);

        /* ... swallow the variable name... */
        next_token = json_scanner_get_next_token (scanner);
        if (next_token != JSON_TOKEN_IDENTIFIER)
          {
            priv->error_code = JSON_PARSER_ERROR_INVALID_BAREWORD;
            return JSON_TOKEN_IDENTIFIER;
          }

        name = json_scanner_dup_identifier (scanner);

        /* ... and finally swallow the '=' */
        next_token = json_scanner_get_next_token (scanner);
        if (next_token != '=')
          {
            priv->error_code = JSON_PARSER_ERROR_INVALID_BAREWORD;
            g_free (name);
            return '=';
          }

        if (priv->has_assignment)
          g_free (priv->variable_name);
        priv->has_assignment = TRUE;
        priv->variable_name = name;

        token = json_parse_statement (parser, scanner);

        /* remove the trailing semi-colon */
        next_token = json_scanner_peek_next_token (scanner);
        if (next_token == ';')
          {
            token = json_scanner_get_next_token (scanner);
            return JSON_TOKEN_NONE;
          }

        return token;
      }
      break;

    case JSON_TOKEN_NULL:
    case JSON_TOKEN_TRUE:
    case JSON_TOKEN_FALSE:
    case '-':
    case JSON_TOKEN_INT:
    case JSON_TOKEN_FLOAT:
    case JSON_TOKEN_STRING:
    case JSON_TOKEN_IDENTIFIER:
      if (priv->root != NULL)
        {
          JSON_NOTE (PARSER, "Only one top level statement is possible");
          json_scanner_get_next_token (scanner);
          priv->error_code = JSON_PARSER_ERROR_INVALID_BAREWORD;
          return JSON_TOKEN_EOF;
        }

      JSON_NOTE (PARSER, "Statement is a value");
      token = json_scanner_get_next_token (scanner);
      return json_parse_value (parser, scanner, token, &priv->root);

    default:
      JSON_NOTE (PARSER, "Unknown statement");
      json_scanner_get_next_token (scanner);
      priv->error_code = JSON_PARSER_ERROR_INVALID_BAREWORD;
      return priv->root != NULL ? JSON_TOKEN_EOF : JSON_TOKEN_SYMBOL;
    }
}

static void
json_scanner_msg_handler (JsonScanner *scanner,
                          const char  *message,
                          gpointer     user_data)
{
  JsonParser *parser = user_data;
  JsonParserPrivate *priv = parser->priv;
  GError *error = NULL;

  g_set_error (&error, JSON_PARSER_ERROR,
               priv->error_code,
               /* translators: %s: is the file name, the first %d is the line
                * number, the second %d is the position on the line, and %s is
                * the error message
                */
               _("%s:%d:%d: Parse error: %s"),
               priv->is_filename ? priv->filename : "<data>",
               json_scanner_get_current_line (scanner),
               json_scanner_get_current_position (scanner),
               message);
      
  parser->priv->last_error = error;
  g_signal_emit (parser, parser_signals[ERROR], 0, error);
}

static JsonScanner *
json_scanner_create (JsonParser *parser)
{
  JsonParserPrivate *priv = json_parser_get_instance_private (parser);
  JsonScanner *scanner;

  scanner = json_scanner_new (priv->is_strict);
  json_scanner_set_msg_handler (scanner, json_scanner_msg_handler, parser);

  return scanner;
}

/**
 * json_parser_new:
 * 
 * Creates a new JSON parser.
 *
 * You can use the `JsonParser` to load a JSON stream from either a file or a
 * buffer and then walk the hierarchy using the data types API.
 *
 * Returns: (transfer full): the newly created parser
 */
JsonParser *
json_parser_new (void)
{
  return g_object_new (JSON_TYPE_PARSER, NULL);
}

/**
 * json_parser_new_immutable:
 *
 * Creates a new parser instance with its [property@Json.Parser:immutable]
 * property set to `TRUE` to create immutable output trees.
 *
 * Since: 1.2
 * Returns: (transfer full): the newly created parser 
 */
JsonParser *
json_parser_new_immutable (void)
{
  return g_object_new (JSON_TYPE_PARSER, "immutable", TRUE, NULL);
}

static gboolean
json_parser_load (JsonParser   *parser,
                  const gchar  *input_data,
                  gsize         length,
                  GError      **error)
{
  JsonParserPrivate *priv = parser->priv;
  JsonScanner *scanner;
  gboolean done;
  gboolean retval = TRUE;
  const char *data = input_data;

  if (priv->is_strict && (length == 0 || data == NULL || *data == '\0'))
    {
      g_set_error_literal (error, JSON_PARSER_ERROR,
                           JSON_PARSER_ERROR_INVALID_DATA,
                           "JSON data must not be empty");
      g_signal_emit (parser, parser_signals[ERROR], 0, *error);
      return FALSE;
    }

  json_parser_clear (parser);

  if (!g_utf8_validate (data, length, NULL))
    {
      g_set_error_literal (error, JSON_PARSER_ERROR,
                           JSON_PARSER_ERROR_INVALID_DATA,
                           _("JSON data must be UTF-8 encoded"));
      g_signal_emit (parser, parser_signals[ERROR], 0, *error);
      return FALSE;
    }

  if (length >= 3)
    {
      /* Check for UTF-8 signature and skip it if necessary */
       if (((data[0] & 0xFF) == 0xEF) &&
           ((data[1] & 0xFF) == 0xBB) &&
           ((data[2] & 0xFF) == 0xBF))
         {
           JSON_NOTE (PARSER, "Skipping BOM");
           data += 3;
           length -= 3;
         }

       if (priv->is_strict && length == 0)
         {
           g_set_error_literal (error, JSON_PARSER_ERROR,
                                JSON_PARSER_ERROR_INVALID_DATA,
                                "JSON data must not be empty after BOM character");
           g_signal_emit (parser, parser_signals[ERROR], 0, *error);
           return FALSE;
         }
    }

  /* Skip leading space */
  if (priv->is_strict)
    {
      const char *p = data;
      while (length > 0 && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        {
          length -= 1;
          data += 1;
          p = data;
        }

      if (length == 0)
        {
          g_set_error_literal (error, JSON_PARSER_ERROR,
                               JSON_PARSER_ERROR_INVALID_DATA,
                               "JSON data must not be empty after leading whitespace");
          g_signal_emit (parser, parser_signals[ERROR], 0, *error);
          return FALSE;
        }
    }

  scanner = json_scanner_create (parser);
  json_scanner_input_text (scanner, data, length);

  priv->scanner = scanner;

  g_signal_emit (parser, parser_signals[PARSE_START], 0);

  done = FALSE;
  while (!done)
    {
      if (json_scanner_peek_next_token (scanner) == JSON_TOKEN_EOF)
        done = TRUE;
      else
        {
          unsigned int expected_token;

          /* we try to show the expected token, if possible */
          expected_token = json_parse_statement (parser, scanner);
          if (expected_token != JSON_TOKEN_NONE)
            {
              /* this will emit the ::error signal via the custom
               * message handler we install
               */
              json_scanner_unknown_token (scanner, expected_token);

              /* and this will propagate the error we create in the
               * same message handler
               */
              if (priv->last_error)
                {
                  g_propagate_error (error, priv->last_error);
                  priv->last_error = NULL;
                }

              retval = FALSE;
              done = TRUE;
            }
        }
    }

  g_signal_emit (parser, parser_signals[PARSE_END], 0);

  /* remove the scanner */
  json_scanner_destroy (scanner);
  priv->scanner = NULL;
  priv->current_node = NULL;

  return retval;
}

/**
 * json_parser_load_from_file:
 * @parser: a parser
 * @filename: (type filename): the path for the file to parse
 * @error: return location for a #GError
 *
 * Loads a JSON stream from the content of `filename` and parses it.
 *
 * If the file is large or shared between processes,
 * [method@Json.Parser.load_from_mapped_file] may be a more efficient
 * way to load it.
 *
 * See also: [method@Json.Parser.load_from_data]
 *
 * Returns: `TRUE` if the file was successfully loaded and parsed.
 */
gboolean
json_parser_load_from_file (JsonParser   *parser,
                            const gchar  *filename,
                            GError      **error)
{
  JsonParserPrivate *priv;
  GError *internal_error;
  gchar *data;
  gsize length;
  gboolean retval = TRUE;

  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  priv = parser->priv;

  internal_error = NULL;
  if (!g_file_get_contents (filename, &data, &length, &internal_error))
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  g_free (priv->filename);

  priv->is_filename = TRUE;
  priv->filename = g_strdup (filename);

  if (!json_parser_load (parser, data, length, &internal_error))
    {
      g_propagate_error (error, internal_error);
      retval = FALSE;
    }

  g_free (data);

  return retval;
}

/**
 * json_parser_load_from_mapped_file:
 * @parser: a parser
 * @filename: (type filename): the path for the file to parse
 * @error: return location for a #GError
 *
 * Loads a JSON stream from the content of `filename` and parses it.
 *
 * Unlike [method@Json.Parser.load_from_file], `filename` will be memory
 * mapped as read-only and parsed. `filename` will be unmapped before this
 * function returns.
 *
 * If mapping or reading the file fails, a `G_FILE_ERROR` will be returned.
 *
 * Returns: `TRUE` if the file was successfully loaded and parsed.
 * Since: 1.6
 */
gboolean
json_parser_load_from_mapped_file (JsonParser   *parser,
                                   const gchar  *filename,
                                   GError      **error)
{
  JsonParserPrivate *priv;
  GError *internal_error = NULL;
  gboolean retval = TRUE;
  GMappedFile *mapped_file = NULL;

  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  priv = parser->priv;

  mapped_file = g_mapped_file_new (filename, FALSE, &internal_error);
  if (mapped_file == NULL)
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  g_free (priv->filename);

  priv->is_filename = TRUE;
  priv->filename = g_strdup (filename);

  if (!json_parser_load (parser, g_mapped_file_get_contents (mapped_file),
                         g_mapped_file_get_length (mapped_file), &internal_error))
    {
      g_propagate_error (error, internal_error);
      retval = FALSE;
    }

  g_clear_pointer (&mapped_file, g_mapped_file_unref);

  return retval;
}

/**
 * json_parser_load_from_data:
 * @parser: a parser
 * @data: the buffer to parse
 * @length: the length of the buffer, or -1 if it is `NUL` terminated
 * @error: return location for a #GError
 *
 * Loads a JSON stream from a buffer and parses it.
 *
 * You can call this function multiple times with the same parser, but the
 * contents of the parser will be destroyed each time.
 *
 * Returns: `TRUE` if the buffer was succesfully parsed
 */
gboolean
json_parser_load_from_data (JsonParser   *parser,
                            const gchar  *data,
                            gssize        length,
                            GError      **error)
{
  JsonParserPrivate *priv;
  GError *internal_error;
  gboolean retval = TRUE;

  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  priv = parser->priv;

  if (length < 0)
    length = strlen (data);

  priv->is_filename = FALSE;
  g_free (priv->filename);
  priv->filename = NULL;

  internal_error = NULL;
  if (!json_parser_load (parser, data, length, &internal_error))
    {
      g_propagate_error (error, internal_error);
      retval = FALSE;
    }

  return retval;
}

/**
 * json_parser_get_root:
 * @parser: a parser
 *
 * Retrieves the top level node from the parsed JSON stream.
 *
 * If the parser input was an empty string, or if parsing failed, the root
 * will be `NULL`. It will also be `NULL` if it has been stolen using
 * [method@Json.Parser.steal_root].
 *
 * Returns: (transfer none) (nullable): the root node.
 */
JsonNode *
json_parser_get_root (JsonParser *parser)
{
  g_return_val_if_fail (JSON_IS_PARSER (parser), NULL);

  /* Sanity check. */
  g_assert (parser->priv->root == NULL ||
            !parser->priv->is_immutable ||
            json_node_is_immutable (parser->priv->root));

  return parser->priv->root;
}

/**
 * json_parser_steal_root:
 * @parser: a parser
 *
 * Steals the top level node from the parsed JSON stream.
 *
 * This will be `NULL` in the same situations as [method@Json.Parser.get_root]
 * return `NULL`.
 *
 * Returns: (transfer full) (nullable): the root node
 *
 * Since: 1.4
 */
JsonNode *
json_parser_steal_root (JsonParser *parser)
{
  JsonParserPrivate *priv = json_parser_get_instance_private (parser);

  g_return_val_if_fail (JSON_IS_PARSER (parser), NULL);

  /* Sanity check. */
  g_assert (parser->priv->root == NULL ||
            !parser->priv->is_immutable ||
            json_node_is_immutable (parser->priv->root));

  return g_steal_pointer (&priv->root);
}

/**
 * json_parser_get_current_line:
 * @parser: a parser
 *
 * Retrieves the line currently parsed, starting from 1.
 *
 * This function has defined behaviour only while parsing; calling this
 * function from outside the signal handlers emitted by the parser will
 * yield 0.
 *
 * Returns: the currently parsed line, or 0.
 */
guint
json_parser_get_current_line (JsonParser *parser)
{
  g_return_val_if_fail (JSON_IS_PARSER (parser), 0);

  if (parser->priv->scanner != NULL)
    return json_scanner_get_current_line (parser->priv->scanner);

  return 0;
}

/**
 * json_parser_get_current_pos:
 * @parser: a parser
 *
 * Retrieves the current position inside the current line, starting
 * from 0.
 *
 * This function has defined behaviour only while parsing; calling this
 * function from outside the signal handlers emitted by the parser will
 * yield 0.
 *
 * Returns: the position in the current line, or 0.
 */
guint
json_parser_get_current_pos (JsonParser *parser)
{
  g_return_val_if_fail (JSON_IS_PARSER (parser), 0);

  if (parser->priv->scanner != NULL)
    return json_scanner_get_current_position (parser->priv->scanner);

  return 0;
}

/**
 * json_parser_has_assignment:
 * @parser: a parser
 * @variable_name: (out) (optional) (transfer none): the variable name
 *
 * A JSON data stream might sometimes contain an assignment, like:
 *
 * ```
 * var _json_data = { "member_name" : [ ...
 * ```
 *
 * even though it would technically constitute a violation of the RFC.
 *
 * `JsonParser` will ignore the left hand identifier and parse the right
 * hand value of the assignment. `JsonParser` will record, though, the
 * existence of the assignment in the data stream and the variable name
 * used.
 *
 * Returns: `TRUE` if there was an assignment, and `FALSE` otherwise
 *
 * Since: 0.4
 */
gboolean
json_parser_has_assignment (JsonParser  *parser,
                            gchar      **variable_name)
{
  JsonParserPrivate *priv;

  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);

  priv = parser->priv;

  if (priv->has_assignment && variable_name)
    *variable_name = priv->variable_name;

  return priv->has_assignment;
}

#define GET_DATA_BLOCK_SIZE     8192

/**
 * json_parser_load_from_stream:
 * @parser: a parser
 * @stream: the input stream with the JSON data
 * @cancellable: (nullable): a #GCancellable
 * @error: the return location for a #GError
 *
 * Loads the contents of an input stream and parses them.
 *
 * If `cancellable` is not `NULL`, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the
 * operation was cancelled, `G_IO_ERROR_CANCELLED` will be set
 * on the given `error`.
 *
 * Returns: `TRUE` if the data stream was successfully read and
 *   parsed, and `FALSE` otherwise
 *
 * Since: 0.12
 */
gboolean
json_parser_load_from_stream (JsonParser    *parser,
                              GInputStream  *stream,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GByteArray *content;
  gsize pos;
  gssize res;
  gboolean retval = FALSE;
  GError *internal_error;

  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);
  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  content = g_byte_array_new ();
  pos = 0;

  g_byte_array_set_size (content, pos + GET_DATA_BLOCK_SIZE + 1);
  while ((res = g_input_stream_read (stream, content->data + pos,
                                     GET_DATA_BLOCK_SIZE,
                                     cancellable, error)) > 0)
    {
      pos += res;
      g_byte_array_set_size (content, pos + GET_DATA_BLOCK_SIZE + 1);
    }

  if (res < 0)
    {
      /* error has already been set */
      retval = FALSE;
      goto out;
    }

  /* zero-terminate the content; we allocated an extra byte for this */
  content->data[pos] = 0;

  internal_error = NULL;
  retval = json_parser_load (parser, (const gchar *) content->data, pos, &internal_error);

  if (internal_error != NULL)
    g_propagate_error (error, internal_error);

out:
  g_byte_array_free (content, TRUE);

  return retval;
}

typedef struct {
  GInputStream *stream;
  GByteArray *content;
  gsize pos;
} LoadData;

static void
load_data_free (gpointer data_)
{
  if (data_ != NULL)
    {
      LoadData *data = data_;

      g_object_unref (data->stream);
      g_byte_array_unref (data->content);
      g_free (data);
    }
}

/**
 * json_parser_load_from_stream_finish:
 * @parser: a parser
 * @result: the result of the asynchronous operation
 * @error: the return location for a #GError
 *
 * Finishes an asynchronous stream loading started with
 * [method@Json.Parser.load_from_stream_async].
 *
 * Returns: `TRUE` if the content of the stream was successfully retrieved
 *   and parsed, and `FALSE` otherwise
 *
 * Since: 0.12
 */
gboolean
json_parser_load_from_stream_finish (JsonParser    *parser,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  gboolean res;

  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, parser), FALSE);

  res = g_task_propagate_boolean (G_TASK (result), error);
  if (res)
    {
      LoadData *data = g_task_get_task_data (G_TASK (result));
      GError *internal_error = NULL;

      /* We need to do this inside the finish() function because JsonParser will emit
       * signals, and we need to ensure that the signals are emitted in the right
       * context; it's easier to do that if we just rely on the async callback being
       * called in the right context, even if it means making the finish() function
       * necessary to complete the async operation.
       */
      res = json_parser_load (parser, (const gchar *) data->content->data, data->pos, &internal_error);
      if (internal_error != NULL)
        g_propagate_error (error, internal_error);
    }

  return res;
}

static void
read_from_stream (GTask *task,
                  gpointer source_obj G_GNUC_UNUSED,
                  gpointer task_data,
                  GCancellable *cancellable)
{
  LoadData *data = task_data;
  GError *error = NULL;
  gssize res;

  data->pos = 0;
  g_byte_array_set_size (data->content, data->pos + GET_DATA_BLOCK_SIZE + 1);
  while ((res = g_input_stream_read (data->stream,
                                     data->content->data + data->pos,
                                     GET_DATA_BLOCK_SIZE,
                                     cancellable, &error)) > 0)
    {
      data->pos += res;
      g_byte_array_set_size (data->content, data->pos + GET_DATA_BLOCK_SIZE + 1);
    }

  if (res < 0)
    {
      g_task_return_error (task, error);
      return;
    }

  /* zero-terminate the content; we allocated an extra byte for this */
  data->content->data[data->pos] = 0;
  g_task_return_boolean (task, TRUE);
}

/**
 * json_parser_load_from_stream_async:
 * @parser: a parser
 * @stream: the input stream with the JSON data
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): the function to call when the request is satisfied
 * @user_data: the data to pass to @callback
 *
 * Asynchronously reads the contents of a stream.
 *
 * For more details, see [method@Json.Parser.load_from_stream], which is the
 * synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You should
 * then call [method@Json.Parser.load_from_stream_finish] to get the result
 * of the operation.
 *
 * Since: 0.12
 */
void
json_parser_load_from_stream_async (JsonParser          *parser,
                                    GInputStream        *stream,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  LoadData *data;
  GTask *task;

  g_return_if_fail (JSON_IS_PARSER (parser));
  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data = g_new (LoadData, 1);
  data->stream = g_object_ref (stream);
  data->content = g_byte_array_new ();
  data->pos = 0;

  task = g_task_new (parser, cancellable, callback, user_data);
  g_task_set_task_data (task, data, load_data_free);

  g_task_run_in_thread (task, read_from_stream);
  g_object_unref (task);
}

/**
 * json_parser_set_strict:
 * @parser: the JSON parser
 * @strict: whether the parser should be strict
 *
 * Sets whether the parser should operate in strict mode.
 *
 * If @strict is true, `JsonParser` will strictly conform to
 * the JSON format.
 *
 * If @strict is false, `JsonParser` will allow custom extensions
 * to the JSON format, like comments.
 *
 * Since: 1.10
 */
void
json_parser_set_strict (JsonParser *parser,
                        gboolean    strict)
{
  g_return_if_fail (JSON_IS_PARSER (parser));

  JsonParserPrivate *priv = json_parser_get_instance_private (parser);

  strict = !!strict;

  if (priv->is_strict != strict)
    {
      priv->is_strict = strict;
      g_object_notify_by_pspec (G_OBJECT (parser), parser_props[PROP_STRICT]);
    }
}

/**
 * json_parser_get_strict:
 * @parser: the JSON parser
 *
 * Retrieves whether the parser is operating in strict mode.
 *
 * Returns: true if the parser is strict, and false otherwise
 *
 * Since: 1.10
 */
gboolean
json_parser_get_strict (JsonParser *parser)
{
  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);

  JsonParserPrivate *priv = json_parser_get_instance_private (parser);

  return priv->is_strict;
}
