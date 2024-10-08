/* json-node.c - JSON object model node
 * 
 * This file is part of JSON-GLib
 * Copyright (C) 2007  OpenedHand Ltd.
 * Copyright (C) 2009  Intel Corp.
 * Copyright (C) 2015  Collabora Ltd.
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

#include "config.h"

#include <glib.h>

#include "json-types.h"
#include "json-types-private.h"
#include "json-debug.h"

/**
 * JsonNode:
 *
 * A generic container of JSON data types.
 *
 * `JsonNode` can contain fundamental types (integers, booleans, floating point
 * numbers, strings) and complex types (arrays and objects).
 *
 * When parsing a JSON data stream you extract the root node and walk
 * the node tree by retrieving the type of data contained inside the
 * node with the `JSON_NODE_TYPE` macro. If the node contains a fundamental
 * type you can retrieve a copy of the `GValue` holding it with the
 * [method@Json.Node.get_value] function, and then use the `GValue` API to extract
 * the data; if the node contains a complex type you can retrieve the
 * [struct@Json.Object] or the [struct@Json.Array] using [method@Json.Node.get_object]
 * or [method@Json.Node.get_array] respectively, and then retrieve the nodes
 * they contain.
 *
 * A `JsonNode` may be marked as immutable using [method@Json.Node.seal]. This
 * marks the node and all its descendents as read-only, and means that
 * subsequent calls to setter functions (such as [method@Json.Node.set_array])
 * on them will abort as a programmer error. By marking a node tree as
 * immutable, it may be referenced in multiple places and its hash value cached
 * for fast lookups, without the possibility of a value deep within the tree
 * changing and affecting hash values. Immutable nodes may be passed to
 * functions which retain a reference to them without needing to take a copy.
 *
 * A `JsonNode` supports two types of memory management: `malloc`/`free`
 * semantics, and reference counting semantics. The two may be mixed to a
 * limited extent: nodes may be allocated (which gives them a reference count
 * of 1), referenced one or more times, unreferenced exactly that number of
 * times (using [method@Json.Node.unref]), then either unreferenced exactly
 * once more or freed (using [method@Json.Node.free]) to destroy them.
 * The [method@Json.Node.free] function must not be used when a node might
 * have a reference count not equal to 1. To this end, JSON-GLib uses
 * [method@Json.Node.copy] and [method@Json.Node.unref] internally.
 */

G_DEFINE_BOXED_TYPE (JsonNode, json_node, json_node_copy, json_node_unref)

/**
 * json_node_get_value_type:
 * @node: the node to check
 *
 * Returns the `GType` of the payload of the node.
 *
 * For `JSON_NODE_NULL` nodes, the returned type is `G_TYPE_INVALID`.
 *
 * Return value: the type for the payload
 *
 * Since: 0.4
 */
GType
json_node_get_value_type (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, G_TYPE_INVALID);

  switch (node->type)
    {
    case JSON_NODE_OBJECT:
      return JSON_TYPE_OBJECT;

    case JSON_NODE_ARRAY:
      return JSON_TYPE_ARRAY;

    case JSON_NODE_NULL:
      return G_TYPE_INVALID;

    case JSON_NODE_VALUE:
      if (node->data.value)
        return JSON_VALUE_TYPE (node->data.value);
      else
        return G_TYPE_INVALID;

    default:
      g_assert_not_reached ();
      return G_TYPE_INVALID;
    }
}

/**
 * json_node_alloc: (constructor)
 *
 * Allocates a new, uninitialized node.
 *
 * Use [method@Json.Node.init] and its variants to initialize the returned value.
 *
 * Return value: (transfer full): the newly allocated node
 *
 * Since: 0.16
 */
JsonNode *
json_node_alloc (void)
{
  JsonNode *node = NULL;

  node = g_slice_new0 (JsonNode);
  node->ref_count = 1;
  node->allocated = TRUE;

  return node;
}

static void
json_node_unset (JsonNode *node)
{
  /* Note: Don't use JSON_NODE_IS_VALID here because this may legitimately be
   * called with (node->ref_count == 0) from json_node_unref(). */
  g_assert (node != NULL);

  switch (node->type)
    {
    case JSON_NODE_OBJECT:
      g_clear_pointer (&(node->data.object), json_object_unref);
      break;

    case JSON_NODE_ARRAY:
      g_clear_pointer (&(node->data.array), json_array_unref);
      break;

    case JSON_NODE_VALUE:
      g_clear_pointer (&(node->data.value), json_value_unref);
      break;

    case JSON_NODE_NULL:
      break;
    }
}

/**
 * json_node_init:
 * @node: the node to initialize
 * @type: the type of JSON node to initialize @node to
 *
 * Initializes a @node to a specific @type.
 *
 * If the node has already been initialized once, it will be reset to
 * the given type, and any data contained will be cleared.
 *
 * Return value: (transfer none): the initialized node
 *
 * Since: 0.16
 */
