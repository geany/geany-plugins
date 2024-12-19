/* json-value.c - JSON value container
 * 
 * This file is part of JSON-GLib
 * Copyright (C) 2012  Emmanuele Bassi <ebassi@gnome.org>
 * Copyright (C) 2015 Collabora Ltd.
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

#include "json-types-private.h"

const gchar *
json_value_type_get_name (JsonValueType value_type)
{
  switch (value_type)
    {
    case JSON_VALUE_INVALID:
      return "Unset";

    case JSON_VALUE_INT:
      return "Integer";

    case JSON_VALUE_DOUBLE:
      return "Floating Point";

    case JSON_VALUE_BOOLEAN:
      return "Boolean";

    case JSON_VALUE_STRING:
      return "String";

    case JSON_VALUE_NULL:
      return "Null";
    }

  return "Undefined";
}

GType
json_value_type (const JsonValue *value)
{
  switch (value->type)
    {
    case JSON_VALUE_INVALID:
      return G_TYPE_INVALID;

    case JSON_VALUE_INT:
      return G_TYPE_INT64;

    case JSON_VALUE_DOUBLE:
      return G_TYPE_DOUBLE;

    case JSON_VALUE_BOOLEAN:
      return G_TYPE_BOOLEAN;

    case JSON_VALUE_STRING:
      return G_TYPE_STRING;

    case JSON_VALUE_NULL:
      return G_TYPE_INVALID;
    }

  return G_TYPE_INVALID;
}

JsonValue *
json_value_alloc (void)
{
  JsonValue *res = g_slice_new0 (JsonValue);

  res->ref_count = 1;

  return res;
}

JsonValue *
json_value_init (JsonValue     *value,
                 JsonValueType  value_type)
{
  g_return_val_if_fail (value != NULL, NULL);

  if (value->type != JSON_VALUE_INVALID)
    json_value_unset (value);

  value->type = value_type;

  return value;
}

JsonValue *
json_value_ref (JsonValue *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  value->ref_count++;

  return value;
}

void
json_value_unref (JsonValue *value)
{
  g_return_if_fail (value != NULL);

  if (--value->ref_count == 0)
    json_value_free (value);
}

void
json_value_unset (JsonValue *value)
{
  g_return_if_fail (value != NULL);

  switch (value->type)
    {
    case JSON_VALUE_INVALID:
      break;

    case JSON_VALUE_INT:
      value->data.v_int = 0;
      break;

    case JSON_VALUE_DOUBLE:
      value->data.v_double = 0.0;
      break;

    case JSON_VALUE_BOOLEAN:
      value->data.v_bool = FALSE;
      break;

    case JSON_VALUE_STRING:
      g_free (value->data.v_str);
      value->data.v_str = NULL;
      break;

    case JSON_VALUE_NULL:
      break;
    }
}

void
json_value_free (JsonValue *value)
{
  if (G_LIKELY (value != NULL))
    {
      json_value_unset (value);
      g_slice_free (JsonValue, value);
    }
}

/*< private >
 * json_value_seal:
 * @value: a JSON scalar value
 *
 * Seals the value, making it immutable to further changes.
 *
 * If the value is already immutable, this is a no-op.
 */
void
json_value_seal (JsonValue *value)
{
  g_return_if_fail (JSON_VALUE_IS_VALID (value));
  g_return_if_fail (value->ref_count > 0);

  value->immutable = TRUE;
}

guint
json_value_hash (gconstpointer key)
{
  JsonValue *value;
  guint value_hash;
  guint type_hash;

  value = (JsonValue *) key;

  /* Hash the type and value separately.
   * Use the top 3 bits to store the type. */
  type_hash = value->type << (sizeof (guint) * 8 - 3);

  switch (value->type)
    {
    case JSON_VALUE_NULL:
      value_hash = 0;
      break;
    case JSON_VALUE_BOOLEAN:
      value_hash = json_value_get_boolean (value) ? 1 : 0;
      break;
    case JSON_VALUE_STRING:
      value_hash = json_string_hash (json_value_get_string (value));
      break;
    case JSON_VALUE_INT: {
      gint64 v = json_value_get_int (value);
      value_hash = g_int64_hash (&v);
      break;
    }
    case JSON_VALUE_DOUBLE: {
      gdouble v = json_value_get_double (value);
      value_hash = g_double_hash (&v);
      break;
    }
    case JSON_VALUE_INVALID:
    default:
      g_assert_not_reached ();
    }

  /* Mask out the top 3 bits of the @value_hash. */
  value_hash &= ~(7u << (sizeof (guint) * 8 - 3));

  return (type_hash | value_hash);
}

#define _JSON_VALUE_DEFINE_SET(Type,EType,CType,VField) \
void \
json_value_set_##Type (JsonValue *value, CType VField) \
{ \
  g_return_if_fail (JSON_VALUE_IS_VALID (value)); \
  g_return_if_fail (JSON_VALUE_HOLDS (value, JSON_VALUE_##EType)); \
  g_return_if_fail (!value->immutable); \
\
  value->data.VField = VField; \
\
}

#define _JSON_VALUE_DEFINE_GET(Type,EType,CType,VField) \
CType \
json_value_get_##Type (const JsonValue *value) \
{ \
  g_return_val_if_fail (JSON_VALUE_IS_VALID (value), 0); \
  g_return_val_if_fail (JSON_VALUE_HOLDS (value, JSON_VALUE_##EType), 0); \
\
  return value->data.VField; \
}

#define _JSON_VALUE_DEFINE_SET_GET(Type,EType,CType,VField) \
_JSON_VALUE_DEFINE_SET(Type,EType,CType,VField) \
_JSON_VALUE_DEFINE_GET(Type,EType,CType,VField)

_JSON_VALUE_DEFINE_SET_GET(int, INT, gint64, v_int)

_JSON_VALUE_DEFINE_SET_GET(double, DOUBLE, gdouble, v_double)

_JSON_VALUE_DEFINE_SET_GET(boolean, BOOLEAN, gboolean, v_bool)

void
json_value_set_string (JsonValue *value,
                       const gchar *v_str)
{
  g_return_if_fail (JSON_VALUE_IS_VALID (value));
  g_return_if_fail (JSON_VALUE_HOLDS_STRING (value));
  g_return_if_fail (!value->immutable);

  g_free (value->data.v_str);
  value->data.v_str = g_strdup (v_str);
}

_JSON_VALUE_DEFINE_GET(string, STRING, const gchar *, v_str)

#undef _JSON_VALUE_DEFINE_SET_GET
#undef _JSON_VALUE_DEFINE_GET
#undef _JSON_VALUE_DEFINE_SET
