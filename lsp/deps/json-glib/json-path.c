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

/**
 * JsonPath:
 *
 * `JsonPath` is a simple class implementing the JSONPath syntax for extracting
 * data out of a JSON tree.
 *
 * While the semantics of the JSONPath expressions are heavily borrowed by the
 * XPath specification for XML, the syntax follows the ECMAScript origins of
 * JSON.
 *
 * Once a `JsonPath` instance has been created, it has to compile a JSONPath
 * expression using [method@Json.Path.compile] before being able to match it to
 * a JSON tree; the same `JsonPath` instance can be used to match multiple JSON
 * trees. It it also possible to compile a new JSONPath expression using the
 * same `JsonPath` instance; the previous expression will be discarded only if
 * the compilation of the new expression is successful.
 *
 * The simple convenience function [func@Json.Path.query] can be used for
 * one-off matching.
 *
 * ## Syntax of the JSONPath expressions
 *
 * A JSONPath expression is composed by path indices and operators.
 * Each path index can either be a member name or an element index inside
 * a JSON tree. A JSONPath expression must start with the `$` operator; each
 * path index is separated using either the dot notation or the bracket
 * notation, e.g.:
 *
 * ```
 * // dot notation
 * $.store.book[0].title
 *
 * // bracket notation
 * $['store']['book'][0]['title']
 * ```
 *
 * The available operators are:
 *
 * * The `$` character represents the root node of the JSON tree, and
 *   matches the entire document.
 *
 * * Child nodes can either be matched using `.` or `[]`. For instance,
 *   both `$.store.book` and `$['store']['book']` match the contents of
 *   the book member of the store object.
 *
 * * Child nodes can be reached without specifying the whole tree structure
 *   through the recursive descent operator, or `..`. For instance,
 *   `$..author` matches all author member in every object.
 *
 * * Child nodes can grouped through the wildcard operator, or `*`. For
 *   instance, `$.store.book[*].author` matches all author members of any
 *   object element contained in the book array of the store object.
 *
 * * Element nodes can be accessed using their index (starting from zero)
 *   in the subscript operator `[]`. For instance, `$.store.book[0]` matches
 *   the first element of the book array of the store object.
 *
 * * Subsets of element nodes can be accessed using the set notation
 *   operator `[i,j,...]`. For instance, `$.store.book[0,2]` matches the
 *   elements 0 and 2 (the first and third) of the book array of the store
 *   object.
 *
 * * Slices of element nodes can be accessed using the slice notation
 *   operation `[start:end:step]`. If start is omitted, the starting index
 *   of the slice is implied to be zero; if end is omitted, the ending index
 *   of the slice is implied to be the length of the array; if step is
 *   omitted, the step of the slice is implied to be 1. For instance,
 *   `$.store.book[:2]` matches the first two elements of the book array
 *   of the store object.
 *
 * More information about JSONPath is available on Stefan Gössner's
 * [JSONPath website](http://goessner.net/articles/JsonPath/).
 *
 * ## Example of JSONPath matches
 *
 * The following example shows some of the results of using `JsonPath`
 * on a JSON tree. We use the following JSON description of a bookstore:
 *
 * ```json
 * { "store": {
 *     "book": [
 *       { "category": "reference", "author": "Nigel Rees",
 *         "title": "Sayings of the Century", "price": "8.95"  },
 *       { "category": "fiction", "author": "Evelyn Waugh",
 *         "title": "Sword of Honour", "price": "12.99" },
 *       { "category": "fiction", "author": "Herman Melville",
 *         "title": "Moby Dick", "isbn": "0-553-21311-3",
 *         "price": "8.99" },
 *       { "category": "fiction", "author": "J. R. R. Tolkien",
 *         "title": "The Lord of the Rings", "isbn": "0-395-19395-8",
 *         "price": "22.99" }
 *     ],
 *     "bicycle": { "color": "red", "price": "19.95" }
 *   }
 * }
 * ```
 *
 * We can parse the JSON using [class@Json.Parser]:
 *
 * ```c
 * JsonParser *parser = json_parser_new ();
 * json_parser_load_from_data (parser, json_data, -1, NULL);
 * ```
 *
 * If we run the following code:
 *
 * ```c
 * JsonNode *result;
 * JsonPath *path = json_path_new ();
 * json_path_compile (path, "$.store..author", NULL);
 * result = json_path_match (path, json_parser_get_root (parser));
 * ```
 *
 * The `result` node will contain an array with all values of the
 * author member of the objects in the JSON tree. If we use a
 * [class@Json.Generator] to convert the `result` node to a string
 * and print it:
 *
 * ```c
 * JsonGenerator *generator = json_generator_new ();
 * json_generator_set_root (generator, result);
 * char *str = json_generator_to_data (generator, NULL);
 * g_print ("Results: %s\n", str);
 * ```
 *
 * The output will be:
 *
 * ```json
 * ["Nigel Rees","Evelyn Waugh","Herman Melville","J. R. R. Tolkien"]
 * ```
 *
 * Since: 0.14
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "json-path.h"

#include "json-debug.h"
#include "json-types-private.h"

typedef enum {
  JSON_PATH_NODE_ROOT,
  JSON_PATH_NODE_CHILD_MEMBER,
  JSON_PATH_NODE_CHILD_ELEMENT,
  JSON_PATH_NODE_RECURSIVE_DESCENT,
  JSON_PATH_NODE_WILDCARD_MEMBER,
  JSON_PATH_NODE_WILDCARD_ELEMENT,
  JSON_PATH_NODE_ELEMENT_SET,
  JSON_PATH_NODE_ELEMENT_SLICE
} PathNodeType;

typedef struct _PathNode        PathNode;

struct _JsonPath
{
  GObject parent_instance;

  /* the compiled path */
  GList *nodes;

  guint is_compiled : 1;
};