JsonNode *
json_node_init (JsonNode *node,
                JsonNodeType type)
{
  g_return_val_if_fail (type >= JSON_NODE_OBJECT &&
                        type <= JSON_NODE_NULL, NULL);
  g_return_val_if_fail (node->ref_count == 1, NULL);

  json_node_unset (node);

  node->type = type;

  return node;
}

/**
 * json_node_init_object:
 * @node: the node to initialize
 * @object: (nullable): the JSON object to initialize @node with, or `NULL`
 *
 * Initializes @node to `JSON_NODE_OBJECT` and sets @object into it.
 *
 * This function will take a reference on @object.
 *
 * If the node has already been initialized once, it will be reset to
 * the given type, and any data contained will be cleared.
 *
 * Return value: (transfer none): the initialized node
 *
 * Since: 0.16
 */
JsonNode *
json_node_init_object (JsonNode   *node,
                       JsonObject *object)
{
  g_return_val_if_fail (node != NULL, NULL);
  
  json_node_init (node, JSON_NODE_OBJECT);
  json_node_set_object (node, object);

  return node;
}

/**
 * json_node_init_array:
 * @node: the node to initialize
 * @array: (nullable): the JSON array to initialize @node with, or `NULL`
 *
 * Initializes @node to `JSON_NODE_ARRAY` and sets @array into it.
 *
 * This function will take a reference on @array.
 *
 * If the node has already been initialized once, it will be reset to
 * the given type, and any data contained will be cleared.
 *
 * Return value: (transfer none): the initialized node
 *
 * Since: 0.16
 */
JsonNode *
json_node_init_array (JsonNode  *node,
                      JsonArray *array)
{
  g_return_val_if_fail (node != NULL, NULL);

  json_node_init (node, JSON_NODE_ARRAY);
  json_node_set_array (node, array);

  return node;
}

/**
 * json_node_init_int:
 * @node: the node to initialize
 * @value: an integer
 *
 * Initializes @node to `JSON_NODE_VALUE` and sets @value into it.
 *
 * If the node has already been initialized once, it will be reset to
 * the given type, and any data contained will be cleared.
 *
 * Return value: (transfer none): the initialized node
 *
 * Since: 0.16
 */
JsonNode *
json_node_init_int (JsonNode *node,
                    gint64    value)
{
  g_return_val_if_fail (node != NULL, NULL);

  json_node_init (node, JSON_NODE_VALUE);
  json_node_set_int (node, value);

  return node;
}

/**
 * json_node_init_double:
 * @node: the node to initialize
 * @value: a floating point value
 *
 * Initializes @node to `JSON_NODE_VALUE` and sets @value into it.
 *
 * If the node has already been initialized once, it will be reset to
 * the given type, and any data contained will be cleared.
 *
 * Return value: (transfer none): the initialized node
 *
 * Since: 0.16
 */
JsonNode *
json_node_init_double (JsonNode *node,
                       gdouble   value)
{
  g_return_val_if_fail (node != NULL, NULL);

  json_node_init (node, JSON_NODE_VALUE);
  json_node_set_double (node, value);

  return node;
}

/**
 * json_node_init_boolean:
 * @node: the node to initialize
 * @value: a boolean value
 *
 * Initializes @node to `JSON_NODE_VALUE` and sets @value into it.
 *
 * If the node has already been initialized once, it will be reset to
 * the given type, and any data contained will be cleared.
 *
 * Return value: (transfer none): the initialized node
 *
 * Since: 0.16
 */
JsonNode *
json_node_init_boolean (JsonNode *node,
                        gboolean  value)
{
  g_return_val_if_fail (node != NULL, NULL);

  json_node_init (node, JSON_NODE_VALUE);
  json_node_set_boolean (node, value);

  return node;
}

/**
 * json_node_init_string:
 * @node: the node to initialize
 * @value: (nullable): a string value
 *
 * Initializes @node to `JSON_NODE_VALUE` and sets @value into it.
 *
 * If the node has already been initialized once, it will be reset to
 * the given type, and any data contained will be cleared.
 *
 * Return value: (transfer none): the initialized node
 *
 * Since: 0.16
 */
JsonNode *
json_node_init_string (JsonNode   *node,
                       const char *value)
{
  g_return_val_if_fail (node != NULL, NULL);

  json_node_init (node, JSON_NODE_VALUE);
  json_node_set_string (node, value);

  return node;
}

/**
 * json_node_init_null:
 * @node: the node to initialize
 *
 * Initializes @node to `JSON_NODE_NULL`.
 *
 * If the node has already been initialized once, it will be reset to
 * the given type, and any data contained will be cleared.
 *
 * Return value: (transfer none): the initialized node
 *
 * Since: 0.16
 */
JsonNode *
json_node_init_null (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);

  return json_node_init (node, JSON_NODE_NULL);
}

/**
 * json_node_new: (constructor)
 * @type: the type of the node to create 
 *
 * Creates a new node holding the given @type.
 *
 * This is a convenience function for [ctor@Json.Node.alloc] and
 * [method@Json.Node.init], and it's the equivalent of:
 *
 * ```c
   json_node_init (json_node_alloc (), type);
 * ```
 *
 * Return value: (transfer full): the newly created node
 */
