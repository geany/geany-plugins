/* json-gboxed.c - JSON GBoxed integration
 * 
 * This file is part of JSON-GLib
 *
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
 * Author:
 *   Emmanuele Bassi  <ebassi@linux.intel.com>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "json-types-private.h"
#include "json-gobject.h"

typedef struct _BoxedTransform  BoxedTransform;

struct _BoxedTransform
{
  GType boxed_type;
  gint node_type;

  JsonBoxedSerializeFunc serialize;
  JsonBoxedDeserializeFunc deserialize;
};

G_LOCK_DEFINE_STATIC (boxed_serialize);
static GSList *boxed_serialize = NULL;

G_LOCK_DEFINE_STATIC (boxed_deserialize);
static GSList *boxed_deserialize = NULL;

static gint
boxed_transforms_cmp (gconstpointer a,
                      gconstpointer b)
{
  const BoxedTransform *ta = a;
  const BoxedTransform *tb = b;

  return tb->boxed_type - ta->boxed_type;
}

static gint
boxed_transforms_find (gconstpointer a,
                       gconstpointer b)
{
  const BoxedTransform *haystack = a;
  const BoxedTransform *needle = b;

  if (needle->node_type != -1)
    return (haystack->boxed_type == needle->boxed_type &&
            haystack->node_type == needle->node_type) ? 0 : 1;
  else
    return (haystack->boxed_type == needle->boxed_type) ? 0 : 1;
}

static BoxedTransform *
lookup_boxed_transform (GSList       *transforms,
                        GType         gboxed_type,
                        JsonNodeType  node_type)
{
  BoxedTransform lookup;
  GSList *t;

  lookup.boxed_type = gboxed_type;
  lookup.node_type = node_type;

  t = g_slist_find_custom (transforms, &lookup, boxed_transforms_find);
  if (t == NULL)
    return NULL;

  return t->data;
}

/**
 * json_boxed_register_serialize_func: (skip)
 * @gboxed_type: a boxed type
 * @node_type: a node type
 * @serialize_func: serialization function
 *
 * Registers a serialization function for a `GBoxed` of type `gboxed_type`
 * to a [struct@Json.Node] of type `node_type`.
 *
 * Since: 0.10
 */
void
json_boxed_register_serialize_func (GType                  gboxed_type,
                                    JsonNodeType           node_type,
                                    JsonBoxedSerializeFunc serialize_func)
{
  BoxedTransform *t;

  g_return_if_fail (G_TYPE_IS_BOXED (gboxed_type));
  g_return_if_fail (G_TYPE_IS_ABSTRACT (gboxed_type) == FALSE);

  G_LOCK (boxed_serialize);

  t = lookup_boxed_transform (boxed_serialize, gboxed_type, node_type);
  if (t == NULL)
    {
      t = g_slice_new (BoxedTransform);

      t->boxed_type = gboxed_type;
      t->node_type = node_type;
      t->serialize = serialize_func;

      boxed_serialize = g_slist_insert_sorted (boxed_serialize, t,
                                               boxed_transforms_cmp);
    }
  else
    g_warning ("A serialization function for the boxed type %s into "
               "JSON nodes of type %s already exists",
               g_type_name (gboxed_type),
               json_node_type_get_name (node_type));

  G_UNLOCK (boxed_serialize);
}

/**
 * json_boxed_register_deserialize_func: (skip)
 * @gboxed_type: a boxed type
 * @node_type: a node type
 * @deserialize_func: deserialization function
 *
 * Registers a deserialization function for a `GBoxed` of type `gboxed_type`
 * from a [struct@Json.Node] of type `node_type`.
 *
 * Since: 0.10
 */
void
json_boxed_register_deserialize_func (GType                    gboxed_type,
                                      JsonNodeType             node_type,
                                      JsonBoxedDeserializeFunc deserialize_func)
{
  BoxedTransform *t;

  g_return_if_fail (G_TYPE_IS_BOXED (gboxed_type));
  g_return_if_fail (G_TYPE_IS_ABSTRACT (gboxed_type) == FALSE);

  G_LOCK (boxed_deserialize);

  t = lookup_boxed_transform (boxed_deserialize, gboxed_type, node_type);
  if (t == NULL)
    {
      t = g_slice_new (BoxedTransform);

      t->boxed_type = gboxed_type;
      t->node_type = node_type;
      t->deserialize = deserialize_func;

      boxed_deserialize = g_slist_insert_sorted (boxed_deserialize, t,
                                                 boxed_transforms_cmp);
    }
  else
    g_warning ("A deserialization function for the boxed type %s from "
               "JSON nodes of type %s already exists",
               g_type_name (gboxed_type),
               json_node_type_get_name (node_type));

  G_UNLOCK (boxed_deserialize);
}

