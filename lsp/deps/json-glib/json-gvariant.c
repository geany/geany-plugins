/* json-gvariant.c - JSON GVariant integration
 *
 * This file is part of JSON-GLib
 * Copyright (C) 2007  OpenedHand Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * Author:
 *   Eduardo Lima Mitev  <elima@igalia.com>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "json-gvariant.h"

#include "json-generator.h"
#include "json-parser.h"
#include "json-types-private.h"

/* custom extension to the GVariantClass enumeration to differentiate
 * a single dictionary entry from an array of dictionary entries
 */
#define JSON_G_VARIANT_CLASS_DICTIONARY 'c'

typedef void (* GVariantForeachFunc) (GVariant *variant_child,
                                      gpointer  user_data);

static GVariant * json_to_gvariant_recurse (JsonNode      *json_node,
                                            const gchar  **signature,
                                            GError       **error);

/* ========================================================================== */
/* GVariant to JSON */
/* ========================================================================== */

static void
gvariant_foreach (GVariant            *variant,
                  GVariantForeachFunc  func,
                  gpointer             user_data)
{
  GVariantIter iter;
  GVariant *variant_child;

  g_variant_iter_init (&iter, variant);
  while ((variant_child = g_variant_iter_next_value (&iter)) != NULL)
    {
      func (variant_child, user_data);
      g_variant_unref (variant_child);
    }
}

static void
gvariant_to_json_array_foreach (GVariant *variant_child,
                                gpointer  user_data)
{
  JsonArray *array = user_data;
  JsonNode *json_child;

  json_child = json_gvariant_serialize (variant_child);
  json_array_add_element (array, json_child);
}

static JsonNode *
gvariant_to_json_array (GVariant *variant)
{
  JsonArray *array;
  JsonNode *json_node;

  array = json_array_new ();
  json_node = json_node_new (JSON_NODE_ARRAY);
  json_node_set_array (json_node, array);
  json_array_unref (array);

  gvariant_foreach (variant,
                    gvariant_to_json_array_foreach,
                    array);

  return json_node;
}