JsonNode *
json_node_new (JsonNodeType type)
{
  g_return_val_if_fail (type >= JSON_NODE_OBJECT &&
                        type <= JSON_NODE_NULL, NULL);

  return json_node_init (json_node_alloc (), type);
}

/**
 * json_node_copy:
 * @node: the node to copy 
 *
 * Copies @node.
 *
 * If the node contains complex data types, their reference
 * counts are increased, regardless of whether the node is mutable or
 * immutable.
 *
 * The copy will be immutable if, and only if, @node is immutable. However,
 * there should be no need to copy an immutable node.
 *
 * Return value: (transfer full): the copied of the given node
 */
JsonNode *
json_node_copy (JsonNode *node)
{
  JsonNode *copy;

  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);

  copy = json_node_alloc ();
  copy->type = node->type;
  copy->immutable = node->immutable;

#ifdef JSON_ENABLE_DEBUG
  if (node->immutable)
    {
      JSON_NOTE (NODE, "Copying immutable JsonNode %p of type %s",
                 node,
                 json_node_type_name (node));
    }
#endif

  switch (copy->type)
    {
    case JSON_NODE_OBJECT:
      copy->data.object = json_node_dup_object (node);
      break;

    case JSON_NODE_ARRAY:
      copy->data.array = json_node_dup_array (node);
      break;

    case JSON_NODE_VALUE:
      if (node->data.value)
        copy->data.value = json_value_ref (node->data.value);
      break;

    case JSON_NODE_NULL:
      break;

    default:
      g_assert_not_reached ();
    }

  return copy;
}

/**
 * json_node_ref:
 * @node: the node to reference 
 *
 * Increments the reference count of @node.
 *
 * Since: 1.2
 * Returns: (transfer full): a pointer to @node
 */
JsonNode *
json_node_ref (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);

  g_atomic_int_inc (&node->ref_count);

  return node;
}

/**
 * json_node_unref:
 * @node: (transfer full): the node to unreference
 *
 * Decrements the reference count of @node.
 *
 * If the reference count reaches zero, the node is freed.
 *
 * Since: 1.2
 */
void
json_node_unref (JsonNode *node)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));

  if (g_atomic_int_dec_and_test (&node->ref_count))
    {
      json_node_unset (node);
      if (node->allocated)
        g_slice_free (JsonNode, node);
    }
}

/**
 * json_node_set_object:
 * @node: a node initialized to `JSON_NODE_OBJECT`
 * @object: (nullable): a JSON object
 *
 * Sets @objects inside @node.
 *
 * The reference count of @object is increased.
 *
 * If @object is `NULL`, the node’s existing object is cleared.
 *
 * It is an error to call this on an immutable node, or on a node which is not
 * an object node.
 */
void
json_node_set_object (JsonNode   *node,
                      JsonObject *object)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT);
  g_return_if_fail (!node->immutable);

  if (node->data.object != NULL)
    json_object_unref (node->data.object);

  if (object)
    node->data.object = json_object_ref (object);
  else
    node->data.object = NULL;
}

/**
 * json_node_take_object:
 * @node: a node initialized to `JSON_NODE_OBJECT`
 * @object: (transfer full): a JSON object
 *
 * Sets @object inside @node.
 *
 * The reference count of @object is not increased.
 *
 * It is an error to call this on an immutable node, or on a node which is not
 * an object node.
 */
void
json_node_take_object (JsonNode   *node,
                       JsonObject *object)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT);
  g_return_if_fail (!node->immutable);

  if (node->data.object)
    {
      json_object_unref (node->data.object);
      node->data.object = NULL;
    }

  if (object)
    node->data.object = object;
}

/**
 * json_node_get_object:
 * @node: a node holding a JSON object
 *
 * Retrieves the object stored inside a node.
 *
 * It is a programmer error to call this on a node which doesn’t hold an
 * object value. Use `JSON_NODE_HOLDS_OBJECT` first.
 *
 * Return value: (transfer none) (nullable): the JSON object
 */
JsonObject *
json_node_get_object (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT, NULL);

  return node->data.object;
}

/**
 * json_node_dup_object:
 * @node: a node holding a JSON object
 *
 * Retrieves the object inside @node.
 *
 * The reference count of the returned object is increased.
 *
 * It is a programmer error to call this on a node which doesn’t hold an
 * object value. Use `JSON_NODE_HOLDS_OBJECT` first.
 *
 * Return value: (transfer full) (nullable): the JSON object
 */
JsonObject *
json_node_dup_object (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT, NULL);

  if (node->data.object)
    return json_object_ref (node->data.object);
  
  return NULL;
}