struct _JsonPathClass
{
  GObjectClass parent_class;
};

struct _PathNode
{
  PathNodeType node_type;

  union {
    /* JSON_PATH_NODE_CHILD_ELEMENT */
    guint element_index;

    /* JSON_PATH_NODE_CHILD_MEMBER */
    char *member_name;

    /* JSON_PATH_NODE_ELEMENT_SET */
    struct { int n_indices; int *indices; } set;

    /* JSON_PATH_NODE_ELEMENT_SLICE */
    struct { int start, end, step; } slice;
  } data;
};

G_DEFINE_QUARK (json-path-error-quark, json_path_error)

G_DEFINE_TYPE (JsonPath, json_path, G_TYPE_OBJECT)

static void
path_node_free (gpointer data)
{
  if (data != NULL)
    {
      PathNode *node = data;

      switch (node->node_type)
        {
        case JSON_PATH_NODE_CHILD_MEMBER:
          g_free (node->data.member_name);
          break;

        case JSON_PATH_NODE_ELEMENT_SET:
          g_free (node->data.set.indices);
          break;

        default:
          break;
        }

      g_free (node);
    }
}

static void
json_path_finalize (GObject *gobject)
{
  JsonPath *self = JSON_PATH (gobject);

  g_list_free_full (self->nodes, path_node_free);

  G_OBJECT_CLASS (json_path_parent_class)->finalize (gobject);
}

static void
json_path_class_init (JsonPathClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = json_path_finalize;
}

static void
json_path_init (JsonPath *self G_GNUC_UNUSED)
{
}

/**
 * json_path_new:
 *
 * Creates a new `JsonPath` instance.
 *
 * Once created, the `JsonPath` object should be used with
 * [method@Json.Path.compile] and [method@Json.Path.match].
 *
 * Return value: (transfer full): the newly created path
 *
 * Since: 0.14
 */
JsonPath *
json_path_new (void)
{
  return g_object_new (JSON_TYPE_PATH, NULL);
}