static gchar *
gvariant_simple_to_string (GVariant *variant)
{
  GVariantClass class;
  gchar *str;

  class = g_variant_classify (variant);
  switch (class)
    {
    case G_VARIANT_CLASS_BOOLEAN:
      if (g_variant_get_boolean (variant))
        str = g_strdup ("true");
      else
        str = g_strdup ("false");
      break;

    case G_VARIANT_CLASS_BYTE:
      str = g_strdup_printf ("%u", g_variant_get_byte (variant));
      break;
    case G_VARIANT_CLASS_INT16:
      str = g_strdup_printf ("%d", g_variant_get_int16 (variant));
      break;
    case G_VARIANT_CLASS_UINT16:
      str = g_strdup_printf ("%u", g_variant_get_uint16 (variant));
      break;
    case G_VARIANT_CLASS_INT32:
      str = g_strdup_printf ("%d", g_variant_get_int32 (variant));
      break;
    case G_VARIANT_CLASS_UINT32:
      str = g_strdup_printf ("%u", g_variant_get_uint32 (variant));
      break;
    case G_VARIANT_CLASS_INT64:
      str = g_strdup_printf ("%" G_GINT64_FORMAT,
                             g_variant_get_int64 (variant));
      break;
    case G_VARIANT_CLASS_UINT64:
      str = g_strdup_printf ("%" G_GUINT64_FORMAT,
                             g_variant_get_uint64 (variant));
      break;
    case G_VARIANT_CLASS_HANDLE:
      str = g_strdup_printf ("%d", g_variant_get_handle (variant));
      break;

    case G_VARIANT_CLASS_DOUBLE:
      {
        gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

        g_ascii_formatd (buf,
                         G_ASCII_DTOSTR_BUF_SIZE,
                         "%f",
                         g_variant_get_double (variant));

        str = g_strdup (buf);
        break;
      }

    case G_VARIANT_CLASS_STRING:
    case G_VARIANT_CLASS_OBJECT_PATH:
    case G_VARIANT_CLASS_SIGNATURE:
      str = g_strdup (g_variant_get_string (variant, NULL));
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return str;
}

static JsonNode *
gvariant_dict_entry_to_json (GVariant  *variant, gchar **member_name)
{
  GVariant *member;
  GVariant *value;
  JsonNode *json_node;

  member = g_variant_get_child_value (variant, 0);
  *member_name = gvariant_simple_to_string (member);

  value = g_variant_get_child_value (variant, 1);
  json_node = json_gvariant_serialize (value);

  g_variant_unref (member);
  g_variant_unref (value);

  return json_node;
}

static void
gvariant_to_json_object_foreach (GVariant *variant_child, gpointer  user_data)
{
  gchar *member_name;
  JsonNode *json_child;
  JsonObject *object = (JsonObject *) user_data;

  json_child = gvariant_dict_entry_to_json (variant_child, &member_name);
  json_object_set_member (object, member_name, json_child);
  g_free (member_name);
}

static JsonNode *
gvariant_to_json_object (GVariant *variant)
{
  JsonNode *json_node;
  JsonObject *object;

  json_node = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  json_node_set_object (json_node, object);
  json_object_unref (object);

  gvariant_foreach (variant,
                    gvariant_to_json_object_foreach,
                    object);

  return json_node;
}

/**
 * json_gvariant_serialize:
 * @variant: A `GVariant` to convert
 *
 * Converts `variant` to a JSON tree.
 *
 * Return value: (transfer full): the root of the JSON data structure
 *   obtained from `variant`
 *
 * Since: 0.14
 */
JsonNode *
json_gvariant_serialize (GVariant *variant)
{
  JsonNode *json_node = NULL;
  GVariantClass class;

  g_return_val_if_fail (variant != NULL, NULL);

  class = g_variant_classify (variant);

  if (! g_variant_is_container (variant))
    {
      json_node = json_node_new (JSON_NODE_VALUE);

      switch (class)
        {
        case G_VARIANT_CLASS_BOOLEAN:
          json_node_set_boolean (json_node, g_variant_get_boolean (variant));
          break;

        case G_VARIANT_CLASS_BYTE:
          json_node_set_int (json_node, g_variant_get_byte (variant));
          break;
        case G_VARIANT_CLASS_INT16:
          json_node_set_int (json_node, g_variant_get_int16 (variant));
          break;
        case G_VARIANT_CLASS_UINT16:
          json_node_set_int (json_node, g_variant_get_uint16 (variant));
          break;
        case G_VARIANT_CLASS_INT32:
          json_node_set_int (json_node, g_variant_get_int32 (variant));
          break;
        case G_VARIANT_CLASS_UINT32:
          json_node_set_int (json_node, g_variant_get_uint32 (variant));
          break;
        case G_VARIANT_CLASS_INT64:
          json_node_set_int (json_node, g_variant_get_int64 (variant));
          break;
        case G_VARIANT_CLASS_UINT64:
          json_node_set_int (json_node, g_variant_get_uint64 (variant));
          break;
        case G_VARIANT_CLASS_HANDLE:
          json_node_set_int (json_node, g_variant_get_handle (variant));
          break;

        case G_VARIANT_CLASS_DOUBLE:
          json_node_set_double (json_node, g_variant_get_double (variant));
          break;

        case G_VARIANT_CLASS_STRING:
        case G_VARIANT_CLASS_OBJECT_PATH:
        case G_VARIANT_CLASS_SIGNATURE:
          json_node_set_string (json_node, g_variant_get_string (variant, NULL));
          break;

        default:
          break;
        }
    }
  else
    {
      switch (class)
        {
        case G_VARIANT_CLASS_MAYBE:
          {
            GVariant *value;

            value = g_variant_get_maybe (variant);
            if (value == NULL)
              {
                json_node = json_node_new (JSON_NODE_NULL);
              }
            else
              {
                json_node = json_gvariant_serialize (value);
                g_variant_unref (value);
              }

            break;
          }

        case G_VARIANT_CLASS_VARIANT:
          {
            GVariant *value;

            value = g_variant_get_variant (variant);
            json_node = json_gvariant_serialize (value);
            g_variant_unref (value);

            break;
          }

        case G_VARIANT_CLASS_ARRAY:
          {
            const gchar *type;

            type = g_variant_get_type_string (variant);

            if (type[1] == G_VARIANT_CLASS_DICT_ENTRY)
              {
                /* array of dictionary entries => JsonObject */
                json_node = gvariant_to_json_object (variant);
              }
            else
              {
                /* array of anything else => JsonArray */
                json_node = gvariant_to_json_array (variant);
              }

            break;
          }

        case G_VARIANT_CLASS_DICT_ENTRY:
          {
            gchar *member_name;
            JsonObject *object;
            JsonNode *child;

            /* a single dictionary entry => JsonObject */
            json_node = json_node_new (JSON_NODE_OBJECT);
            object = json_object_new ();
            json_node_set_object (json_node, object);
            json_object_unref (object);

            child = gvariant_dict_entry_to_json (variant, &member_name);

            json_object_set_member (object, member_name, child);
            g_free (member_name);

            break;
          }

        case G_VARIANT_CLASS_TUPLE:
          json_node = gvariant_to_json_array (variant);
          break;

        default:
          break;
        }
    }

  return json_node;
}

/**
 * json_gvariant_serialize_data:
 * @variant: A #GVariant to convert
 * @length: (out) (optional): the length of the returned string
 *
 * Converts @variant to its JSON encoded string representation.
 *
 * This is a convenience function around [func@Json.gvariant_serialize], to
 * obtain the JSON tree, and then [class@Json.Generator] to stringify it.
 *
 * Return value: (transfer full): The JSON encoded string corresponding to
 *   the given variant
 *
 * Since: 0.14
 */
gchar *
json_gvariant_serialize_data (GVariant *variant, gsize *length)
{
  JsonNode *json_node;
  JsonGenerator *generator;
  gchar *json;

  json_node = json_gvariant_serialize (variant);

  generator = json_generator_new ();

  json_generator_set_root (generator, json_node);
  json = json_generator_to_data (generator, length);

  g_object_unref (generator);

  json_node_unref (json_node);

  return json;
}

/* ========================================================================== */
/* JSON to GVariant */
/* ========================================================================== */

static GVariantClass
json_to_gvariant_get_next_class (JsonNode     *json_node,
                                 const gchar **signature)
{
  if (signature == NULL)
    {
      GVariantClass class = 0;

      switch (json_node_get_node_type (json_node))
        {
        case JSON_NODE_VALUE:
          switch (json_node_get_value_type (json_node))
            {
            case G_TYPE_BOOLEAN:
              class = G_VARIANT_CLASS_BOOLEAN;
              break;

            case G_TYPE_INT64:
              class = G_VARIANT_CLASS_INT64;
              break;

            case G_TYPE_DOUBLE:
              class = G_VARIANT_CLASS_DOUBLE;
              break;

            case G_TYPE_STRING:
              class = G_VARIANT_CLASS_STRING;
              break;
            }

          break;

        case JSON_NODE_ARRAY:
          class = G_VARIANT_CLASS_ARRAY;
          break;

        case JSON_NODE_OBJECT:
          class = JSON_G_VARIANT_CLASS_DICTIONARY;
          break;

        case JSON_NODE_NULL:
          class = G_VARIANT_CLASS_MAYBE;
          break;
        }

      return class;
    }
  else
    {
      if ((*signature)[0] == G_VARIANT_CLASS_ARRAY &&
          (*signature)[1] == G_VARIANT_CLASS_DICT_ENTRY)
        return JSON_G_VARIANT_CLASS_DICTIONARY;
      else
        return (*signature)[0];
    }
}

static gboolean
json_node_assert_type (JsonNode       *json_node,
                       JsonNodeType    type,
                       GType           sub_type,
                       GError        **error)
{
  if (JSON_NODE_TYPE (json_node) != type ||
      (type == JSON_NODE_VALUE &&
       (json_node_get_value_type (json_node) != sub_type)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   /* translators: the '%s' is the type name */
                   _("Unexpected type “%s” in JSON node"),
                   g_type_name (json_node_get_value_type (json_node)));
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

static void
json_to_gvariant_foreach_add (gpointer data,
                              gpointer user_data)
{
  GVariantBuilder *builder = user_data;
  GVariant *child = data;

  g_variant_builder_add_value (builder, child);
}

static void
json_to_gvariant_foreach_free (gpointer data,
                               gpointer user_data G_GNUC_UNUSED)
{
  GVariant *child = data;

  g_variant_unref (child);
}

static GVariant *
json_to_gvariant_build_from_glist (GList      *list,
                                   const char *signature)
{
  GVariantBuilder *builder;
  GVariant *result;

  builder = g_variant_builder_new (G_VARIANT_TYPE (signature));

  g_list_foreach (list, json_to_gvariant_foreach_add, builder);
  result = g_variant_builder_end (builder);

  g_variant_builder_unref (builder);

  return result;
}

static GVariant *
json_to_gvariant_tuple (JsonNode     *json_node,
                        const gchar **signature,
                        GError      **error)
{
  GVariant *variant = NULL;
  JsonArray *array;
  guint i;
  GList *children = NULL;
  gboolean roll_back = FALSE;
  const gchar *initial_signature;

  array = json_node_get_array (json_node);

  initial_signature = *signature;
  (*signature)++;
  i = 1;
  while ((*signature)[0] != ')' && (*signature)[0] != '\0')
    {
      JsonNode *json_child;
      GVariant *variant_child;

      if (i - 1 >= json_array_get_length (array))
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               _("Missing elements in JSON array to conform to a tuple"));
          roll_back = TRUE;
          break;
        }

      json_child = json_array_get_element (array, i - 1);

      variant_child = json_to_gvariant_recurse (json_child, signature, error);
      if (variant_child != NULL)
        {
          children = g_list_prepend (children, variant_child);
        }
      else
        {
          roll_back = TRUE;
          break;
        }

      i++;
    }
  children = g_list_reverse (children);

  if (! roll_back)
    {
      if ( (*signature)[0] != ')')
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               _("Missing closing symbol “)” in the GVariant tuple type"));
          roll_back = TRUE;
        }
      else if (json_array_get_length (array) >= i)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               _("Unexpected extra elements in JSON array"));
          roll_back = TRUE;
        }
      else
        {
          gchar *tuple_type;

          tuple_type = g_strndup (initial_signature,
                                  (*signature) - initial_signature + 1);

          variant = json_to_gvariant_build_from_glist (children, tuple_type);

          g_free (tuple_type);
        }
    }

  if (roll_back)
    g_list_foreach (children, json_to_gvariant_foreach_free, NULL);

  g_list_free (children);

  return variant;
}

