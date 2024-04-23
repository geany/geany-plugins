/* json-object.c - JSON object implementation
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

#include "config.h"

#include <string.h>
#include <glib.h>

#include "json-types-private.h"

/**
 * JsonObject:
 *
 * `JsonObject` is the representation of the object type inside JSON.
 *
 * A `JsonObject` contains [struct@Json.Node] "members", which may contain
 * fundamental types, arrays or other objects; each member of an object is
 * accessed using a unique string, or "name".
 *
 * Since objects can be arbitrarily big, copying them can be expensive; for
 * this reason they are reference counted. You can control the lifetime of
 * a `JsonObject` using [method@Json.Object.ref] and [method@Json.Object.unref].
 *
 * To add or overwrite a member with a given name, use [method@Json.Object.set_member].
 *
 * To extract a member with a given name, use [method@Json.Object.get_member].
 *
 * To retrieve the list of members, use [method@Json.Object.get_members].
 *
 * To retrieve the size of the object (that is, the number of members it has),
 * use [method@Json.Object.get_size].
 */

G_DEFINE_BOXED_TYPE (JsonObject, json_object, json_object_ref, json_object_unref);

/**
 * json_object_new: (constructor)
 * 
 * Creates a new object.
 *
 * Returns: (transfer full): the newly created object
 */
JsonObject *
json_object_new (void)
{
  JsonObject *object;

  object = g_slice_new0 (JsonObject);

  object->age = 0;
  object->ref_count = 1;
  object->members = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free,
                                           (GDestroyNotify) json_node_unref);
  g_queue_init (&object->members_ordered);

  return object;
}

/**
 * json_object_ref:
 * @object: a JSON object
 *
 * Acquires a reference on the given object.
 *
 * Returns: (transfer none): the given object, with the reference count
 *   increased by one.
 */
JsonObject *
json_object_ref (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (object->ref_count > 0, NULL);

  object->ref_count++;

  return object;
}

/**
 * json_object_unref:
 * @object: a JSON object
 *
 * Releases a reference on the given object.
 *
 * If the reference count reaches zero, the object is destroyed and
 * all its resources are freed.
 */
void
json_object_unref (JsonObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (object->ref_count > 0);

  if (--object->ref_count == 0)
    {
      g_queue_clear (&object->members_ordered);
      g_hash_table_destroy (object->members);
      object->members = NULL;

      g_slice_free (JsonObject, object);
    }
}

/**
 * json_object_seal:
 * @object: a JSON object
 *
 * Seals the object, making it immutable to further changes.
 *
 * This function will recursively seal all members of the object too.
 *
 * If the object is already immutable, this is a no-op.
 *
 * Since: 1.2
 */
void
json_object_seal (JsonObject *object)
{
  JsonObjectIter iter;
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (object->ref_count > 0);

  if (object->immutable)
    return;

  /* Propagate to all members. */
  json_object_iter_init (&iter, object);

  while (json_object_iter_next (&iter, NULL, &node))
    json_node_seal (node);

  object->immutable_hash = json_object_hash (object);
  object->immutable = TRUE;
}

/**
 * json_object_is_immutable:
 * @object: a JSON object
 *
 * Checks whether the given object has been marked as immutable by calling
 * [method@Json.Object.seal] on it.
 *
 * Since: 1.2
 * Returns: `TRUE` if the object is immutable
 */
gboolean
json_object_is_immutable (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (object->ref_count > 0, FALSE);

  return object->immutable;
}

static inline void
object_set_member_internal (JsonObject  *object,
                            const gchar *member_name,
                            JsonNode    *node)
{
  gchar *name = g_strdup (member_name);

  if (g_hash_table_lookup (object->members, name) == NULL)
    {
      g_queue_push_tail (&object->members_ordered, name);
      object->age += 1;
    }
  else
    {
      GList *l;

      /* if the member already exists then we need to replace the
       * pointer to its name, to avoid keeping invalid pointers
       * once we replace the key in the hash table
       */
      l = g_queue_find_custom (&object->members_ordered, name, (GCompareFunc) strcmp);
      if (l != NULL)
        l->data = name;
    }

  g_hash_table_replace (object->members, name, node);
}