#ifdef JSON_ENABLE_DEBUG
/* used as the function for a g_list_foreach() on a list of PathNode; needs
 * a GString as the payload to build the output string
 */
static void
json_path_foreach_print (gpointer data,
                         gpointer user_data)
{
  PathNode *cur_node = data;
  GString *buf = user_data;

  switch (cur_node->node_type)
    {
    case JSON_PATH_NODE_ROOT:
      g_string_append (buf, "<root");
      break;

    case JSON_PATH_NODE_CHILD_MEMBER:
      g_string_append_printf (buf, "<member '%s'", cur_node->data.member_name);
      break;

    case JSON_PATH_NODE_CHILD_ELEMENT:
      g_string_append_printf (buf, "<element '%u'", cur_node->data.element_index);
      break;

    case JSON_PATH_NODE_RECURSIVE_DESCENT:
      g_string_append (buf, "<recursive descent");
      break;

    case JSON_PATH_NODE_WILDCARD_MEMBER:
      g_string_append (buf, "<wildcard member");
      break;

    case JSON_PATH_NODE_WILDCARD_ELEMENT:
      g_string_append (buf, "<wildcard element");
      break;

    case JSON_PATH_NODE_ELEMENT_SET:
      {
        int i;

        g_string_append (buf, "<element set ");
        for (i = 0; i < cur_node->data.set.n_indices - 1; i++)
          g_string_append_printf (buf, "'%d', ", cur_node->data.set.indices[i]);

        g_string_append_printf (buf, "'%d'", cur_node->data.set.indices[i]);
      }
      break;

    case JSON_PATH_NODE_ELEMENT_SLICE:
      g_string_append_printf (buf, "<slice start '%d', end '%d', step '%d'",
                              cur_node->data.slice.start,
                              cur_node->data.slice.end,
                              cur_node->data.slice.step);
      break;

    default:
      g_string_append (buf, "<unknown node");
      break;
    }

  g_string_append (buf, ">");
}
#endif /* JSON_ENABLE_DEBUG */

/**
 * json_path_compile:
 * @path: a path
 * @expression: a JSONPath expression
 * @error: return location for a #GError, or %NULL
 *
 * Validates and decomposes the given expression.
 *
 * A JSONPath expression must be compiled before calling
 * [method@Json.Path.match].
 *
 * Return value: `TRUE` if the compilation was successful, and `FALSE`
 *   otherwise
 *
 * Since: 0.14
 */