static gchar *
signature_get_next_complete_type (const gchar **signature)
{
  GVariantClass class;
  const gchar *initial_signature;
  gchar *result;

  /* here it is assumed that 'signature' is a valid type string */

  initial_signature = *signature;
  class = (*signature)[0];

  if (class == G_VARIANT_CLASS_TUPLE || class == G_VARIANT_CLASS_DICT_ENTRY)
    {
      gchar stack[256] = {0};
      guint stack_len = 0;

      do
        {
          if ( (*signature)[0] == G_VARIANT_CLASS_TUPLE)
            {
              stack[stack_len] = ')';
              stack_len++;
            }
          else if ( (*signature)[0] == G_VARIANT_CLASS_DICT_ENTRY)
            {
              stack[stack_len] = '}';
              stack_len++;
            }

          (*signature)++;

          if ( (*signature)[0] == stack[stack_len - 1])
            stack_len--;
        }
      while (stack_len > 0);

      (*signature)++;
    }
  else if (class == G_VARIANT_CLASS_ARRAY || class == G_VARIANT_CLASS_MAYBE)
    {
      gchar *tmp_sig;

      (*signature)++;
      tmp_sig = signature_get_next_complete_type (signature);
      g_free (tmp_sig);
    }
  else
    {
      (*signature)++;
    }

  result = g_strndup (initial_signature, (*signature) - initial_signature);

  return result;
}