/**
 * json_object_add_member:
 * @object: a JSON object
 * @member_name: the name of the member
 * @node: (transfer full): the value of the member
 *
 * Adds a new member for the given name and value into an object.
 *
 * This function will return if the object already contains a member
 * with the same name.
 *
 * Deprecated: 0.8: Use [method@Json.Object.set_member] instead
 */
void
json_object_add_member (JsonObject  *object,
                        const gchar *member_name,
                        JsonNode    *node)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);
  g_return_if_fail (node != NULL);

  if (json_object_has_member (object, member_name))
    {
      g_warning ("JsonObject already has a `%s' member of type `%s'",
                 member_name,
                 json_node_type_name (node));
      return;
    }

  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_member:
 * @object: a JSON object
 * @member_name: the name of the member
 * @node: (transfer full): the value of the member
 *
 * Sets the value of a member inside an object.
 *
 * If the object does not have a member with the given name, a new member
 * is created.
 *
 * If the object already has a member with the given name, the current
 * value is overwritten with the new.
 *
 * Since: 0.8
 */
void
json_object_set_member (JsonObject  *object,
                        const gchar *member_name,
                        JsonNode    *node)
{
  JsonNode *old_node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);
  g_return_if_fail (node != NULL);

  old_node = g_hash_table_lookup (object->members, member_name);
  if (old_node == NULL)
    goto set_member;

  if (old_node == node)
    return;

set_member:
  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_int_member:
 * @object: a JSON object
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting an object member with an integer value.
 *
 * See also: [method@Json.Object.set_member], [method@Json.Node.init_int]
 *
 * Since: 0.8
 */
void
json_object_set_int_member (JsonObject  *object,
                            const gchar *member_name,
                            gint64       value)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  object_set_member_internal (object, member_name, json_node_init_int (json_node_alloc (), value));
}

/**
 * json_object_set_double_member:
 * @object: a JSON object
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting an object member with a floating point value.
 *
 * See also: [method@Json.Object.set_member], [method@Json.Node.init_double]
 *
 * Since: 0.8
 */
void
json_object_set_double_member (JsonObject  *object,
                               const gchar *member_name,
                               gdouble      value)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  object_set_member_internal (object, member_name, json_node_init_double (json_node_alloc (), value));
}

/**
 * json_object_set_boolean_member:
 * @object: a JSON object
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting an object member with a boolean value.
 *
 * See also: [method@Json.Object.set_member], [method@Json.Node.init_boolean]
 *
 * Since: 0.8
 */
void
json_object_set_boolean_member (JsonObject  *object,
                                const gchar *member_name,
                                gboolean     value)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  object_set_member_internal (object, member_name, json_node_init_boolean (json_node_alloc (), value));
}

/**
 * json_object_set_string_member:
 * @object: a JSON object
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting an object member with a string value.
 *
 * See also: [method@Json.Object.set_member], [method@Json.Node.init_string]
 *
 * Since: 0.8
 */
void
json_object_set_string_member (JsonObject  *object,
                               const gchar *member_name,
                               const gchar *value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_alloc ();

  if (value != NULL)
    json_node_init_string (node, value);
  else
    json_node_init_null (node);

  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_null_member:
 * @object: a JSON object
 * @member_name: the name of the member
 *
 * Convenience function for setting an object member with a `null` value.
 *
 * See also: [method@Json.Object.set_member], [method@Json.Node.init_null]
 *
 * Since: 0.8
 */
void
json_object_set_null_member (JsonObject  *object,
                             const gchar *member_name)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  object_set_member_internal (object, member_name, json_node_init_null (json_node_alloc ()));
}

/**
 * json_object_set_array_member:
 * @object: a JSON object
 * @member_name: the name of the member
 * @value: (transfer full): the value of the member
 *
 * Convenience function for setting an object member with an array value.
 *
 * See also: [method@Json.Object.set_member], [method@Json.Node.take_array]
 *
 * Since: 0.8
 */