/**
 * json_node_set_array:
 * @node: a node initialized to `JSON_NODE_ARRAY`
 * @array: a JSON array
 *
 * Sets @array inside @node.
 *
 * The reference count of @array is increased.
 *
 * It is a programmer error to call this on a node which doesn’t hold an
 * array value. Use `JSON_NODE_HOLDS_ARRAY` first.
 */
void
json_node_set_array (JsonNode  *node,
                     JsonArray *array)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY);
  g_return_if_fail (!node->immutable);

  if (node->data.array)
    json_array_unref (node->data.array);

  if (array)
    node->data.array = json_array_ref (array);
  else
    node->data.array = NULL;
}

/**
 * json_node_take_array:
 * @node: a node initialized to `JSON_NODE_ARRAY`
 * @array: (transfer full): a JSON array
 *
 * Sets @array inside @node.
 *
 * The reference count of @array is not increased.
 *
 * It is a programmer error to call this on a node which doesn’t hold an
 * array value. Use `JSON_NODE_HOLDS_ARRAY` first.
 */
void
json_node_take_array (JsonNode  *node,
                      JsonArray *array)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY);
  g_return_if_fail (!node->immutable);

  if (node->data.array)
    {
      json_array_unref (node->data.array);
      node->data.array = NULL;
    }

  if (array)
    node->data.array = array;
}

/**
 * json_node_get_array:
 * @node: a node holding an array
 *
 * Retrieves the JSON array stored inside a node.
 *
 * It is a programmer error to call this on a node which doesn’t hold an
 * array value. Use `JSON_NODE_HOLDS_ARRAY` first.
 *
 * Return value: (transfer none) (nullable): the JSON array
 */
JsonArray *
json_node_get_array (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY, NULL);

  return node->data.array;
}

/**
 * json_node_dup_array:
 * @node: a node holding an array
 *
 * Retrieves the JSON array inside @node.
 *
 * The reference count of the returned array is increased.
 *
 * It is a programmer error to call this on a node which doesn’t hold an
 * array value. Use `JSON_NODE_HOLDS_ARRAY` first.
 *
 * Return value: (transfer full) (nullable): the JSON array with its reference
 *   count increased.
 */
JsonArray *
json_node_dup_array (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY, NULL);

  if (node->data.array)
    return json_array_ref (node->data.array);

  return NULL;
}

/**
 * json_node_get_value:
 * @node: a node
 * @value: (out caller-allocates): return location for an uninitialized value
 *
 * Retrieves a value from a node and copies into @value.
 *
 * When done using it, call `g_value_unset()` on the `GValue` to free the
 * associated resources.
 *
 * It is a programmer error to call this on a node which doesn’t hold a scalar
 * value. Use `JSON_NODE_HOLDS_VALUE` first.
 */
void
json_node_get_value (JsonNode *node,
                     GValue   *value)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);

  if (node->data.value)
    {
      g_value_init (value, JSON_VALUE_TYPE (node->data.value));
      switch (JSON_VALUE_TYPE (node->data.value))
        {
        case G_TYPE_INT64:
          g_value_set_int64 (value, json_value_get_int (node->data.value));
          break;

        case G_TYPE_DOUBLE:
          g_value_set_double (value, json_value_get_double (node->data.value));
          break;

        case G_TYPE_BOOLEAN:
          g_value_set_boolean (value, json_value_get_boolean (node->data.value));
          break;

        case G_TYPE_STRING:
          g_value_set_string (value, json_value_get_string (node->data.value));
          break;

        default:
          break;
        }
    }
}

/**
 * json_node_set_value:
 * @node: a node initialized to `JSON_NODE_VALUE`
 * @value: the value to set
 *
 * Sets a scalar value inside the given node.
 *
 * The contents of the given `GValue` are copied into the `JsonNode`.
 *
 * The following `GValue` types have a direct mapping to JSON types:
 *
 *  - `G_TYPE_INT64`
 *  - `G_TYPE_DOUBLE`
 *  - `G_TYPE_BOOLEAN`
 *  - `G_TYPE_STRING`
 *
 * JSON-GLib will also automatically promote the following `GValue` types:
 *
 *  - `G_TYPE_INT` to `G_TYPE_INT64`
 *  - `G_TYPE_FLOAT` to `G_TYPE_DOUBLE`
 *
 * It is an error to call this on an immutable node, or on a node which is not
 * a value node.
 */