gboolean
json_path_compile (JsonPath    *path,
                   const char  *expression,
                   GError     **error)
{
  const char *p, *end_p;
  PathNode *root = NULL;
  GList *nodes = NULL;

  g_return_val_if_fail (expression != NULL, FALSE);

  p = expression;

  while (*p != '\0')
    {
      switch (*p)
        {
        case '$':
          {
            PathNode *node;

            if (root != NULL)
              {
                g_set_error_literal (error, JSON_PATH_ERROR,
                                     JSON_PATH_ERROR_INVALID_QUERY,
                                     _("Only one root node is allowed in a JSONPath expression"));
                return FALSE;
              }

            if (!(*(p + 1) == '.' || *(p + 1) == '[' || *(p + 1) == '\0'))
              {
                g_set_error (error, JSON_PATH_ERROR,
                             JSON_PATH_ERROR_INVALID_QUERY,
                             /* translators: the %c is the invalid character */
                             _("Root node followed by invalid character “%c”"),
                             *(p + 1));
                return FALSE;
              }
            
            node = g_new0 (PathNode, 1);
            node->node_type = JSON_PATH_NODE_ROOT;

            root = node;
            nodes = g_list_prepend (NULL, root);
          }
          break;

        case '.':
        case '[':
          {
            PathNode *node = NULL;

            if (*p == '.' && *(p + 1) == '.')
              {
                node = g_new0 (PathNode, 1);
                node->node_type = JSON_PATH_NODE_RECURSIVE_DESCENT;
              }
            else if (*p == '.' && *(p + 1) == '*')
              {
                node = g_new0 (PathNode, 1);
                node->node_type = JSON_PATH_NODE_WILDCARD_MEMBER;

                p += 1;
              }
            else if (*p == '.')
              {
                end_p = p + 1;
                while (!(*end_p == '.' || *end_p == '[' || *end_p == '\0'))
                  end_p += 1;

                if (end_p == p + 1)
                  {
                    g_set_error_literal (error, JSON_PATH_ERROR,
                                         JSON_PATH_ERROR_INVALID_QUERY,
                                         _("Missing member name or wildcard after . character"));
                    goto fail;
                  }

                node = g_new0 (PathNode, 1);
                node->node_type = JSON_PATH_NODE_CHILD_MEMBER;
                node->data.member_name = g_strndup (p + 1, end_p - p - 1);

                p = end_p - 1;
              }
            else if (*p == '[' && *(p + 1) == '\'')
              {
                if (*(p + 2) == '*' && *(p + 3) == '\'' && *(p + 4) == ']')
                  {
                    node = g_new0 (PathNode, 1);
                    node->node_type = JSON_PATH_NODE_WILDCARD_MEMBER;

                    p += 4;
                  }
                else
                  {
                    node = g_new0 (PathNode, 1);
                    node->node_type = JSON_PATH_NODE_CHILD_MEMBER;

                    end_p = strchr (p + 2, '\'');
                    node->data.member_name = g_strndup (p + 2, end_p - p - 2);

                    p = end_p + 1;
                  }
              }
            else if (*p == '[' && *(p + 1) == '*' && *(p + 2) == ']')
              {
                node = g_new0 (PathNode, 1);
                node->node_type = JSON_PATH_NODE_WILDCARD_ELEMENT;

                p += 1;
              }
            else if (*p == '[')
              {
                int sign = 1;
                int idx;

                end_p = p + 1;

                if (*end_p == '-')
                  {
                    sign = -1;
                    end_p += 1;
                  }

                /* slice with missing start */
                if (*end_p == ':')
                  {
                    int slice_end = g_ascii_strtoll (end_p + 1, (char **) &end_p, 10) * sign;
                    int slice_step = 1;

                    if (*end_p == ':')
                      {
                        end_p += 1;

                        if (*end_p == '-')
                          {
                            sign = -1;
                            end_p += 1;
                          }
                        else
                          sign = 1;

                        slice_step = g_ascii_strtoll (end_p, (char **) &end_p, 10) * sign;

                        if (*end_p != ']')
                          {
                            g_set_error (error, JSON_PATH_ERROR,
                                         JSON_PATH_ERROR_INVALID_QUERY,
                                         _("Malformed slice expression “%*s”"),
                                         (int)(end_p - p),
                                         p + 1);
                            goto fail;
                          }
                      }

                    node = g_new0 (PathNode, 1);
                    node->node_type = JSON_PATH_NODE_ELEMENT_SLICE;
                    node->data.slice.start = 0;
                    node->data.slice.end = slice_end;
                    node->data.slice.step = slice_step;

                    nodes = g_list_prepend (nodes, node);
                    p = end_p;
                    break;
                  }

                idx = g_ascii_strtoll (end_p, (char **) &end_p, 10) * sign;

                if (*end_p == ',')
                  {
                    GArray *indices = g_array_new (FALSE, TRUE, sizeof (int));

                    g_array_append_val (indices, idx);

                    while (*end_p != ']')
                      {
                        end_p += 1;

                        if (*end_p == '-')
                          {
                            sign = -1;
                            end_p += 1;
                          }
                        else
                          sign = 1;

                        idx = g_ascii_strtoll (end_p, (char **) &end_p, 10) * sign;
                        if (!(*end_p == ',' || *end_p == ']'))
                          {
                            g_array_unref (indices);
                            g_set_error (error, JSON_PATH_ERROR,
                                         JSON_PATH_ERROR_INVALID_QUERY,
                                         _("Invalid set definition “%*s”"),
                                         (int)(end_p - p),
                                         p + 1);
                            goto fail;
                          }

                        g_array_append_val (indices, idx);
                      }

                    node = g_new0 (PathNode, 1);
                    node->node_type = JSON_PATH_NODE_ELEMENT_SET;
                    node->data.set.n_indices =  indices->len;
                    node->data.set.indices = (int *) g_array_free (indices, FALSE);
                    nodes = g_list_prepend (nodes, node);
                    p = end_p;
                    break;
                  }
                else if (*end_p == ':')
                  {
                    int slice_start = idx;
                    int slice_end = 0;
                    int slice_step = 1;

                    end_p += 1;

                    if (*end_p == '-')
                      {
                        sign = -1;
                        end_p += 1;
                      }
                    else
                      sign = 1;

                    slice_end = g_ascii_strtoll (end_p, (char **) &end_p, 10) * sign;
                    if (*end_p == ':')
                      {
                        end_p += 1;

                        if (*end_p == '-')
                          {
                            sign = -1;
                            end_p += 1;
                          }
                        else
                          sign = 1;

                        slice_step = g_ascii_strtoll (end_p + 1, (char **) &end_p, 10) * sign;
                      }

                    if (*end_p != ']')
                      {
                        g_set_error (error, JSON_PATH_ERROR,
                                     JSON_PATH_ERROR_INVALID_QUERY,
                                     _("Invalid slice definition “%*s”"),
                                     (int)(end_p - p),
                                     p + 1);
                        goto fail;
                      }

                    node = g_new0 (PathNode, 1);
                    node->node_type = JSON_PATH_NODE_ELEMENT_SLICE;
                    node->data.slice.start = slice_start;
                    node->data.slice.end = slice_end;
                    node->data.slice.step = slice_step;
                    nodes = g_list_prepend (nodes, node);
                    p = end_p;
                    break;
                  }
                else if (*end_p == ']')
                  {
                    node = g_new0 (PathNode, 1);
                    node->node_type = JSON_PATH_NODE_CHILD_ELEMENT;
                    node->data.element_index = idx;
                    nodes = g_list_prepend (nodes, node);
                    p = end_p;
                    break;
                  }
                else
                  {
                    g_set_error (error, JSON_PATH_ERROR,
                                 JSON_PATH_ERROR_INVALID_QUERY,
                                 _("Invalid array index definition “%*s”"),
                                 (int)(end_p - p),
                                 p + 1);
                    goto fail;
                  }
              }
            else
              break;

            if (node != NULL)
              nodes = g_list_prepend (nodes, node);
          }
          break;

        default:
          if (nodes == NULL)
            {
              g_set_error(error, JSON_PATH_ERROR,
                          JSON_PATH_ERROR_INVALID_QUERY,
                          _("Invalid first character “%c”"),
                          *p);
              return FALSE;
            }
          break;
        }

      p += 1;
    }

  nodes = g_list_reverse (nodes);

#ifdef JSON_ENABLE_DEBUG
  if (JSON_HAS_DEBUG (PATH))
    {
      GString *buf = g_string_new (NULL);

      g_list_foreach (nodes, json_path_foreach_print, buf);

      g_message ("[PATH] " G_STRLOC ": expression '%s' => '%s'", expression, buf->str);
      g_string_free (buf, TRUE);
    }
#endif /* JSON_ENABLE_DEBUG */

  g_list_free_full (path->nodes, path_node_free);

  path->nodes = nodes;
  path->is_compiled = (path->nodes != NULL);

  return path->nodes != NULL;

fail:
  g_list_free_full (nodes, path_node_free);

  return FALSE;
}