static GVariant *
json_to_gvariant_maybe (JsonNode     *json_node,
                        const gchar **signature,
                        GError      **error)
{
  GVariant *variant = NULL;
  GVariant *value;
  gchar *maybe_signature;

  if (signature)
    {
      (*signature)++;
      maybe_signature = signature_get_next_complete_type (signature);
    }
  else
    {
      maybe_signature = g_strdup ("v");
    }

  if (json_node_get_node_type (json_node) == JSON_NODE_NULL)
    {
      variant = g_variant_new_maybe (G_VARIANT_TYPE (maybe_signature), NULL);
    }
  else
    {
      const gchar *tmp_signature;

      tmp_signature = maybe_signature;
      value = json_to_gvariant_recurse (json_node,
                                        &tmp_signature,
                                        error);

      if (value != NULL)
        variant = g_variant_new_maybe (G_VARIANT_TYPE (maybe_signature), value);
    }

  g_free (maybe_signature);

  /* compensate the (*signature)++ call at the end of 'recurse()' */
  if (signature)
    (*signature)--;

  return variant;
}

static GVariant *
json_to_gvariant_array (JsonNode     *json_node,
                        const gchar **signature,
                        GError      **error)
{
  GVariant *variant = NULL;
  JsonArray *array;
  GList *children = NULL;
  gboolean roll_back = FALSE;
  const gchar *orig_signature = NULL;
  gchar *child_signature;

  array = json_node_get_array (json_node);

  if (signature != NULL)
    {
      orig_signature = *signature;

      (*signature)++;
      child_signature = signature_get_next_complete_type (signature);
    }
  else
    child_signature = g_strdup ("v");

  if (json_array_get_length (array) > 0)
    {
      guint i;
      guint len;

      len = json_array_get_length (array);
      for (i = 0; i < len; i++)
        {
          JsonNode *json_child;
          GVariant *variant_child;
          const gchar *tmp_signature;

          json_child = json_array_get_element (array, i);

          tmp_signature = child_signature;
          variant_child = json_to_gvariant_recurse (json_child,
                                                    &tmp_signature,
                                                    error);
          if (variant_child != NULL)
            {
              children = g_list_prepend (children, variant_child);
            }
          else
            {
              roll_back = TRUE;
              break;
            }
        }
      children = g_list_reverse (children);
    }

  if (!roll_back)
    {
      gchar *array_signature;

      if (signature)
        array_signature = g_strndup (orig_signature, (*signature) - orig_signature);
      else
        array_signature = g_strdup ("av");

      variant = json_to_gvariant_build_from_glist (children, array_signature);

      g_free (array_signature);

      /* compensate the (*signature)++ call at the end of 'recurse()' */
      if (signature)
        (*signature)--;
    }
  else
    g_list_foreach (children, json_to_gvariant_foreach_free, NULL);

  g_list_free (children);
  g_free (child_signature);

  return variant;
}