void
json_node_set_value (JsonNode     *node,
                     const GValue *value)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);
  g_return_if_fail (G_VALUE_TYPE (value) != G_TYPE_INVALID);
  g_return_if_fail (!node->immutable);

  if (node->data.value == NULL)
    node->data.value = json_value_alloc ();

  switch (G_VALUE_TYPE (value))
    {
    /* auto-promote machine integers to 64 bit integers */
    case G_TYPE_INT64:
    case G_TYPE_INT:
      json_value_init (node->data.value, JSON_VALUE_INT);
      if (G_VALUE_TYPE (value) == G_TYPE_INT64)
        json_value_set_int (node->data.value, g_value_get_int64 (value));
      else
        json_value_set_int (node->data.value, g_value_get_int (value));
      break;

    case G_TYPE_BOOLEAN:
      json_value_init (node->data.value, JSON_VALUE_BOOLEAN);
      json_value_set_boolean (node->data.value, g_value_get_boolean (value));
      break;

    /* auto-promote single-precision floats to double precision floats */
    case G_TYPE_DOUBLE:
    case G_TYPE_FLOAT:
      json_value_init (node->data.value, JSON_VALUE_DOUBLE);
      if (G_VALUE_TYPE (value) == G_TYPE_DOUBLE)
        json_value_set_double (node->data.value, g_value_get_double (value));
      else
        json_value_set_double (node->data.value, g_value_get_float (value));
      break;

    case G_TYPE_STRING:
      json_value_init (node->data.value, JSON_VALUE_STRING);
      json_value_set_string (node->data.value, g_value_get_string (value));
      break;

    default:
      g_message ("Invalid value of type '%s'",
                 g_type_name (G_VALUE_TYPE (value)));
      return;
    }

}

/**
 * json_node_free:
 * @node: the node to free
 *
 * Frees the resources allocated by the node.
 */
void
json_node_free (JsonNode *node)
{
  g_return_if_fail (node == NULL || JSON_NODE_IS_VALID (node));
  g_return_if_fail (node == NULL || node->allocated);

  if (G_LIKELY (node))
    {
      if (node->ref_count > 1)
        g_warning ("Freeing a JsonNode %p owned by other code.", node);

      json_node_unset (node);
      g_slice_free (JsonNode, node);
    }
}

/**
 * json_node_seal:
 * @node: the node to seal
 *
 * Seals the given node, making it immutable to further changes.
 *
 * In order to be sealed, the @node must have a type and value set. The value
 * will be recursively sealed — if the node holds an object, that JSON object
 * will be sealed, etc.
 *
 * If the `node` is already immutable, this is a no-op.
 *
 * Since: 1.2
 */
void
json_node_seal (JsonNode *node)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));

  if (node->immutable)
    return;

  switch (node->type)
    {
    case JSON_NODE_OBJECT:
      g_return_if_fail (node->data.object != NULL);
      json_object_seal (node->data.object);
      break;
    case JSON_NODE_ARRAY:
      g_return_if_fail (node->data.array != NULL);
      json_array_seal (node->data.array);
      break;
    case JSON_NODE_NULL:
      break;
    case JSON_NODE_VALUE:
      g_return_if_fail (node->data.value != NULL);
      json_value_seal (node->data.value);
      break;
    default:
      g_assert_not_reached ();
    }

  node->immutable = TRUE;
}

/**
 * json_node_is_immutable:
 * @node: the node to check
 *
 * Check whether the given @node has been marked as immutable by calling
 * [method@Json.Node.seal] on it.
 *
 * Since: 1.2
 * Returns: `TRUE` if the @node is immutable
 */
gboolean
json_node_is_immutable (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), FALSE);

  return node->immutable;
}

/**
 * json_node_type_name:
 * @node: a node
 *
 * Retrieves the user readable name of the data type contained by @node.
 *
 * **Note**: The name is only meant for debugging purposes, and there is no
 * guarantee the name will stay the same across different versions.
 *
 * Return value: (transfer none): a string containing the name of the type
 */
const gchar *
json_node_type_name (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, "(null)");

  switch (node->type)
    {
    case JSON_NODE_OBJECT:
    case JSON_NODE_ARRAY:
    case JSON_NODE_NULL:
      return json_node_type_get_name (node->type);

    case JSON_NODE_VALUE:
      if (node->data.value)
        return json_value_type_get_name (node->data.value->type);
    }

  return "unknown";
}

const gchar *
json_node_type_get_name (JsonNodeType node_type)
{
  switch (node_type)
    {
    case JSON_NODE_OBJECT:
      return "JsonObject";

    case JSON_NODE_ARRAY:
      return "JsonArray";

    case JSON_NODE_NULL:
      return "NULL";

    case JSON_NODE_VALUE:
      return "Value";

    default:
      g_assert_not_reached ();
      break;
    }

  return "unknown";
}

/**
 * json_node_set_parent:
 * @node: the node to change
 * @parent: (transfer none) (nullable): the parent node
 *
 * Sets the parent node for the given `node`.
 *
 * It is an error to call this with an immutable @parent.
 *
 * The @node may be immutable.
 *
 * Since: 0.8
 */
void
json_node_set_parent (JsonNode *node,
                      JsonNode *parent)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (parent == NULL ||
                    !json_node_is_immutable (parent));

  node->parent = parent;
}

/**
 * json_node_get_parent:
 * @node: the node to query
 *
 * Retrieves the parent node of the given @node.
 *
 * Return value: (transfer none) (nullable): the parent node, or `NULL` if @node
 *   is the root node
 */
JsonNode *
json_node_get_parent (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);

  return node->parent;
}

