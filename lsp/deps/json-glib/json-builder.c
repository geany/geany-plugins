/* json-generator.c - JSON tree builder
 *
 * This file is part of JSON-GLib
 * Copyright (C) 2010  Luca Bruno <lethalman88@gmail.com>
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
 *   Luca Bruno  <lethalman88@gmail.com>
 *   Philip Withnall  <philip.withnall@collabora.co.uk>
 */

/**
 * JsonBuilder:
 *
 * `JsonBuilder` provides an object for generating a JSON tree.
 *
 * The root of the JSON tree can be either a [struct@Json.Object] or a [struct@Json.Array].
 * Thus the first call must necessarily be either
 * [method@Json.Builder.begin_object] or [method@Json.Builder.begin_array].
 *
 * For convenience to language bindings, most `JsonBuilder` method return the
 * instance, making it easy to chain function calls.
 *
 * ## Using `JsonBuilder`
 *
 * ```c
 * g_autoptr(JsonBuilder) builder = json_builder_new ();
 *
 * json_builder_begin_object (builder);
 *
 * json_builder_set_member_name (builder, "url");
 * json_builder_add_string_value (builder, "http://www.gnome.org/img/flash/two-thirty.png");
 *
 * json_builder_set_member_name (builder, "size");
 * json_builder_begin_array (builder);
 * json_builder_add_int_value (builder, 652);
 * json_builder_add_int_value (builder, 242);
 * json_builder_end_array (builder);
 *
 * json_builder_end_object (builder);
 *
 * g_autoptr(JsonNode) root = json_builder_get_root (builder);
 *
 * g_autoptr(JsonGenerator) gen = json_generator_new ();
 * json_generator_set_root (gen, root);
 * g_autofree char *str = json_generator_to_data (gen, NULL);
 *
 * // str now contains the following JSON data
 * // { "url" : "http://www.gnome.org/img/flash/two-thirty.png", "size" : [ 652, 242 ] }
 * ```
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "json-types-private.h"

#include "json-builder.h"

struct _JsonBuilderPrivate
{
  GQueue *stack;
  JsonNode *root;
  gboolean immutable;
};

enum
{
  PROP_IMMUTABLE = 1,
  PROP_LAST
};

static GParamSpec *builder_props[PROP_LAST] = { NULL, };

typedef enum
{
  JSON_BUILDER_MODE_OBJECT,
  JSON_BUILDER_MODE_ARRAY,
  JSON_BUILDER_MODE_MEMBER
} JsonBuilderMode;

typedef struct
{
  JsonBuilderMode mode;

  union
  {
    JsonObject *object;
    JsonArray *array;
  } data;
  gchar *member_name;
} JsonBuilderState;

static void
json_builder_state_free (JsonBuilderState *state)
{
  if (G_LIKELY (state))
    {
      switch (state->mode)
        {
        case JSON_BUILDER_MODE_OBJECT:
        case JSON_BUILDER_MODE_MEMBER:
          json_object_unref (state->data.object);
          g_free (state->member_name);
          state->data.object = NULL;
          state->member_name = NULL;
          break;

        case JSON_BUILDER_MODE_ARRAY:
          json_array_unref (state->data.array);
          state->data.array = NULL;
          break;

        default:
          g_assert_not_reached ();
        }

      g_slice_free (JsonBuilderState, state);
    }
}

G_DEFINE_TYPE_WITH_PRIVATE (JsonBuilder, json_builder, G_TYPE_OBJECT)

static void
json_builder_free_all_state (JsonBuilder *builder)
{
  JsonBuilderState *state;

  while (!g_queue_is_empty (builder->priv->stack))
    {
      state = g_queue_pop_head (builder->priv->stack);
      json_builder_state_free (state);
    }

  if (builder->priv->root)
    {
      json_node_unref (builder->priv->root);
      builder->priv->root = NULL;
    }
}

static void
json_builder_finalize (GObject *gobject)
{
  JsonBuilderPrivate *priv = json_builder_get_instance_private ((JsonBuilder *) gobject);

  json_builder_free_all_state (JSON_BUILDER (gobject));

  g_queue_free (priv->stack);
  priv->stack = NULL;

  G_OBJECT_CLASS (json_builder_parent_class)->finalize (gobject);
}

static void
json_builder_set_property (GObject      *gobject,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  JsonBuilderPrivate *priv = JSON_BUILDER (gobject)->priv;

  switch (prop_id)
    {
    case PROP_IMMUTABLE:
      /* Construct-only. */
      priv->immutable = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