static void
walk_path_node (GList      *path,
                JsonNode   *root,
                JsonArray  *results)
{
  PathNode *node = path->data;

  switch (node->node_type)
    {
    case JSON_PATH_NODE_ROOT:
      if (path->next != NULL)
          walk_path_node (path->next, root, results);
      else
          json_array_add_element (results, json_node_copy (root));
      break;

    case JSON_PATH_NODE_CHILD_MEMBER:
      if (JSON_NODE_HOLDS_OBJECT (root))
        {
          JsonObject *object = json_node_get_object (root);

          if (json_object_has_member (object, node->data.member_name))
            {
              JsonNode *member = json_object_get_member (object, node->data.member_name);
              
              if (path->next == NULL)
                {
                  JSON_NOTE (PATH, "end of path at member '%s'", node->data.member_name);
                  json_array_add_element (results, json_node_copy (member));
                }
              else
                walk_path_node (path->next, member, results);
            }
        }
      break;

    case JSON_PATH_NODE_CHILD_ELEMENT:
      if (JSON_NODE_HOLDS_ARRAY (root))
        {
          JsonArray *array = json_node_get_array (root);

          if (json_array_get_length (array) >= node->data.element_index)
            {
              JsonNode *element = json_array_get_element (array, node->data.element_index);

              if (path->next == NULL)
                {
                  JSON_NOTE (PATH, "end of path at element '%d'", node->data.element_index);
                  json_array_add_element (results, json_node_copy (element));
                }
              else
                walk_path_node (path->next, element, results);
            }
        }
      break;

    case JSON_PATH_NODE_RECURSIVE_DESCENT:
      {
        PathNode *tmp = path->next->data;

        switch (json_node_get_node_type (root))
          {
          case JSON_NODE_OBJECT:
            {
              JsonObject *object = json_node_get_object (root);
              GQueue *members = json_object_get_members_internal (object);
              GList *l;

              for (l = members->head; l != NULL; l = l->next)
                {
                  JsonNode *m = json_object_get_member (object, l->data);

                  if (tmp->node_type == JSON_PATH_NODE_CHILD_MEMBER &&
                      strcmp (tmp->data.member_name, l->data) == 0)
                    {
                      JSON_NOTE (PATH, "entering '%s'", tmp->data.member_name);
                      walk_path_node (path->next, root, results);
                    }
                  else
                    {
                      JSON_NOTE (PATH, "recursing into '%s'", (char *) l->data);
                      walk_path_node (path, m, results);
                    }
                }
            }
            break;

          case JSON_NODE_ARRAY:
            {
              JsonArray *array = json_node_get_array (root);
              GList *members, *l;
              guint i;

              members = json_array_get_elements (array);
              for (l = members, i = 0; l != NULL; l = l->next, i += 1)
                {
                  JsonNode *m = l->data;

                  if (tmp->node_type == JSON_PATH_NODE_CHILD_ELEMENT &&
                      tmp->data.element_index == i)
                    {
                      JSON_NOTE (PATH, "entering '%d'", tmp->data.element_index);
                      walk_path_node (path->next, root, results);
                    }
                  else
                    {
                      JSON_NOTE (PATH, "recursing into '%d'", i);
                      walk_path_node (path, m, results);
                    }
                }
              g_list_free (members);
            }
            break;

          default:
            break;
          }
      }
      break;

    case JSON_PATH_NODE_WILDCARD_MEMBER:
      if (JSON_NODE_HOLDS_OBJECT (root))
        {
          JsonObject *object = json_node_get_object (root);
          GQueue *members = json_object_get_members_internal (object);
          GList *l;

          for (l = members->head; l != NULL; l = l->next)
            {
              JsonNode *member = json_object_get_member (object, l->data);

              if (path->next != NULL)
                walk_path_node (path->next, member, results);
              else
                {
                  JSON_NOTE (PATH, "glob match member '%s'", (char *) l->data);
                  json_array_add_element (results, json_node_copy (member));
                }
            }
        }
      else
        json_array_add_element (results, json_node_copy (root));
      break;

    case JSON_PATH_NODE_WILDCARD_ELEMENT:
      if (JSON_NODE_HOLDS_ARRAY (root))
        {
          JsonArray *array = json_node_get_array (root);
          GList *elements, *l;
          int i;

          elements = json_array_get_elements (array);
          for (l = elements, i = 0; l != NULL; l = l->next, i += 1)
            {
              JsonNode *element = l->data;

              if (path->next != NULL)
                walk_path_node (path->next, element, results);
              else
                {
                  JSON_NOTE (PATH, "glob match element '%d'", i);
                  json_array_add_element (results, json_node_copy (element));
                }
            }
          g_list_free (elements);
        }
      else
        json_array_add_element (results, json_node_copy (root));
      break;

    case JSON_PATH_NODE_ELEMENT_SET:
      if (JSON_NODE_HOLDS_ARRAY (root))
        {
          JsonArray *array = json_node_get_array (root);
          int i;

          for (i = 0; i < node->data.set.n_indices; i += 1)
            {
              int idx = node->data.set.indices[i];
              JsonNode *element = json_array_get_element (array, idx);

              if (path->next != NULL)
                walk_path_node (path->next, element, results);
              else
                {
                  JSON_NOTE (PATH, "set element '%d'", idx);
                  json_array_add_element (results, json_node_copy (element));
                }
            }
        }
      break;

    case JSON_PATH_NODE_ELEMENT_SLICE:
      if (JSON_NODE_HOLDS_ARRAY (root))
        {
          JsonArray *array = json_node_get_array (root);
          int i, start, end;

          if (node->data.slice.start < 0)
            {
              start = json_array_get_length (array)
                    + node->data.slice.start;

              end = json_array_get_length (array)
                  + node->data.slice.end;
            }
          else
            {
              start = node->data.slice.start;
              end = node->data.slice.end;
            }

          for (i = start; i < end; i += node->data.slice.step)
            {
              JsonNode *element = json_array_get_element (array, i);

              if (path->next != NULL)
                walk_path_node (path->next, element, results);
              else
                {
                  JSON_NOTE (PATH, "slice element '%d'", i);
                  json_array_add_element (results, json_node_copy (element));
                }
            }
        }
      break;

    default:
      break;
    }
}