/**
 * json_node_set_string:
 * @node: a node initialized to `JSON_NODE_VALUE`
 * @value: a string value
 *
 * Sets @value as the string content of the @node, replacing any existing
 * content.
 *
 * It is an error to call this on an immutable node, or on a node which is not
 * a value node.
 */
void
json_node_set_string (JsonNode    *node,
                      const gchar *value)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);
  g_return_if_fail (!node->immutable);

  if (node->data.value == NULL)
    node->data.value = json_value_init (json_value_alloc (), JSON_VALUE_STRING);
  else
    json_value_init (node->data.value, JSON_VALUE_STRING);

  json_value_set_string (node->data.value, value);
}

/**
 * json_node_get_string:
 * @node: a node holding a string
 *
 * Gets the string value stored inside a node.
 *
 * If the node does not hold a string value, `NULL` is returned.
 *
 * Return value: (nullable): a string value.
 */
const gchar *
json_node_get_string (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);

  if (JSON_NODE_TYPE (node) == JSON_NODE_NULL)
    return NULL;

  if (JSON_VALUE_HOLDS_STRING (node->data.value))
    return json_value_get_string (node->data.value);

  return NULL;
}

/**
 * json_node_dup_string:
 * @node: a node holding a string
 *
 * Gets a copy of the string value stored inside a node.
 *
 * If the node does not hold a string value, `NULL` is returned.
 *
 * Return value: (transfer full) (nullable): a copy of the string
 *   inside the node
 */
gchar *
json_node_dup_string (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), NULL);

  return g_strdup (json_node_get_string (node));
}

/**
 * json_node_set_int:
 * @node: a node initialized to `JSON_NODE_VALUE`
 * @value: an integer value
 *
 * Sets @value as the integer content of the @node, replacing any existing
 * content.
 *
 * It is an error to call this on an immutable node, or on a node which is not
 * a value node.
 */
void
json_node_set_int (JsonNode *node,
                   gint64    value)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);
  g_return_if_fail (!node->immutable);

  if (node->data.value == NULL)
    node->data.value = json_value_init (json_value_alloc (), JSON_VALUE_INT);
  else
    json_value_init (node->data.value, JSON_VALUE_INT);

  json_value_set_int (node->data.value, value);
}

/**
 * json_node_get_int:
 * @node: a node holding an integer
 *
 * Gets the integer value stored inside a node.
 *
 * If the node holds a double value, its integer component is returned.
 *
 * If the node holds a `FALSE` boolean value, `0` is returned; otherwise,
 * a non-zero integer is returned.
 *
 * If the node holds a `JSON_NODE_NULL` value or a value of another
 * non-integer type, `0` is returned.
 *
 * Return value: an integer value.
 */
gint64
json_node_get_int (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), 0);

  if (JSON_NODE_TYPE (node) == JSON_NODE_NULL)
    return 0;

  if (JSON_VALUE_HOLDS_INT (node->data.value))
    return json_value_get_int (node->data.value);

  if (JSON_VALUE_HOLDS_DOUBLE (node->data.value))
    return json_value_get_double (node->data.value);

  if (JSON_VALUE_HOLDS_BOOLEAN (node->data.value))
    return json_value_get_boolean (node->data.value);

  return 0;
}

/**
 * json_node_set_double:
 * @node: a node initialized to `JSON_NODE_VALUE`
 * @value: a double value
 *
 * Sets @value as the double content of the @node, replacing any existing
 * content.
 *
 * It is an error to call this on an immutable node, or on a node which is not
 * a value node.
 */
void
json_node_set_double (JsonNode *node,
                      gdouble   value)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);
  g_return_if_fail (!node->immutable);

  if (node->data.value == NULL)
    node->data.value = json_value_init (json_value_alloc (), JSON_VALUE_DOUBLE);
  else
    json_value_init (node->data.value, JSON_VALUE_DOUBLE);

  json_value_set_double (node->data.value, value);
}

/**
 * json_node_get_double:
 * @node: a node holding a floating point value
 *
 * Gets the double value stored inside a node.
 *
 * If the node holds an integer value, it is returned as a double.
 *
 * If the node holds a `FALSE` boolean value, `0.0` is returned; otherwise
 * a non-zero double is returned.
 *
 * If the node holds a `JSON_NODE_NULL` value or a value of another
 * non-double type, `0.0` is returned.
 *
 * Return value: a double value.
 */
gdouble
json_node_get_double (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), 0.0);

  if (JSON_NODE_TYPE (node) == JSON_NODE_NULL)
    return 0;

  if (JSON_VALUE_HOLDS_DOUBLE (node->data.value))
    return json_value_get_double (node->data.value);

  if (JSON_VALUE_HOLDS_INT (node->data.value))
    return (gdouble) json_value_get_int (node->data.value);

  if (JSON_VALUE_HOLDS_BOOLEAN (node->data.value))
    return (gdouble) json_value_get_boolean (node->data.value);

  return 0.0;
}