void
json_object_set_array_member (JsonObject  *object,
                              const gchar *member_name,
                              JsonArray   *value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_alloc ();

  if (value != NULL)
    {
      json_node_init_array (node, value);
      json_array_unref (value);
    }
  else
    json_node_init_null (node);

  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_object_member:
 * @object: a JSON object
 * @member_name: the name of the member
 * @value: (transfer full): the value of the member
 *
 * Convenience function for setting an object member with an object value.
 *
 * See also: [method@Json.Object.set_member], [method@Json.Node.take_object]
 *
 * Since: 0.8
 */
void
json_object_set_object_member (JsonObject  *object,
                               const gchar *member_name,
                               JsonObject  *value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_alloc ();

  if (value != NULL)
    {
      json_node_init_object (node, value);
      json_object_unref (value);
    }
  else
    json_node_init_null (node);

  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_get_members:
 * @object: a JSON object
 *
 * Retrieves all the names of the members of an object.
 *
 * You can obtain the value for each member by iterating the returned list
 * and calling [method@Json.Object.get_member].
 *
 * Returns: (element-type utf8) (transfer container) (nullable): the
 *   member names of the object
 */
GList *
json_object_get_members (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);

  return g_list_copy (object->members_ordered.head);
}


GQueue *
json_object_get_members_internal (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);

  return &object->members_ordered;
}

/**
 * json_object_get_values:
 * @object: a JSON object
 *
 * Retrieves all the values of the members of an object.
 *
 * Returns: (element-type JsonNode) (transfer container) (nullable): the
 *   member values of the object
 */
GList *
json_object_get_values (JsonObject *object)
{
  GList *values, *l;

  g_return_val_if_fail (object != NULL, NULL);

  values = NULL;
  for (l = object->members_ordered.tail; l != NULL; l = l->prev)
    values = g_list_prepend (values, g_hash_table_lookup (object->members, l->data));

  return values;
}

/**
 * json_object_dup_member:
 * @object: a JSON object
 * @member_name: the name of the JSON object member to access
 *
 * Retrieves a copy of the value of the given member inside an object.
 *
 * Returns: (transfer full) (nullable): a copy of the value for the
 *   requested object member
 *
 * Since: 0.6
 */
JsonNode *
json_object_dup_member (JsonObject  *object,
                        const gchar *member_name)
{
  JsonNode *retval;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  retval = json_object_get_member (object, member_name);
  if (!retval)
    return NULL;

  return json_node_copy (retval);
}

static inline JsonNode *
object_get_member_internal (JsonObject  *object,
                            const gchar *member_name)
{
  return g_hash_table_lookup (object->members, member_name);
}

/**
 * json_object_get_member:
 * @object: a JSON object
 * @member_name: the name of the JSON object member to access
 *
 * Retrieves the value of the given member inside an object.
 *
 * Returns: (transfer none) (nullable): the value for the
 *   requested object member
 */
JsonNode *
json_object_get_member (JsonObject  *object,
                        const gchar *member_name)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  return object_get_member_internal (object, member_name);
}

#define JSON_OBJECT_GET(ret_type,type_name) \
ret_type \
json_object_get_ ##type_name## _member (JsonObject *object, \
                                        const char *member_name) \
{ \
  g_return_val_if_fail (object != NULL, (ret_type) 0); \
  g_return_val_if_fail (member_name != NULL, (ret_type) 0); \
\
  JsonNode *node = object_get_member_internal (object, member_name); \
  g_return_val_if_fail (node != NULL, (ret_type) 0); \
\
  if (JSON_NODE_HOLDS_NULL (node)) \
    return (ret_type) 0; \
\
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, (ret_type) 0); \
\
  return json_node_get_ ##type_name (node); \
}

#define JSON_OBJECT_GET_DEFAULT(ret_type,type_name) \
ret_type \
json_object_get_ ##type_name## _member_with_default (JsonObject *object, \
                                                     const char *member_name, \
                                                     ret_type    default_value) \
{ \
  g_return_val_if_fail (object != NULL, default_value); \
  g_return_val_if_fail (member_name != NULL, default_value); \
\
  JsonNode *node = object_get_member_internal (object, member_name); \
  if (node == NULL) \
    return default_value; \
\
  if (JSON_NODE_HOLDS_NULL (node)) \
    return default_value; \
\
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, default_value); \
\
  return json_node_get_ ##type_name (node); \
}

/**
 * json_object_get_int_member:
 * @object: a JSON object
 * @member_name: the name of the object member
 *
 * Convenience function that retrieves the integer value
 * stored in @member_name of @object. It is an error to specify a
 * @member_name which does not exist.
 *
 * See also: [method@Json.Object.get_int_member_with_default],
 *   [method@Json.Object.get_member], [method@Json.Object.has_member]
 *
 * Returns: the integer value of the object's member
 *
 * Since: 0.8
 */