/**
 * json_path_match:
 * @path: a compiled path
 * @root: the root node of the JSON data to match
 *
 * Matches the JSON tree pointed by `root` using the expression compiled
 * into the `JsonPath`.
 *
 * The nodes matching the expression will be copied into an array.
 *
 * Return value: (transfer full): a newly-created node of type
 *   `JSON_NODE_ARRAY` containing the array of matching nodes
 *
 * Since: 0.14
 */
JsonNode *
json_path_match (JsonPath *path,
                 JsonNode *root)
{
  JsonArray *results;
  JsonNode *retval;

  g_return_val_if_fail (JSON_IS_PATH (path), NULL);
  g_return_val_if_fail (path->is_compiled, NULL);
  g_return_val_if_fail (root != NULL, NULL);

  results = json_array_new ();

  walk_path_node (path->nodes, root, results);

  retval = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (retval, results);

  return retval;
}

/**
 * json_path_query:
 * @expression: a JSONPath expression
 * @root: the root of a JSON tree
 * @error: return location for a #GError, or %NULL
 *
 * Queries a JSON tree using a JSONPath expression.
 *
 * This function is a simple wrapper around [ctor@Json.Path.new],
 * [method@Json.Path.compile], and [method@Json.Path.match]. It implicitly
 * creates a `JsonPath` instance, compiles the given expression and matches
 * it against the JSON tree pointed by `root`.
 *
 * Return value: (transfer full): a newly-created node of type
 *   `JSON_NODE_ARRAY` containing the array of matching nodes
 *
 * Since: 0.14
 */
JsonNode *
json_path_query (const char  *expression,
                 JsonNode    *root,
                 GError     **error)
{
  JsonPath *path = json_path_new ();
  JsonNode *retval;

  if (!json_path_compile (path, expression, error))
    {
      g_object_unref (path);
      return NULL;
    }

  retval = json_path_match (path, root);

  g_object_unref (path);

  return retval;
}