/**
 * json_node_set_boolean:
 * @node: a node initialized to `JSON_NODE_VALUE`
 * @value: a boolean value
 *
 * Sets @value as the boolean content of the @node, replacing any existing
 * content.
 *
 * It is an error to call this on an immutable node, or on a node which is not
 * a value node.
 */
void
json_node_set_boolean (JsonNode *node,
                       gboolean  value)
{
  g_return_if_fail (JSON_NODE_IS_VALID (node));
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);
  g_return_if_fail (!node->immutable);

  if (node->data.value == NULL)
    node->data.value = json_value_init (json_value_alloc (), JSON_VALUE_BOOLEAN);
  else
    json_value_init (node->data.value, JSON_VALUE_BOOLEAN);

  json_value_set_boolean (node->data.value, value);
}

/**
 * json_node_get_boolean:
 * @node: a node holding a boolean value
 *
 * Gets the boolean value stored inside a node.
 *
 * If the node holds an integer or double value which is zero, `FALSE` is
 * returned; otherwise `TRUE` is returned.
 *
 * If the node holds a `JSON_NODE_NULL` value or a value of another
 * non-boolean type, `FALSE` is returned.
 *
 * Return value: a boolean value.
 */
gboolean
json_node_get_boolean (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), FALSE);

  if (JSON_NODE_TYPE (node) == JSON_NODE_NULL)
    return FALSE;

  if (JSON_VALUE_HOLDS_BOOLEAN (node->data.value))
    return json_value_get_boolean (node->data.value);

  if (JSON_VALUE_HOLDS_INT (node->data.value))
    return json_value_get_int (node->data.value) != 0;

  if (JSON_VALUE_HOLDS_DOUBLE (node->data.value))
    return json_value_get_double (node->data.value) != 0.0;

  return FALSE;
}

/**
 * json_node_get_node_type:
 * @node: the node to check
 *
 * Retrieves the type of a @node.
 *
 * Return value: the type of the node
 *
 * Since: 0.8
 */
JsonNodeType
json_node_get_node_type (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), JSON_NODE_NULL);

  return node->type;
}

/**
 * json_node_is_null:
 * @node: the node to check
 *
 * Checks whether @node is a `JSON_NODE_NULL`.
 *
 * A `JSON_NODE_NULL` node is not the same as a `NULL` node; a `JSON_NODE_NULL`
 * represents a literal `null` value in the JSON tree.
 *
 * Return value: `TRUE` if the node is null
 *
 * Since: 0.8
 */
gboolean
json_node_is_null (JsonNode *node)
{
  g_return_val_if_fail (JSON_NODE_IS_VALID (node), TRUE);

  return node->type == JSON_NODE_NULL;
}

/*< private >
 * json_type_is_a:
 * @sub: sub-type
 * @super: super-type
 *
 * Check whether @sub is a sub-type of, or equal to, @super.
 *
 * The only sub-type relationship in the JSON Schema type system is that
 * an integer type is a sub-type of a number type.
 *
 * Formally, this function calculates: `@sub <: @super`.
 *
 * Reference: http://json-schema.org/latest/json-schema-core.html#rfc.section.3.5
 *
 * Returns: `TRUE` if @sub is a sub-type of, or equal to, @super; `FALSE` otherwise
 * Since: 1.2
 */
static gboolean
json_type_is_a (JsonNode *sub,
                JsonNode *super)
{
  if (super->type == JSON_NODE_VALUE && sub->type == JSON_NODE_VALUE)
    {
      JsonValueType super_value_type, sub_value_type;

      if (super->data.value == NULL || sub->data.value == NULL)
        return FALSE;

      super_value_type = super->data.value->type;
      sub_value_type = sub->data.value->type;

      return (super_value_type == sub_value_type ||
              (super_value_type == JSON_VALUE_DOUBLE &&
	       sub_value_type == JSON_VALUE_INT));
    }

  return (super->type == sub->type);
}

/**
 * json_string_hash:
 * @key: (type utf8): a JSON string to hash
 *
 * Calculate a hash value for the given @key (a UTF-8 JSON string).
 *
 * Note: Member names are compared byte-wise, without applying any Unicode
 * decomposition or normalisation. This is not explicitly mentioned in the JSON
 * standard (ECMA-404), but is assumed.
 *
 * Returns: hash value for @key
 * Since: 1.2
 */
guint
json_string_hash (gconstpointer key)
{
  return g_str_hash (key);
}

/**
 * json_string_equal:
 * @a: (type utf8): a JSON string
 * @b: (type utf8): another JSON string
 *
 * Check whether @a and @b are equal UTF-8 JSON strings.
 *
 * Returns: `TRUE` if @a and @b are equal; `FALSE` otherwise
 * Since: 1.2
 */
gboolean
json_string_equal (gconstpointer  a,
                   gconstpointer  b)
{
  return g_str_equal (a, b);
}

/**
 * json_string_compare:
 * @a: (type utf8): a JSON string
 * @b: (type utf8): another JSON string
 *
 * Check whether @a and @b are equal UTF-8 JSON strings and return an ordering
 * over them in `strcmp()` style.
 *
 * Returns: an integer less than zero if `a < b`, equal to zero if `a == b`, and
 *   greater than zero if `a > b`
 *
 * Since: 1.2
 */