/**
 * json_boxed_can_serialize:
 * @gboxed_type: a boxed type
 * @node_type: (out) (optional): the node type to which the boxed type
 *   can be serialized into
 *
 * Checks whether it is possible to serialize a `GBoxed` of
 * type `gboxed_type` into a [struct@Json.Node].
 *
 * The type of the node is placed inside `node_type` if the function
 * returns `TRUE`, and it's undefined otherwise.
 *
 * Return value: `TRUE` if the type can be serialized, and `FALSE` otherwise
 *
 * Since: 0.10
 */
gboolean
json_boxed_can_serialize (GType         gboxed_type,
                          JsonNodeType *node_type)
{
  BoxedTransform *t;

  g_return_val_if_fail (G_TYPE_IS_BOXED (gboxed_type), FALSE);
  g_return_val_if_fail (G_TYPE_IS_ABSTRACT (gboxed_type) == FALSE, FALSE);

  t = lookup_boxed_transform (boxed_serialize, gboxed_type, -1);
  if (t != NULL)
    {
      if (node_type)
        *node_type = t->node_type;

      return TRUE;
    }

  return FALSE;
}

/**
 * json_boxed_can_deserialize:
 * @gboxed_type: a boxed type
 * @node_type: a node type
 *
 * Checks whether it is possible to deserialize a `GBoxed` of
 * type `gboxed_type` from a [struct@Json.Node] of type `node_type`.
 *
 * Return value: `TRUE` if the type can be deserialized, and `FALSE` otherwise
 *
 * Since: 0.10
 */
gboolean
json_boxed_can_deserialize (GType        gboxed_type,
                            JsonNodeType node_type)
{
  BoxedTransform *t;

  g_return_val_if_fail (G_TYPE_IS_BOXED (gboxed_type), FALSE);
  g_return_val_if_fail (G_TYPE_IS_ABSTRACT (gboxed_type) == FALSE, FALSE);

  t = lookup_boxed_transform (boxed_deserialize, gboxed_type, node_type);
  if (t != NULL)
    return TRUE;

  return FALSE;
}

/**
 * json_boxed_serialize:
 * @gboxed_type: a boxed type
 * @boxed: a pointer to a boxed of type `gboxed_type`
 *
 * Serializes a pointer to a `GBoxed` of the given type into a [struct@Json.Node].
 *
 * If the serialization is not possible, this function will return `NULL`.
 *
 * Return value: (nullable) (transfer full): a node with the serialized boxed type
 *
 * Since: 0.10
 */
JsonNode *
json_boxed_serialize (GType         gboxed_type,
                      gconstpointer boxed)
{
  BoxedTransform *t;

  g_return_val_if_fail (G_TYPE_IS_BOXED (gboxed_type), NULL);
  g_return_val_if_fail (G_TYPE_IS_ABSTRACT (gboxed_type) == FALSE, NULL);
  g_return_val_if_fail (boxed != NULL, NULL);

  t = lookup_boxed_transform (boxed_serialize, gboxed_type, -1);
  if (t != NULL && t->serialize != NULL)
    return t->serialize (boxed);

  return NULL;
}

/**
 * json_boxed_deserialize:
 * @gboxed_type: a boxed type
 * @node: a node
 *
 * Deserializes the given [struct@Json.Node] into a `GBoxed` of the given type.
 *
 * Return value: (transfer full): the newly allocated boxed data
 *
 * Since: 0.10
 */
gpointer
json_boxed_deserialize (GType     gboxed_type,
                        JsonNode *node)
{
  JsonNodeType node_type;
  BoxedTransform *t;

  g_return_val_if_fail (G_TYPE_IS_BOXED (gboxed_type), NULL);
  g_return_val_if_fail (G_TYPE_IS_ABSTRACT (gboxed_type) == FALSE, NULL);
  g_return_val_if_fail (node != NULL, NULL);

  node_type = json_node_get_node_type (node);

  t = lookup_boxed_transform (boxed_deserialize, gboxed_type, node_type);
  if (t != NULL && t->deserialize != NULL)
    return t->deserialize (node);

  return NULL;
}