static GVariant *
gvariant_simple_from_string (const gchar    *st,
                             GVariantClass   class,
                             GError        **error)
{
  GVariant *variant = NULL;
  gchar *nptr = NULL;
  gboolean conversion_error = FALSE;
  gint64 signed_value;
  guint64 unsigned_value;
  gdouble double_value;

  errno = 0;

  switch (class)
    {
    case G_VARIANT_CLASS_BOOLEAN:
      if (g_strcmp0 (st, "true") == 0)
        variant = g_variant_new_boolean (TRUE);
      else if (g_strcmp0 (st, "false") == 0)
        variant = g_variant_new_boolean (FALSE);
      else
        conversion_error = TRUE;
      break;

    case G_VARIANT_CLASS_BYTE:
      signed_value = g_ascii_strtoll (st, &nptr, 10);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_byte (signed_value);
      break;

    case G_VARIANT_CLASS_INT16:
      signed_value = g_ascii_strtoll (st, &nptr, 10);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_int16 (signed_value);
      break;

    case G_VARIANT_CLASS_UINT16:
      signed_value = g_ascii_strtoll (st, &nptr, 10);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_uint16 (signed_value);
      break;

    case G_VARIANT_CLASS_INT32:
      signed_value = g_ascii_strtoll (st, &nptr, 10);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_int32 (signed_value);
      break;

    case G_VARIANT_CLASS_UINT32:
      unsigned_value = g_ascii_strtoull (st, &nptr, 10);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_uint32 (unsigned_value);
      break;

    case G_VARIANT_CLASS_INT64:
      signed_value = g_ascii_strtoll (st, &nptr, 10);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_int64 (signed_value);
      break;

    case G_VARIANT_CLASS_UINT64:
      unsigned_value = g_ascii_strtoull (st, &nptr, 10);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_uint64 (unsigned_value);
      break;

    case G_VARIANT_CLASS_HANDLE:
      signed_value = strtol (st, &nptr, 10);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_handle (signed_value);
      break;

    case G_VARIANT_CLASS_DOUBLE:
      double_value = g_ascii_strtod (st, &nptr);
      conversion_error = errno != 0 || nptr == st;
      variant = g_variant_new_double (double_value);
      break;

    case G_VARIANT_CLASS_STRING:
    case G_VARIANT_CLASS_OBJECT_PATH:
    case G_VARIANT_CLASS_SIGNATURE:
      variant = g_variant_new_string (st);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (conversion_error)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           _("Invalid string value converting to GVariant"));
      if (variant != NULL)
        {
          g_variant_unref (variant);
          variant = NULL;
        }
    }

  return variant;
}

