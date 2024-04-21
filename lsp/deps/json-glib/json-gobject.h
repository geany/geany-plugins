/* json-gobject.h - JSON GObject integration
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

#include <json-glib/json-types.h>

G_BEGIN_DECLS

#define JSON_TYPE_SERIALIZABLE                  (json_serializable_get_type ())
#define JSON_SERIALIZABLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), JSON_TYPE_SERIALIZABLE, JsonSerializable))
#define JSON_IS_SERIALIZABLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JSON_TYPE_SERIALIZABLE))
#define JSON_SERIALIZABLE_GET_IFACE(obj)        (G_TYPE_INSTANCE_GET_INTERFACE ((obj), JSON_TYPE_SERIALIZABLE, JsonSerializableIface))

typedef struct _JsonSerializable        JsonSerializable; /* dummy */
typedef struct _JsonSerializableIface   JsonSerializableIface;

/**
 * JsonSerializableIface:
 * @serialize_property: virtual function for serializing an object property
 *   into JSON
 * @deserialize_property: virtual function for deserializing JSON
 *   into an object property
 * @find_property: virtual function for finding a property definition using
 *   its name
 * @list_properties: virtual function for listing the installed property
 *   definitions
 * @set_property: virtual function for setting a property
 * @get_property: virtual function for getting a property
 *
 * Interface that allows serializing and deserializing object instances
 * with properties storing complex data types.
 *
 * The [func@Json.gobject_from_data] and [func@Json.gobject_to_data]
 * functions will check if the passed object type implements this interface,
 * so it can also be used to override the default property serialization
 * sequence.
 */
struct _JsonSerializableIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  JsonNode *(* serialize_property)   (JsonSerializable *serializable,
                                      const gchar      *property_name,
                                      const GValue     *value,
                                      GParamSpec       *pspec);
  gboolean  (* deserialize_property) (JsonSerializable *serializable,
                                      const gchar      *property_name,
                                      GValue           *value,
                                      GParamSpec       *pspec,
                                      JsonNode         *property_node);

  GParamSpec * (* find_property)       (JsonSerializable *serializable,
                                        const char       *name);
  GParamSpec **(* list_properties)     (JsonSerializable *serializable,
                                        guint            *n_pspecs);
  void         (* set_property)        (JsonSerializable *serializable,
                                        GParamSpec       *pspec,
                                        const GValue     *value);
  void         (* get_property)        (JsonSerializable *serializable,
                                        GParamSpec       *pspec,
                                        GValue           *value);
};

JSON_AVAILABLE_IN_1_0
GType json_serializable_get_type (void) G_GNUC_CONST;

JSON_AVAILABLE_IN_1_0
JsonNode *json_serializable_serialize_property           (JsonSerializable *serializable,
                                                          const gchar      *property_name,
                                                          const GValue     *value,
                                                          GParamSpec       *pspec);
JSON_AVAILABLE_IN_1_0
gboolean  json_serializable_deserialize_property         (JsonSerializable *serializable,
                                                          const gchar      *property_name,
                                                          GValue           *value,
                                                          GParamSpec       *pspec,
                                                          JsonNode         *property_node);

JSON_AVAILABLE_IN_1_0
GParamSpec *    json_serializable_find_property         (JsonSerializable *serializable,
                                                         const char       *name);
JSON_AVAILABLE_IN_1_0
GParamSpec **   json_serializable_list_properties       (JsonSerializable *serializable,
                                                         guint            *n_pspecs);
JSON_AVAILABLE_IN_1_0
void            json_serializable_set_property          (JsonSerializable *serializable,
                                                         GParamSpec       *pspec,
                                                         const GValue     *value);
JSON_AVAILABLE_IN_1_0
void            json_serializable_get_property          (JsonSerializable *serializable,
                                                         GParamSpec       *pspec,
                                                         GValue           *value);

JSON_AVAILABLE_IN_1_0
JsonNode *json_serializable_default_serialize_property   (JsonSerializable *serializable,
                                                          const gchar      *property_name,
                                                          const GValue     *value,
                                                          GParamSpec       *pspec);
JSON_AVAILABLE_IN_1_0
gboolean  json_serializable_default_deserialize_property (JsonSerializable *serializable,
                                                          const gchar      *property_name,
                                                          GValue           *value,
                                                          GParamSpec       *pspec,
                                                          JsonNode         *property_node);