gint
json_string_compare (gconstpointer  a,
                     gconstpointer  b)
{
  return g_strcmp0 (a, b);
}

/**
 * json_node_hash:
 * @key: (type JsonNode): a JSON node to hash
 *
 * Calculate a hash value for the given @key.
 *
 * The hash is calculated over the node and its value, recursively. If the node
 * is immutable, this is a fast operation; otherwise, it scales proportionally
 * with the size of the node’s value (for example, with the number of members
 * in the JSON object if this node contains an object).
 *
 * Returns: hash value for @key
 * Since: 1.2
 */
guint
json_node_hash (gconstpointer key)
{
  JsonNode *node;  /* unowned */

  /* These are all randomly generated and arbitrary. */
  const guint value_hash = 0xc19e75ad;
  const guint array_hash = 0x865acfc2;
  const guint object_hash = 0x3c8f3135;

  node = (JsonNode *) key;

  /* XOR the hash values with a (constant) random number depending on the node’s
   * type so that empty values, arrays and objects do not all collide at the
   * hash value 0. */
  switch (node->type)
    {
    case JSON_NODE_NULL:
      return 0;
    case JSON_NODE_VALUE:
      return value_hash ^ json_value_hash (node->data.value);
    case JSON_NODE_ARRAY:
      return array_hash ^ json_array_hash (json_node_get_array (node));
    case JSON_NODE_OBJECT:
      return object_hash ^ json_object_hash (json_node_get_object (node));
    default:
      g_assert_not_reached ();
    }
}

/**
 * json_node_equal:
 * @a: (type JsonNode): a JSON node
 * @b: (type JsonNode): another JSON node
 *
 * Check whether @a and @b are equal node, meaning they have the same
 * type and same values (checked recursively).
 *
 * Note that integer values are compared numerically, ignoring type, so a
 * double value 4.0 is equal to the integer value 4.
 *
 * Returns: `TRUE` if @a and @b are equal; `FALSE` otherwise
 * Since: 1.2
 */
gboolean
json_node_equal (gconstpointer  a,
                 gconstpointer  b)
{
  JsonNode *node_a, *node_b;  /* unowned */

  node_a = (JsonNode *) a;
  node_b = (JsonNode *) b;

  /* Identity comparison. */
  if (node_a == node_b)
    return TRUE;

  /* Eliminate mismatched types rapidly. */
  if (!json_type_is_a (node_a, node_b) &&
      !json_type_is_a (node_b, node_a))
    {
      return FALSE;
    }

  switch (node_a->type)
    {
    case JSON_NODE_NULL:
      /* Types match already. */
      return TRUE;
    case JSON_NODE_ARRAY:
      return json_array_equal (json_node_get_array (node_a),
                               json_node_get_array (node_b));
    case JSON_NODE_OBJECT:
      return json_object_equal (json_node_get_object (node_a),
                                json_node_get_object (node_b));
    case JSON_NODE_VALUE:
      /* Handled below. */
      break;
    default:
      g_assert_not_reached ();
    }

  /* Handle values. */
  switch (node_a->data.value->type)
    {
    case JSON_VALUE_NULL:
      /* Types already match. */
      return TRUE;
    case JSON_VALUE_BOOLEAN:
      return (json_node_get_boolean (node_a) == json_node_get_boolean (node_b));
    case JSON_VALUE_STRING:
      return json_string_equal (json_node_get_string (node_a),
                                json_node_get_string (node_b));
    case JSON_VALUE_DOUBLE:
    case JSON_VALUE_INT: {
      gdouble val_a, val_b;
      JsonValueType value_type_a, value_type_b;

      value_type_a = node_a->data.value->type;
      value_type_b = node_b->data.value->type;

      /* Integer comparison doesn’t need to involve doubles… */
      if (value_type_a == JSON_VALUE_INT &&
          value_type_b == JSON_VALUE_INT)
        {
          return (json_node_get_int (node_a) ==
                  json_node_get_int (node_b));
        }

      /* …but everything else does. We can use bitwise double equality here,
       * since we’re not doing any calculations which could introduce floating
       * point error. We expect that the doubles in the JSON nodes come directly
       * from strtod() or similar, so should be bitwise equal for equal string
       * representations.
       *
       * Interesting background reading:
       * http://randomascii.wordpress.com/2012/06/26/\
       *   doubles-are-not-floats-so-dont-compare-them/
       */
      if (value_type_a == JSON_VALUE_INT)
        val_a = json_node_get_int (node_a);
      else
        val_a = json_node_get_double (node_a);

      if (value_type_b == JSON_VALUE_INT)
        val_b = json_node_get_int (node_b);
      else
        val_b = json_node_get_double (node_b);

      return (val_a == val_b);
    }
    case JSON_VALUE_INVALID:
    default:
      g_assert_not_reached ();
    }
}