static void
parse_dict_entry_signature (const gchar **signature,
                            gchar       **entry_signature,
                            gchar       **key_signature,
                            gchar       **value_signature)
{
  const gchar *tmp_sig;

  if (signature != NULL)
    *entry_signature = signature_get_next_complete_type (signature);
  else
    *entry_signature = g_strdup ("{sv}");

  tmp_sig = (*entry_signature) + 1;
  *key_signature = signature_get_next_complete_type (&tmp_sig);
  *value_signature = signature_get_next_complete_type (&tmp_sig);
}

static GVariant *
json_to_gvariant_dict_entry (JsonNode     *json_node,
                             const gchar **signature,
                             GError      **error)
{
  GVariant *variant = NULL;
  JsonObject *obj;

  gchar *entry_signature;
  gchar *key_signature;
  gchar *value_signature;
  const gchar *tmp_signature;

  GQueue *members;
  const gchar *json_member;
  JsonNode *json_value;
  GVariant *variant_member;
  GVariant *variant_value;

  obj = json_node_get_object (json_node);

  if (json_object_get_size (obj) != 1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           _("A GVariant dictionary entry expects a JSON object with exactly one member"));
      return NULL;
    }

  parse_dict_entry_signature (signature,
                              &entry_signature,
                              &key_signature,
                              &value_signature);

  members = json_object_get_members_internal (obj);
  json_member = (const gchar *) members->head->data;
  variant_member = gvariant_simple_from_string (json_member,
                                                key_signature[0],
                                                error);
  if (variant_member != NULL)
    {
      json_value = json_object_get_member (obj, json_member);

      tmp_signature = value_signature;
      variant_value = json_to_gvariant_recurse (json_value,
                                                &tmp_signature,
                                                error);

      if (variant_value != NULL)
        {
          GVariantBuilder *builder;

          builder = g_variant_builder_new (G_VARIANT_TYPE (entry_signature));
          g_variant_builder_add_value (builder, variant_member);
          g_variant_builder_add_value (builder, variant_value);
          variant = g_variant_builder_end (builder);

          g_variant_builder_unref (builder);
        }
    }

  g_free (value_signature);
  g_free (key_signature);
  g_free (entry_signature);

  /* compensate the (*signature)++ call at the end of 'recurse()' */
  if (signature)
    (*signature)--;

  return variant;
}

static GVariant *
json_to_gvariant_dictionary (JsonNode     *json_node,
                             const gchar **signature,
                             GError      **error)
{
  GVariant *variant = NULL;
  JsonObject *obj;
  gboolean roll_back = FALSE;

  gchar *dict_signature;
  gchar *entry_signature;
  gchar *key_signature;
  gchar *value_signature;
  const gchar *tmp_signature;

  GVariantBuilder *builder;
  GQueue *members;
  GList *member;

  obj = json_node_get_object (json_node);

  if (signature != NULL)
    (*signature)++;

  parse_dict_entry_signature (signature,
                              &entry_signature,
                              &key_signature,
                              &value_signature);

  dict_signature = g_strdup_printf ("a%s", entry_signature);

  builder = g_variant_builder_new (G_VARIANT_TYPE (dict_signature));

  members = json_object_get_members_internal (obj);

  for (member = members->head; member != NULL; member = member->next)
    {
      const gchar *json_member;
      JsonNode *json_value;
      GVariant *variant_member;
      GVariant *variant_value;

      json_member = (const gchar *) member->data;
      variant_member = gvariant_simple_from_string (json_member,
                                                    key_signature[0],
                                                    error);
      if (variant_member == NULL)
        {
          roll_back = TRUE;
          break;
        }

      json_value = json_object_get_member (obj, json_member);

      tmp_signature = value_signature;
      variant_value = json_to_gvariant_recurse (json_value,
                                                &tmp_signature,
                                                error);

      if (variant_value != NULL)
        {
          g_variant_builder_open (builder, G_VARIANT_TYPE (entry_signature));
          g_variant_builder_add_value (builder, variant_member);
          g_variant_builder_add_value (builder, variant_value);
          g_variant_builder_close (builder);
        }
      else
        {
          roll_back = TRUE;
          break;
        }
    }

  if (! roll_back)
    variant = g_variant_builder_end (builder);

  g_variant_builder_unref (builder);
  g_free (value_signature);
  g_free (key_signature);
  g_free (entry_signature);
  g_free (dict_signature);

  /* compensate the (*signature)++ call at the end of 'recurse()' */
  if (signature != NULL)
    (*signature)--;

  return variant;
}