JSON_OBJECT_GET (gint64, int)

/**
 * json_object_get_int_member_with_default:
 * @object: a JSON object
 * @member_name: the name of the object member
 * @default_value: the value to return if @member_name is not valid
 *
 * Convenience function that retrieves the integer value
 * stored in @member_name of @object.
 *
 * If @member_name does not exist, does not contain a scalar value,
 * or contains `null`, then @default_value is returned instead.
 *
 * Returns: the integer value of the object's member, or the
 *   given default
 *
 * Since: 1.6
 */
JSON_OBJECT_GET_DEFAULT (gint64, int)

/**
 * json_object_get_double_member:
 * @object: a JSON object
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the floating point value
 * stored in @member_name of @object. It is an error to specify a
 * @member_name which does not exist.
 *
 * See also: [method@Json.Object.get_double_member_with_default],
 *   [method@Json.Object.get_member], [method@Json.Object.has_member]
 *
 * Returns: the floating point value of the object's member
 *
 * Since: 0.8
 */
JSON_OBJECT_GET (gdouble, double)

/**
 * json_object_get_double_member_with_default:
 * @object: a JSON object
 * @member_name: the name of the @object member
 * @default_value: the value to return if @member_name is not valid
 *
 * Convenience function that retrieves the floating point value
 * stored in @member_name of @object.
 *
 * If @member_name does not exist, does not contain a scalar value,
 * or contains `null`, then @default_value is returned instead.
 *
 * Returns: the floating point value of the object's member, or the
 *   given default
 *
 * Since: 1.6
 */
JSON_OBJECT_GET_DEFAULT (double, double)

/**
 * json_object_get_boolean_member:
 * @object: a JSON object
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the boolean value
 * stored in @member_name of @object. It is an error to specify a
 * @member_name which does not exist.
 *
 * See also: [method@Json.Object.get_boolean_member_with_default],
 *   [method@Json.Object.get_member], [method@Json.Object.has_member]
 *
 * Returns: the boolean value of the object's member
 *
 * Since: 0.8
 */
JSON_OBJECT_GET (gboolean, boolean)

/**
 * json_object_get_boolean_member_with_default:
 * @object: a JSON object
 * @member_name: the name of the @object member
 * @default_value: the value to return if @member_name is not valid
 *
 * Convenience function that retrieves the boolean value
 * stored in @member_name of @object.
 *
 * If @member_name does not exist, does not contain a scalar value,
 * or contains `null`, then @default_value is returned instead.
 *
 * Returns: the boolean value of the object's member, or the
 *   given default
 *
 * Since: 1.6
 */
JSON_OBJECT_GET_DEFAULT (gboolean, boolean)

/**
 * json_object_get_string_member:
 * @object: a JSON object
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the string value
 * stored in @member_name of @object. It is an error to specify a
 * @member_name that does not exist.
 *
 * See also: [method@Json.Object.get_string_member_with_default],
 *   [method@Json.Object.get_member], [method@Json.Object.has_member]
 *
 * Returns: the string value of the object's member
 *
 * Since: 0.8
 */
JSON_OBJECT_GET (const gchar *, string)

/**
 * json_object_get_string_member_with_default:
 * @object: a JSON object
 * @member_name: the name of the @object member
 * @default_value: the value to return if @member_name is not valid
 *
 * Convenience function that retrieves the string value
 * stored in @member_name of @object.
 *
 * If @member_name does not exist, does not contain a scalar value,
 * or contains `null`, then @default_value is returned instead.
 *
 * Returns: the string value of the object's member, or the
 *   given default
 *
 * Since: 1.6
 */
JSON_OBJECT_GET_DEFAULT (const char *, string)

/**
 * json_object_get_null_member:
 * @object: a JSON object
 * @member_name: the name of the member
 *
 * Convenience function that checks whether the value
 * stored in @member_name of @object is null. It is an error to
 * specify a @member_name which does not exist.
 *
 * See also: [method@Json.Object.get_member], [method@Json.Object.has_member]
 *
 * Returns: `TRUE` if the value is `null`
 *
 * Since: 0.8
 */