json_builder_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  JsonBuilderPrivate *priv = JSON_BUILDER (gobject)->priv;

  switch (prop_id)
    {
    case PROP_IMMUTABLE:
      g_value_set_boolean (value, priv->immutable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
json_builder_class_init (JsonBuilderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /**
   * JsonBuilder:immutable:
   *
   * Whether the tree should be immutable when created.
   *
   * Making the output immutable on creation avoids the expense
   * of traversing it to make it immutable later.
   *
   * Since: 1.2
   */
  builder_props[PROP_IMMUTABLE] =
    g_param_spec_boolean ("immutable",
                          "Immutable Output",
                          "Whether the builder output is immutable.",
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  gobject_class->set_property = json_builder_set_property;
  gobject_class->get_property = json_builder_get_property;
  gobject_class->finalize = json_builder_finalize;

  g_object_class_install_properties (gobject_class, PROP_LAST, builder_props);
}

static void
json_builder_init (JsonBuilder *builder)
{
  JsonBuilderPrivate *priv = json_builder_get_instance_private (builder);

  builder->priv = priv;

  priv->stack = g_queue_new ();
  priv->root = NULL;
}

static inline JsonBuilderMode
json_builder_current_mode (JsonBuilder *builder)
{
  JsonBuilderState *state = g_queue_peek_head (builder->priv->stack);
  return state->mode;
}

static inline gboolean
json_builder_is_valid_add_mode (JsonBuilder *builder)
{
  JsonBuilderMode mode = json_builder_current_mode (builder);
  return mode == JSON_BUILDER_MODE_MEMBER || mode == JSON_BUILDER_MODE_ARRAY;
}

/**
 * json_builder_new:
 *
 * Creates a new `JsonBuilder`.
 *
 * You can use this object to generate a JSON tree and obtain the root node.
 *
 * Return value: the newly created builder instance
 */
JsonBuilder *
json_builder_new (void)
{
  return g_object_new (JSON_TYPE_BUILDER, NULL);
}

/**
 * json_builder_new_immutable: (constructor)
 *
 * Creates a new, immutable `JsonBuilder` instance.
 *
 * It is equivalent to setting the [property@Json.Builder:immutable] property
 * set to `TRUE` at construction time.
 *
 * Since: 1.2
 * Returns: (transfer full): the newly create builder instance
 */
JsonBuilder *
json_builder_new_immutable (void)
{
  return g_object_new (JSON_TYPE_BUILDER, "immutable", TRUE, NULL);
}

/**
 * json_builder_get_root:
 * @builder: a builder
 *
 * Returns the root of the currently constructed tree.
 *
 * if the build is incomplete (ie: if there are any opened objects, or any
 * open object members and array elements) then this function will return
 * `NULL`.
 *
 * Return value: (nullable) (transfer full): the root node
 */
JsonNode *
json_builder_get_root (JsonBuilder *builder)
{
  JsonNode *root = NULL;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);

  if (builder->priv->root)
    root = json_node_copy (builder->priv->root);

  /* Sanity check. */
  g_assert (!builder->priv->immutable ||
            root == NULL ||
            json_node_is_immutable (root));

  return root;
}

/**
 * json_builder_reset:
 * @builder: a builder
 *
 * Resets the state of the builder back to its initial state.
 */
void
json_builder_reset (JsonBuilder *builder)
{
  g_return_if_fail (JSON_IS_BUILDER (builder));

  json_builder_free_all_state (builder);
}

/**
 * json_builder_begin_object:
 * @builder: a builder
 *
 * Opens an object inside the given builder.
 *
 * You can add a new member to the object by using [method@Json.Builder.set_member_name],
 * followed by [method@Json.Builder.add_value].
 *
 * Once you added all members to the object, you must call [method@Json.Builder.end_object]
 * to close the object.
 *
 * If the builder is in an inconsistent state, this function will return `NULL`.
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_begin_object (JsonBuilder *builder)
{
  JsonObject *object;
  JsonBuilderState *state;
  JsonBuilderState *cur_state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (builder->priv->root == NULL, NULL);
  g_return_val_if_fail (g_queue_is_empty (builder->priv->stack) || json_builder_is_valid_add_mode (builder), NULL);

  object = json_object_new ();
  cur_state = g_queue_peek_head (builder->priv->stack);
  if (cur_state)
    {
      switch (cur_state->mode)
        {
        case JSON_BUILDER_MODE_ARRAY:
          json_array_add_object_element (cur_state->data.array, json_object_ref (object));
          break;

        case JSON_BUILDER_MODE_MEMBER:
          json_object_set_object_member (cur_state->data.object, cur_state->member_name, json_object_ref (object));
          g_free (cur_state->member_name);
          cur_state->member_name = NULL;
          cur_state->mode = JSON_BUILDER_MODE_OBJECT;
          break;

        default:
          g_assert_not_reached ();
        }
    }

  state = g_slice_new (JsonBuilderState);
  state->data.object = object;
  state->member_name = NULL;
  state->mode = JSON_BUILDER_MODE_OBJECT;
  g_queue_push_head (builder->priv->stack, state);

  return builder;
}

/**
 * json_builder_end_object:
 * @builder: a builder
 *
 * Closes the object inside the given builder that was opened by the most
 * recent call to [method@Json.Builder.begin_object].
 *
 * This function cannot be called after [method@Json.Builder.set_member_name].
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_end_object (JsonBuilder *builder)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_current_mode (builder) == JSON_BUILDER_MODE_OBJECT, NULL);

  state = g_queue_pop_head (builder->priv->stack);

  if (builder->priv->immutable)
    json_object_seal (state->data.object);

  if (g_queue_is_empty (builder->priv->stack))
    {
      builder->priv->root = json_node_new (JSON_NODE_OBJECT);
      json_node_take_object (builder->priv->root, json_object_ref (state->data.object));

      if (builder->priv->immutable)
        json_node_seal (builder->priv->root);
    }

  json_builder_state_free (state);

  return builder;
}

/**
 * json_builder_begin_array:
 * @builder: a builder
 *
 * Opens an array inside the given builder.
 *
 * You can add a new element to the array by using [method@Json.Builder.add_value].
 *
 * Once you added all elements to the array, you must call
 * [method@Json.Builder.end_array] to close the array.
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_begin_array (JsonBuilder *builder)
{
  JsonArray *array;
  JsonBuilderState *state;
  JsonBuilderState *cur_state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (builder->priv->root == NULL, NULL);
  g_return_val_if_fail (g_queue_is_empty (builder->priv->stack) || json_builder_is_valid_add_mode (builder), NULL);

  array = json_array_new ();
  cur_state = g_queue_peek_head (builder->priv->stack);
  if (cur_state)
    {
      switch (cur_state->mode)
        {
        case JSON_BUILDER_MODE_ARRAY:
          json_array_add_array_element (cur_state->data.array, json_array_ref (array));
          break;

        case JSON_BUILDER_MODE_MEMBER:
          json_object_set_array_member (cur_state->data.object, cur_state->member_name, json_array_ref (array));
          g_free (cur_state->member_name);
          cur_state->member_name = NULL;
          cur_state->mode = JSON_BUILDER_MODE_OBJECT;
          break;

        default:
          g_assert_not_reached ();
        }
    }

  state = g_slice_new (JsonBuilderState);
  state->data.array = array;
  state->mode = JSON_BUILDER_MODE_ARRAY;
  g_queue_push_head (builder->priv->stack, state);

  return builder;
}

/**
 * json_builder_end_array:
 * @builder: a builder
 *
 * Closes the array inside the given builder that was opened by the most
 * recent call to [method@Json.Builder.begin_array].
 *
 * This function cannot be called after [method@Json.Builder.set_member_name].
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_end_array (JsonBuilder *builder)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_current_mode (builder) == JSON_BUILDER_MODE_ARRAY, NULL);

  state = g_queue_pop_head (builder->priv->stack);

  if (builder->priv->immutable)
    json_array_seal (state->data.array);

  if (g_queue_is_empty (builder->priv->stack))
    {
      builder->priv->root = json_node_new (JSON_NODE_ARRAY);
      json_node_take_array (builder->priv->root, json_array_ref (state->data.array));

      if (builder->priv->immutable)
        json_node_seal (builder->priv->root);
    }

  json_builder_state_free (state);

  return builder;
}

/**
 * json_builder_set_member_name:
 * @builder: a builder
 * @member_name: the name of the member
 *
 * Sets the name of the member in an object.
 *
 * This function must be followed by of these functions:
 *
 *  - [method@Json.Builder.add_value], to add a scalar value to the member
 *  - [method@Json.Builder.begin_object], to add an object to the member
 *  - [method@Json.Builder.begin_array], to add an array to the member
 *
 * This function can only be called within an open object.
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_set_member_name (JsonBuilder *builder,
                              const gchar *member_name)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (member_name != NULL, NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_current_mode (builder) == JSON_BUILDER_MODE_OBJECT, NULL);

  state = g_queue_peek_head (builder->priv->stack);
  state->member_name = g_strdup (member_name);
  state->mode = JSON_BUILDER_MODE_MEMBER;

  return builder;
}

/**
 * json_builder_add_value:
 * @builder: a builder
 * @node: (transfer full): the value of the member or element
 *
 * Adds a value to the currently open object member or array.
 *
 * If called after [method@Json.Builder.set_member_name], sets the given node
 * as the value of the current member in the open object; otherwise, the node
 * is appended to the elements of the open array.
 *
 * The builder will take ownership of the node.
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_add_value (JsonBuilder *builder,
                        JsonNode    *node)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_is_valid_add_mode (builder), NULL);

  state = g_queue_peek_head (builder->priv->stack);

  if (builder->priv->immutable)
    json_node_seal (node);

  switch (state->mode)
    {
    case JSON_BUILDER_MODE_MEMBER:
      json_object_set_member (state->data.object, state->member_name, node);
      g_free (state->member_name);
      state->member_name = NULL;
      state->mode = JSON_BUILDER_MODE_OBJECT;
      break;

    case JSON_BUILDER_MODE_ARRAY:
      json_array_add_element (state->data.array, node);
      break;

    default:
      g_assert_not_reached ();
    }

  return builder;
}

/**
 * json_builder_add_int_value:
 * @builder: a builder
 * @value: the value of the member or element
 *
 * Adds an integer value to the currently open object member or array.
 *
 * If called after [method@Json.Builder.set_member_name], sets the given value
 * as the value of the current member in the open object; otherwise, the value
 * is appended to the elements of the open array.
 *
 * See also: [method@Json.Builder.add_value]
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_add_int_value (JsonBuilder *builder,
                            gint64       value)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_is_valid_add_mode (builder), NULL);

  state = g_queue_peek_head (builder->priv->stack);
  switch (state->mode)
    {
    case JSON_BUILDER_MODE_MEMBER:
      json_object_set_int_member (state->data.object, state->member_name, value);
      g_free (state->member_name);
      state->member_name = NULL;
      state->mode = JSON_BUILDER_MODE_OBJECT;
      break;

    case JSON_BUILDER_MODE_ARRAY:
      json_array_add_int_element (state->data.array, value);
      break;

    default:
      g_assert_not_reached ();
    }

  return builder;
}

/**
 * json_builder_add_double_value:
 * @builder: a builder
 * @value: the value of the member or element
 *
 * Adds a floating point value to the currently open object member or array.
 *
 * If called after [method@Json.Builder.set_member_name], sets the given value
 * as the value of the current member in the open object; otherwise, the value
 * is appended to the elements of the open array.
 *
 * See also: [method@Json.Builder.add_value]
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_add_double_value (JsonBuilder *builder,
                               gdouble      value)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_is_valid_add_mode (builder), NULL);

  state = g_queue_peek_head (builder->priv->stack);

  switch (state->mode)
    {
    case JSON_BUILDER_MODE_MEMBER:
      json_object_set_double_member (state->data.object, state->member_name, value);
      g_free (state->member_name);
      state->member_name = NULL;
      state->mode = JSON_BUILDER_MODE_OBJECT;
      break;

    case JSON_BUILDER_MODE_ARRAY:
      json_array_add_double_element (state->data.array, value);
      break;

    default:
      g_assert_not_reached ();
    }

  return builder;
}

/**
 * json_builder_add_boolean_value:
 * @builder: a builder
 * @value: the value of the member or element
 *
 * Adds a boolean value to the currently open object member or array.
 *
 * If called after [method@Json.Builder.set_member_name], sets the given value
 * as the value of the current member in the open object; otherwise, the value
 * is appended to the elements of the open array.
 *
 * See also: [method@Json.Builder.add_value]
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_add_boolean_value (JsonBuilder *builder,
                                gboolean     value)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_is_valid_add_mode (builder), NULL);

  state = g_queue_peek_head (builder->priv->stack);

  switch (state->mode)
    {
    case JSON_BUILDER_MODE_MEMBER:
      json_object_set_boolean_member (state->data.object, state->member_name, value);
      g_free (state->member_name);
      state->member_name = NULL;
      state->mode = JSON_BUILDER_MODE_OBJECT;
      break;

    case JSON_BUILDER_MODE_ARRAY:
      json_array_add_boolean_element (state->data.array, value);
      break;

    default:
      g_assert_not_reached ();
    }

  return builder;
}

/**
 * json_builder_add_string_value:
 * @builder: a builder
 * @value: the value of the member or element
 *
 * Adds a string value to the currently open object member or array.
 *
 * If called after [method@Json.Builder.set_member_name], sets the given value
 * as the value of the current member in the open object; otherwise, the value
 * is appended to the elements of the open array.
 *
 * See also: [method@Json.Builder.add_value]
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_add_string_value (JsonBuilder *builder,
                               const gchar *value)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_is_valid_add_mode (builder), NULL);

  state = g_queue_peek_head (builder->priv->stack);

  switch (state->mode)
    {
    case JSON_BUILDER_MODE_MEMBER:
      json_object_set_string_member (state->data.object, state->member_name, value);
      g_free (state->member_name);
      state->member_name = NULL;
      state->mode = JSON_BUILDER_MODE_OBJECT;
      break;

    case JSON_BUILDER_MODE_ARRAY:
      json_array_add_string_element (state->data.array, value);
      break;

    default:
      g_assert_not_reached ();
    }

  return builder;
}

/**
 * json_builder_add_null_value:
 * @builder: a builder
 *
 * Adds a null value to the currently open object member or array.
 *
 * If called after [method@Json.Builder.set_member_name], sets the given value
 * as the value of the current member in the open object; otherwise, the value
 * is appended to the elements of the open array.
 *
 * See also: [method@Json.Builder.add_value]
 *
 * Return value: (nullable) (transfer none): the builder instance
 */
JsonBuilder *
json_builder_add_null_value (JsonBuilder *builder)
{
  JsonBuilderState *state;

  g_return_val_if_fail (JSON_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (!g_queue_is_empty (builder->priv->stack), NULL);
  g_return_val_if_fail (json_builder_is_valid_add_mode (builder), NULL);

  state = g_queue_peek_head (builder->priv->stack);

  switch (state->mode)
    {
    case JSON_BUILDER_MODE_MEMBER:
      json_object_set_null_member (state->data.object, state->member_name);
      g_free (state->member_name);
      state->member_name = NULL;
      state->mode = JSON_BUILDER_MODE_OBJECT;
      break;

    case JSON_BUILDER_MODE_ARRAY:
      json_array_add_null_element (state->data.array);
      break;

    default:
      g_assert_not_reached ();
    }

  return builder;
}