static GVariant *
json_to_gvariant_recurse (JsonNode      *json_node,
                          const gchar  **signature,
                          GError       **error)
{
  GVariant *variant = NULL;
  GVariantClass class;

  class = json_to_gvariant_get_next_class (json_node, signature);

  if (class == JSON_G_VARIANT_CLASS_DICTIONARY)
    {
      if (json_node_assert_type (json_node, JSON_NODE_OBJECT, 0, error))
        variant = json_to_gvariant_dictionary (json_node, signature, error);

      goto out;
    }

  if (JSON_NODE_TYPE (json_node) == JSON_NODE_VALUE &&
      json_node_get_value_type (json_node) == G_TYPE_STRING)
    {
      const gchar* str = json_node_get_string (json_node);
      switch (class)
        {
        case G_VARIANT_CLASS_BOOLEAN:
        case G_VARIANT_CLASS_BYTE:
        case G_VARIANT_CLASS_INT16:
        case G_VARIANT_CLASS_UINT16:
        case G_VARIANT_CLASS_INT32:
        case G_VARIANT_CLASS_UINT32:
        case G_VARIANT_CLASS_INT64:
        case G_VARIANT_CLASS_UINT64:
        case G_VARIANT_CLASS_HANDLE:
        case G_VARIANT_CLASS_DOUBLE:
        case G_VARIANT_CLASS_STRING:
          variant = gvariant_simple_from_string (str, class, error);
          goto out;
        default:
          break;
        }
    }

  switch (class)
    {
    case G_VARIANT_CLASS_BOOLEAN:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_BOOLEAN, error))
        variant = g_variant_new_boolean (json_node_get_boolean (json_node));
      break;

    case G_VARIANT_CLASS_BYTE:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_INT64, error))
        variant = g_variant_new_byte (json_node_get_int (json_node));
      break;

    case G_VARIANT_CLASS_INT16:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_INT64, error))
        variant = g_variant_new_int16 (json_node_get_int (json_node));
      break;

    case G_VARIANT_CLASS_UINT16:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_INT64, error))
        variant = g_variant_new_uint16 (json_node_get_int (json_node));
      break;

    case G_VARIANT_CLASS_INT32:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_INT64, error))
        variant = g_variant_new_int32 (json_node_get_int (json_node));
      break;

    case G_VARIANT_CLASS_UINT32:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_INT64, error))
        variant = g_variant_new_uint32 (json_node_get_int (json_node));
      break;

    case G_VARIANT_CLASS_INT64:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_INT64, error))
        variant = g_variant_new_int64 (json_node_get_int (json_node));
      break;

    case G_VARIANT_CLASS_UINT64:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_INT64, error))
        variant = g_variant_new_uint64 (json_node_get_int (json_node));
      break;

    case G_VARIANT_CLASS_HANDLE:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_INT64, error))
        variant = g_variant_new_handle (json_node_get_int (json_node));
      break;

    case G_VARIANT_CLASS_DOUBLE:
      /* Doubles can look like ints to the json parser: when they don't have a dot */
      if (JSON_NODE_TYPE (json_node) == JSON_NODE_VALUE &&
          json_node_get_value_type (json_node) == G_TYPE_INT64)
        variant = g_variant_new_double (json_node_get_int (json_node));
      else if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_DOUBLE, error))
        variant = g_variant_new_double (json_node_get_double (json_node));
      break;

    case G_VARIANT_CLASS_STRING:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_STRING, error))
        variant = g_variant_new_string (json_node_get_string (json_node));
      break;

    case G_VARIANT_CLASS_OBJECT_PATH:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_STRING, error))
        variant = g_variant_new_object_path (json_node_get_string (json_node));
      break;

    case G_VARIANT_CLASS_SIGNATURE:
      if (json_node_assert_type (json_node, JSON_NODE_VALUE, G_TYPE_STRING, error))
        variant = g_variant_new_signature (json_node_get_string (json_node));
      break;

    case G_VARIANT_CLASS_VARIANT:
      variant = g_variant_new_variant (json_to_gvariant_recurse (json_node,
                                                                 NULL,
                                                                 error));
      break;

    case G_VARIANT_CLASS_MAYBE:
      variant = json_to_gvariant_maybe (json_node, signature, error);
      break;

    case G_VARIANT_CLASS_ARRAY:
      if (json_node_assert_type (json_node, JSON_NODE_ARRAY, 0, error))
        variant = json_to_gvariant_array (json_node, signature, error);
      break;

    case G_VARIANT_CLASS_TUPLE:
      if (json_node_assert_type (json_node, JSON_NODE_ARRAY, 0, error))
        variant = json_to_gvariant_tuple (json_node, signature, error);
      break;

    case G_VARIANT_CLASS_DICT_ENTRY:
      if (json_node_assert_type (json_node, JSON_NODE_OBJECT, 0, error))
        variant = json_to_gvariant_dict_entry (json_node, signature, error);
      break;

    default:
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("GVariant class “%c” not supported"), class);
      break;
    }