gboolean
json_object_get_null_member (JsonObject  *object,
                             const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (member_name != NULL, FALSE);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, FALSE);

  if (JSON_NODE_HOLDS_NULL (node))
    return TRUE;

  if (JSON_NODE_HOLDS_OBJECT (node))
    return json_node_get_object (node) == NULL;

  if (JSON_NODE_HOLDS_ARRAY (node))
    return json_node_get_array (node) == NULL;

  return FALSE;
}

/**
 * json_object_get_array_member:
 * @object: a JSON object
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the array
 * stored in @member_name of @object. It is an error to specify a
 * @member_name which does not exist.
 *
 * If @member_name contains `null`, then this function will return `NULL`.
 *
 * See also: [method@Json.Object.get_member], [method@Json.Object.has_member]
 *
 * Returns: (transfer none) (nullable): the array inside the object's member
 *
 * Since: 0.8
 */
JsonArray *
json_object_get_array_member (JsonObject  *object,
                              const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_HOLDS_ARRAY (node) || JSON_NODE_HOLDS_NULL (node), NULL);

  if (JSON_NODE_HOLDS_NULL (node))
    return NULL;

  return json_node_get_array (node);
}

/**
 * json_object_get_object_member:
 * @object: a JSON object
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the object
 * stored in @member_name of @object. It is an error to specify a @member_name
 * which does not exist.
 *
 * If @member_name contains `null`, then this function will return `NULL`.
 *
 * See also: [method@Json.Object.get_member], [method@Json.Object.has_member]
 *
 * Returns: (transfer none) (nullable): the object inside the object's member
 *
 * Since: 0.8
 */
JsonObject *
json_object_get_object_member (JsonObject  *object,
                               const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (node) || JSON_NODE_HOLDS_NULL (node), NULL);

  if (JSON_NODE_HOLDS_NULL (node))
    return NULL;

  return json_node_get_object (node);
}

/**
 * json_object_has_member:
 * @object: a JSON object
 * @member_name: the name of a JSON object member
 *
 * Checks whether @object has a member named @member_name.
 *
 * Returns: `TRUE` if the JSON object has the requested member
 */
gboolean
json_object_has_member (JsonObject *object,
                        const gchar *member_name)
{
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (member_name != NULL, FALSE);

  return (g_hash_table_lookup (object->members, member_name) != NULL);
}

/**
 * json_object_get_size:
 * @object: a JSON object
 *
 * Retrieves the number of members of a JSON object.
 *
 * Returns: the number of members
 */
guint
json_object_get_size (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, 0);

  return g_hash_table_size (object->members);
}

/**
 * json_object_remove_member:
 * @object: a JSON object
 * @member_name: the name of the member to remove
 *
 * Removes @member_name from @object, freeing its allocated resources.
 */
void
json_object_remove_member (JsonObject  *object,
                           const gchar *member_name)
{
  GList *l;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  for (l = object->members_ordered.head; l != NULL; l = l->next)
    {
      const gchar *name = l->data;

      if (g_strcmp0 (name, member_name) == 0)
        {
          g_queue_delete_link (&object->members_ordered, l);
          break;
        }
    }

  g_hash_table_remove (object->members, member_name);
}

/**
 * json_object_foreach_member:
 * @object: a JSON object
 * @func: (scope call): the function to be called on each member
 * @data: (closure): data to be passed to the function
 *
 * Iterates over all members of @object and calls @func on
 * each one of them.
 *
 * It is safe to change the value of a member of the oobject
 * from within the iterator function, but it is not safe to add or
 * remove members from the object.
 *
 * The order in which the object members are iterated is the
 * insertion order.
 *
 * Since: 0.8
 */
void
json_object_foreach_member (JsonObject        *object,
                            JsonObjectForeach  func,
                            gpointer           data)
{
  GList *l;
  int age;

  g_return_if_fail (object != NULL);
  g_return_if_fail (func != NULL);

  age = object->age;

  for (l = object->members_ordered.head; l != NULL; l = l->next)
    {
      const gchar *member_name = l->data;
      JsonNode *member_node = g_hash_table_lookup (object->members, member_name);

      func (object, member_name, member_node, data);

      g_assert (object->age == age);
    }
}