/**
 * JsonBoxedSerializeFunc:
 * @boxed: a boxed data structure
 *
 * Serializes the passed `GBoxed` and stores it inside a `JsonNode`, for instance:
 *
 * ```c
 * static JsonNode *
 * my_point_serialize (gconstpointer boxed)
 * {
 *   const MyPoint *point = boxed;
 *
 *   g_autoptr(JsonBuilder) builder = json_builder_new ();
 *
 *   json_builder_begin_object (builder);
 *   json_builder_set_member_name (builder, "x");
 *   json_builder_add_double_value (builder, point->x);
 *   json_builder_set_member_name (builder, "y");
 *   json_builder_add_double_value (builder, point->y);
 *   json_builder_end_object (builder);
 *
 *   return json_builder_get_root (builder);
 * }
 * ```
 *
 * Return value: the newly created JSON node tree representing the boxed data
 *
 * Since: 0.10
 */
typedef JsonNode *(* JsonBoxedSerializeFunc) (gconstpointer boxed);

/**
 * JsonBoxedDeserializeFunc:
 * @node: a node tree representing a boxed data
 *
 * Deserializes the contents of the passed `JsonNode` into a `GBoxed`, for instance:
 *
 * ```c
 * static gpointer
 * my_point_deserialize (JsonNode *node)
 * {
 *   double x = 0.0, y = 0.0;
 *
 *   if (JSON_NODE_HOLDS_ARRAY (node))
 *     {
 *       JsonArray *array = json_node_get_array (node);
 *
 *       if (json_array_get_length (array) == 2)
 *         {
 *           x = json_array_get_double_element (array, 0);
 *           y = json_array_get_double_element (array, 1);
 *         }
 *     }
 *   else if (JSON_NODE_HOLDS_OBJECT (node))
 *     {
 *       JsonObject *obj = json_node_get_object (node);
 *
 *       x = json_object_get_double_member_with_default (obj, "x", 0.0);
 *       y = json_object_get_double_member_with_default (obj, "y", 0.0);
 *     }
 *
 *   // my_point_new() is defined elsewhere
 *   return my_point_new (x, y);
 * }
 * ```
 *
 * Return value: the newly created boxed structure
 *
 * Since: 0.10
 */
typedef gpointer (* JsonBoxedDeserializeFunc) (JsonNode *node);

JSON_AVAILABLE_IN_1_0
void      json_boxed_register_serialize_func   (GType                    gboxed_type,
                                                JsonNodeType             node_type,
                                                JsonBoxedSerializeFunc   serialize_func);
JSON_AVAILABLE_IN_1_0
void      json_boxed_register_deserialize_func (GType                    gboxed_type,
                                                JsonNodeType             node_type,
                                                JsonBoxedDeserializeFunc deserialize_func);
JSON_AVAILABLE_IN_1_0
gboolean  json_boxed_can_serialize             (GType                    gboxed_type,
                                                JsonNodeType            *node_type);
JSON_AVAILABLE_IN_1_0
gboolean  json_boxed_can_deserialize           (GType                    gboxed_type,
                                                JsonNodeType             node_type);
JSON_AVAILABLE_IN_1_0
JsonNode *json_boxed_serialize                 (GType                    gboxed_type,
                                                gconstpointer            boxed);
JSON_AVAILABLE_IN_1_0
gpointer  json_boxed_deserialize               (GType                    gboxed_type,
                                                JsonNode                *node);

JSON_AVAILABLE_IN_1_0
JsonNode *json_gobject_serialize               (GObject                 *gobject);
JSON_AVAILABLE_IN_1_0
GObject * json_gobject_deserialize             (GType                    gtype,
                                                JsonNode                *node);

JSON_AVAILABLE_IN_1_0
GObject * json_gobject_from_data               (GType                    gtype,
                                                const gchar             *data,
                                                gssize                   length,
                                                GError                 **error);
JSON_AVAILABLE_IN_1_0
gchar *   json_gobject_to_data                 (GObject                 *gobject,
                                                gsize                   *length);

JSON_DEPRECATED_IN_1_0_FOR(json_gobject_from_data)
GObject * json_construct_gobject               (GType                    gtype,
                                                const gchar             *data,
                                                gsize                    length,
                                                GError                 **error);
JSON_DEPRECATED_IN_1_0_FOR(json_gobject_to_data)
gchar *   json_serialize_gobject               (GObject                 *gobject,
                                                gsize                   *length) G_GNUC_MALLOC;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JsonSerializable, g_object_unref)
#endif

G_END_DECLS