out:
  if (signature)
    (*signature)++;

  return variant;
}

/**
 * json_gvariant_deserialize:
 * @json_node: the node to convert
 * @signature: (nullable): a valid `GVariant` type string
 * @error: (nullable): return location for a #GError, or `NULL`
 *
 * Converts a JSON data structure to a `GVariant`.
 *
 * If `signature` is not `NULL`, it will be used to resolve ambiguous
 * data types.
 *
 * If no error occurs, the resulting `GVariant` is guaranteed to conform
 * to `signature`.
 *
 * If `signature` is not `NULL` but does not represent a valid `GVariant` type
 * string, `NULL` is returned and the `error` is set to
 * `G_IO_ERROR_INVALID_ARGUMENT`.
 *
 * If a `signature` is provided but the JSON structure cannot be mapped to it,
 * `NULL` is returned and the `error` is set to `G_IO_ERROR_INVALID_DATA`.
 *
 * If `signature` is `NULL`, the conversion is done based strictly on the types
 * in the JSON nodes.
 *
 * The returned variant has a floating reference that will need to be sunk
 * by the caller code.
 *
 * Return value: (transfer floating) (nullable): A newly created `GVariant`
 *
 * Since: 0.14
 */
GVariant *
json_gvariant_deserialize (JsonNode     *json_node,
                           const gchar  *signature,
                           GError      **error)
{
  g_return_val_if_fail (json_node != NULL, NULL);

  if (signature != NULL && ! g_variant_type_string_is_valid (signature))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid GVariant signature"));
      return NULL;
    }

  return json_to_gvariant_recurse (json_node, signature ? &signature : NULL, error);
}

/**
 * json_gvariant_deserialize_data:
 * @json: A JSON data string
 * @length: The length of @json, or -1 if `NUL`-terminated
 * @signature: (nullable): A valid `GVariant` type string
 * @error: A pointer to a #GError
 *
 * Converts a JSON string to a `GVariant` value.
 *
 * This function works exactly like [func@Json.gvariant_deserialize], but
 * takes a JSON encoded string instead.
 *
 * The string is first converted to a [struct@Json.Node] using
 * [class@Json.Parser], and then `json_gvariant_deserialize` is called on
 * the node.
 *
 * The returned variant has a floating reference that will need to be sunk
 * by the caller code.
 *
 * Returns: (transfer floating) (nullable): A newly created `GVariant`D compliant
 *
 * Since: 0.14
 */
GVariant *
json_gvariant_deserialize_data (const gchar  *json,
                                gssize        length,
                                const gchar  *signature,
                                GError      **error)
{
  JsonParser *parser;
  GVariant *variant = NULL;
  JsonNode *root;

  parser = json_parser_new ();

  if (! json_parser_load_from_data (parser, json, length, error))
    {
      g_object_unref (parser);
      return NULL;
    }

  root = json_parser_get_root (parser);
  if (root == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           _("JSON data is empty"));
    }
  else
    {
      variant =
        json_gvariant_deserialize (json_parser_get_root (parser), signature, error);
    }

  g_object_unref (parser);

  return variant;
}