/**
 * json_object_hash:
 * @key: (type JsonObject): a JSON object to hash
 *
 * Calculate a hash value for the given @key (a JSON object).
 *
 * The hash is calculated over the object and all its members, recursively. If
 * the object is immutable, this is a fast operation; otherwise, it scales
 * proportionally with the number of members in the object.
 *
 * Returns: hash value for @key
 * Since: 1.2
 */
guint
json_object_hash (gconstpointer key)
{
  JsonObject *object = (JsonObject *) key;
  guint hash = 0;
  JsonObjectIter iter;
  const gchar *member_name;
  JsonNode *node;

  g_return_val_if_fail (object != NULL, 0);

  /* If the object is immutable, use the cached hash. */
  if (object->immutable)
    return object->immutable_hash;

  /* Otherwise, calculate from scratch. */
  json_object_iter_init (&iter, object);

  while (json_object_iter_next (&iter, &member_name, &node))
    hash ^= (json_string_hash (member_name) ^ json_node_hash (node));

  return hash;
}

/**
 * json_object_equal:
 * @a: (type JsonObject): a JSON object
 * @b: (type JsonObject): another JSON object
 *
 * Check whether @a and @b are equal objects, meaning they have the same
 * set of members, and the values of corresponding members are equal.
 *
 * Returns: `TRUE` if @a and @b are equal, and `FALSE` otherwise
 * Since: 1.2
 */
gboolean
json_object_equal (gconstpointer  a,
                   gconstpointer  b)
{
  JsonObject *object_a, *object_b;
  guint size_a, size_b;
  JsonObjectIter iter_a;
  JsonNode *child_a, *child_b;  /* unowned */
  const gchar *member_name;

  object_a = (JsonObject *) a;
  object_b = (JsonObject *) b;

  /* Identity comparison. */
  if (object_a == object_b)
    return TRUE;

  /* Check sizes. */
  size_a = json_object_get_size (object_a);
  size_b = json_object_get_size (object_b);

  if (size_a != size_b)
    return FALSE;

  /* Check member names and values. Check the member names first
   * to avoid expensive recursive value comparisons which might
   * be unnecessary. */
  json_object_iter_init (&iter_a, object_a);

  while (json_object_iter_next (&iter_a, &member_name, NULL))
    {
      if (!json_object_has_member (object_b, member_name))
        return FALSE;
    }

  json_object_iter_init (&iter_a, object_a);

  while (json_object_iter_next (&iter_a, &member_name, &child_a))
    {
      child_b = json_object_get_member (object_b, member_name);

      if (!json_node_equal (child_a, child_b))
        return FALSE;
    }

  return TRUE;
}

/**
 * json_object_iter_init:
 * @iter: an uninitialised JSON object iterator
 * @object: the JSON object to iterate over
 *
 * Initialises the @iter and associate it with @object.
 *
 * ```c
 * JsonObjectIter iter;
 * const gchar *member_name;
 * JsonNode *member_node;
 *
 * json_object_iter_init (&iter, some_object);
 * while (json_object_iter_next (&iter, &member_name, &member_node))
 *   {
 *     // Do something with @member_name and @member_node.
 *   }
 * ```
 *
 * The iterator initialized with this function will iterate the
 * members of the object in an undefined order.
 *
 * See also: [method@Json.ObjectIter.init_ordered]
 *
 * Since: 1.2
 */
void
json_object_iter_init (JsonObjectIter  *iter,
                       JsonObject      *object)
{
  JsonObjectIterReal *iter_real = (JsonObjectIterReal *) iter;;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (object->ref_count > 0);

  iter_real->object = object;
  g_hash_table_iter_init (&iter_real->members_iter, object->members);
}

/**
 * json_object_iter_next:
 * @iter: a JSON object iterator
 * @member_name: (out callee-allocates) (transfer none) (optional): return
 *    location for the member name, or %NULL to ignore
 * @member_node: (out callee-allocates) (transfer none) (optional): return
 *    location for the member value, or %NULL to ignore
 *
 * Advances the iterator and retrieves the next member in the object.
 *
 * If the end of the object is reached, `FALSE` is returned and @member_name
 * and @member_node are set to invalid values. After that point, the @iter
 * is invalid.
 *
 * The order in which members are returned by the iterator is undefined. The
 * iterator is invalidated if the object is modified during iteration.
 *
 * You must use this function with an iterator initialized with
 * [method@Json.ObjectIter.init]; using this function with an iterator
 * initialized with [method@Json.ObjectIter.init_ordered] yields undefined
 * behavior.
 *
 * See also: [method@Json.ObjectIter.next_ordered]
 *
 * Returns: `TRUE` if @member_name and @member_node are valid; `FALSE` if
 *   there are no more members
 *
 * Since: 1.2
 */
gboolean
json_object_iter_next (JsonObjectIter  *iter,
                       const gchar    **member_name,
                       JsonNode       **member_node)
{
  JsonObjectIterReal *iter_real = (JsonObjectIterReal *) iter;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (iter_real->object != NULL, FALSE);
  g_return_val_if_fail (iter_real->object->ref_count > 0, FALSE);

  return g_hash_table_iter_next (&iter_real->members_iter,
                                 (gpointer *) member_name,
                                 (gpointer *) member_node);
}

/**
 * json_object_iter_init_ordered:
 * @iter: an uninitialised iterator
 * @object: the JSON object to iterate over
 *
 * Initialises the @iter and associate it with @object.
 *
 * ```c
 * JsonObjectIter iter;
 * const gchar *member_name;
 * JsonNode *member_node;
 *
 * json_object_iter_init_ordered (&iter, some_object);
 * while (json_object_iter_next_ordered (&iter, &member_name, &member_node))
 *   {
 *     // Do something with @member_name and @member_node.
 *   }
 * ```
 *
 * See also: [method@Json.ObjectIter.init]
 *
 * Since: 1.6
 */
void
json_object_iter_init_ordered (JsonObjectIter  *iter,
                               JsonObject      *object)
{
  JsonObjectOrderedIterReal *iter_real = (JsonObjectOrderedIterReal *) iter;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (object->ref_count > 0);

  iter_real->object = object;
  iter_real->cur_member = NULL;
  iter_real->next_member = NULL;
  iter_real->age = iter_real->object->age;
}

/**
 * json_object_iter_next_ordered:
 * @iter: an ordered JSON object iterator
 * @member_name: (out callee-allocates) (transfer none) (optional): return
 *    location for the member name, or %NULL to ignore
 * @member_node: (out callee-allocates) (transfer none) (optional): return
 *    location for the member value, or %NULL to ignore
 *
 * Advances the iterator and retrieves the next member in the object.
 *
 * If the end of the object is reached, `FALSE` is returned and @member_name and
 * @member_node are set to invalid values. After that point, the @iter is invalid.
 *
 * The order in which members are returned by the iterator is the same order in
 * which the members were added to the `JsonObject`. The iterator is invalidated
 * if its `JsonObject` is modified during iteration.
 *
 * You must use this function with an iterator initialized with
 * [method@Json.ObjectIter.init_ordered]; using this function with an iterator
 * initialized with [method@Json.ObjectIter.init] yields undefined behavior.
 *
 * See also: [method@Json.ObjectIter.next]
 *
 * Returns: `TRUE `if @member_name and @member_node are valid; `FALSE` if the end
 *    of the object has been reached
 *
 * Since: 1.6
 */
gboolean
json_object_iter_next_ordered (JsonObjectIter  *iter,
                               const gchar    **member_name,
                               JsonNode       **member_node)
{
  JsonObjectOrderedIterReal *iter_real = (JsonObjectOrderedIterReal *) iter;
  const char *name = NULL;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (iter_real->object != NULL, FALSE);
  g_return_val_if_fail (iter_real->object->ref_count > 0, FALSE);
  g_return_val_if_fail (iter_real->age == iter_real->object->age, FALSE);

  if (iter_real->cur_member == NULL)
    iter_real->cur_member = iter_real->object->members_ordered.head;
  else
    iter_real->cur_member = iter_real->cur_member->next;

  name = iter_real->cur_member != NULL ? iter_real->cur_member->data : NULL;

  if (member_name != NULL)
    *member_name = name;
  if (member_node != NULL)
    {
      if (name != NULL)
        *member_node = g_hash_table_lookup (iter_real->object->members, name);
      else
        *member_name = NULL;
    }

  return iter_real->cur_member != NULL;
}
