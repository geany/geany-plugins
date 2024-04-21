/* jsonrpc-output-stream.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#define G_LOG_DOMAIN "jsonrpc-output-stream"

#include "config.h"

#include <string.h>

#include "jsonrpc-output-stream.h"
#include "jsonrpc-version.h"

/**
 * SECTION:jsonrpc-output-stream
 * @title: JsonrpcOutputStream
 * @short_description: A JSON-RPC output stream
 *
 * The #JsonrpcOutputStream is resonsible for serializing messages onto
 * the underlying stream.
 *
 * Optionally, if jsonrpc_output_stream_set_use_gvariant() has been called,
 * the messages will be encoded directly in the #GVariant format instead of
 * JSON along with setting a "Content-Type" header to "application/gvariant".
 * This is useful for situations where you control both sides of the RPC server
 * using jsonrpc-glib as you can reduce the overhead of parsing JSON nodes due
 * to #GVariant not requiring parsing or allocation overhead to the same degree
 * as JSON.
 *
 * For example, if you need a large message, which is encoded in JSON, you need
 * to decode the entire message up front which avoids performing lazy operations.
 * When using GVariant encoding, you have a single allocation created for the
 * #GVariant which means you reduce the memory pressure caused by lots of small
 * allocations.
 */

typedef struct
{
  GQueue queue;
  guint  use_gvariant : 1;
  guint  processing : 1;
} JsonrpcOutputStreamPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (JsonrpcOutputStream, jsonrpc_output_stream, G_TYPE_DATA_OUTPUT_STREAM)

static void jsonrpc_output_stream_write_message_async_cb (GObject      *object,
                                                          GAsyncResult *result,
                                                          gpointer      user_data);

enum {
  PROP_0,
  PROP_USE_GVARIANT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static gboolean jsonrpc_output_stream_debug;

static void
jsonrpc_output_stream_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  JsonrpcOutputStream *self = JSONRPC_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_USE_GVARIANT:
      g_value_set_boolean (value, jsonrpc_output_stream_get_use_gvariant (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
jsonrpc_output_stream_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  JsonrpcOutputStream *self = JSONRPC_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_USE_GVARIANT:
      jsonrpc_output_stream_set_use_gvariant (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
jsonrpc_output_stream_dispose (GObject *object)
{
  JsonrpcOutputStream *self = (JsonrpcOutputStream *)object;
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);

  g_queue_foreach (&priv->queue, (GFunc)g_object_unref, NULL);
  g_queue_clear (&priv->queue);

  G_OBJECT_CLASS (jsonrpc_output_stream_parent_class)->dispose (object);
}

static void
jsonrpc_output_stream_class_init (JsonrpcOutputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = jsonrpc_output_stream_dispose;
  object_class->get_property = jsonrpc_output_stream_get_property;
  object_class->set_property = jsonrpc_output_stream_set_property;

  properties [PROP_USE_GVARIANT] =
    g_param_spec_boolean ("use-gvariant",
                         "Use GVariant",
                         "If GVariant encoding should be used",
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  jsonrpc_output_stream_debug = !!g_getenv ("JSONRPC_DEBUG");
}

static void
jsonrpc_output_stream_init (JsonrpcOutputStream *self)
{
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);

  g_queue_init (&priv->queue);
}

static GBytes *
jsonrpc_output_stream_create_bytes (JsonrpcOutputStream  *self,
                                    GVariant             *message,
                                    GError              **error)
{
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);
  g_autoptr(GByteArray) buffer = NULL;
  g_autofree gchar *message_freeme = NULL;
  gconstpointer message_data = NULL;
  gsize message_len = 0;
  gchar header[256];
  gsize len;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (self));
  g_assert (message != NULL);

  buffer = g_byte_array_sized_new (g_variant_get_size (message) + 128);

  if G_UNLIKELY (jsonrpc_output_stream_debug)
    {
      g_autofree gchar *str = g_variant_print (message, TRUE);
      g_message (">>> %s", str);
    }

  if (priv->use_gvariant)
    {
      message_data = g_variant_get_data (message);
      message_len = g_variant_get_size (message);
    }
  else
    {
      message_freeme = json_gvariant_serialize_data (message, &message_len);
      message_data = message_freeme;
    }

  /* Add Content-Length header */
  len = g_snprintf (header, sizeof header, "Content-Length: %"G_GSIZE_FORMAT"\r\n", message_len);
  g_byte_array_append (buffer, (const guint8 *)header, len);

  if (priv->use_gvariant)
    {
      /* Add Content-Type header */
      len = g_snprintf (header, sizeof header, "Content-Type: application/%s\r\n",
                        priv->use_gvariant ? "gvariant" : "json");
      g_byte_array_append (buffer, (const guint8 *)header, len);

      /* Add our GVariantType for the peer to decode */
      len = g_snprintf (header, sizeof header, "X-GVariant-Type: %s\r\n",
                        (const gchar *)g_variant_get_type_string (message));
      g_byte_array_append (buffer, (const guint8 *)header, len);
    }

  g_byte_array_append (buffer, (const guint8 *)"\r\n", 2);

  /* Add serialized message data */
  g_byte_array_append (buffer, (const guint8 *)message_data, message_len);

  return g_byte_array_free_to_bytes (g_steal_pointer (&buffer));
}

JsonrpcOutputStream *
jsonrpc_output_stream_new (GOutputStream *base_stream)
{
  return g_object_new (JSONRPC_TYPE_OUTPUT_STREAM,
                       "base-stream", base_stream,
                       NULL);
}

static void
jsonrpc_output_stream_fail_pending (JsonrpcOutputStream *self)
{
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);
  const GList *iter;
  GList *list;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (self));

  list = priv->queue.head;

  priv->queue.head = NULL;
  priv->queue.tail = NULL;
  priv->queue.length = 0;

  for (iter = list; iter != NULL; iter = iter->next)
    {
      g_autoptr(GTask) task = iter->data;

      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Task failed due to stream failure");
    }

  g_list_free (list);
}

static void
jsonrpc_output_stream_pump (JsonrpcOutputStream *self)
{
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  const guint8 *data;
  GCancellable *cancellable;
  GBytes *bytes;
  gsize len;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (self));

  if (priv->queue.length == 0)
    return;

  if (priv->processing)
    return;

  task = g_queue_pop_head (&priv->queue);
  bytes = g_task_get_task_data (task);
  data = g_bytes_get_data (bytes, &len);
  cancellable = g_task_get_cancellable (task);

  if (g_output_stream_is_closed (G_OUTPUT_STREAM (self)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CLOSED,
                               "Stream has been closed");
      return;
    }

  priv->processing = TRUE;

  g_output_stream_write_all_async (G_OUTPUT_STREAM (self),
                                   data,
                                   len,
                                   G_PRIORITY_DEFAULT,
                                   cancellable,
                                   jsonrpc_output_stream_write_message_async_cb,
                                   g_steal_pointer (&task));
}

static void
jsonrpc_output_stream_write_message_async_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  JsonrpcOutputStream *self = (JsonrpcOutputStream *)object;
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  GBytes *bytes;
  gsize n_written;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  priv->processing = FALSE;

  if (!g_output_stream_write_all_finish (G_OUTPUT_STREAM (self), result, &n_written, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      jsonrpc_output_stream_fail_pending (self);
      return;
    }

  bytes = g_task_get_task_data (task);

  if (g_bytes_get_size (bytes) != n_written)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CLOSED,
                               "Failed to write all bytes to peer");
      jsonrpc_output_stream_fail_pending (self);
      return;
    }

  g_task_return_boolean (task, TRUE);

  jsonrpc_output_stream_pump (self);
}

/**
 * jsonrpc_output_stream_write_message_async:
 * @self: a #JsonrpcOutputStream
 * @message: (transfer none): a #GVariant
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a #GAsyncReadyCallback or %NULL
 * @user_data: closure data for @callback
 *
 * Asynchronously sends a message to the peer.
 *
 * This asynchronous operation will complete once the message has
 * been buffered, and there is no guarantee the peer received it.
 *
 * Since: 3.26
 */
void
jsonrpc_output_stream_write_message_async (JsonrpcOutputStream *self,
                                           GVariant            *message,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (JSONRPC_IS_OUTPUT_STREAM (self));
  g_return_if_fail (message != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_output_stream_write_message_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  if (NULL == (bytes = jsonrpc_output_stream_create_bytes (self, message, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_set_task_data (task, g_steal_pointer (&bytes), (GDestroyNotify)g_bytes_unref);
  g_queue_push_tail (&priv->queue, g_steal_pointer (&task));
  jsonrpc_output_stream_pump (self);
}

gboolean
jsonrpc_output_stream_write_message_finish (JsonrpcOutputStream  *self,
                                            GAsyncResult         *result,
                                            GError              **error)
{
  g_return_val_if_fail (JSONRPC_IS_OUTPUT_STREAM (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
jsonrpc_output_stream_write_message_sync_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  JsonrpcOutputStream *self = (JsonrpcOutputStream *)object;
  GTask *task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_output_stream_write_message_finish (self, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * jsonrpc_output_stream_write_message:
 * @self: a #JsonrpcOutputStream
 * @message: (transfer none): a #GVariant
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @error: a location for a #GError, or %NULL
 *
 * Synchronously sends a message to the peer.
 *
 * This operation will complete once the message has been buffered. There
 * is no guarantee the peer received it.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_output_stream_write_message (JsonrpcOutputStream  *self,
                                     GVariant             *message,
                                     GCancellable         *cancellable,
                                     GError              **error)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GMainContext) main_context = NULL;

  g_return_val_if_fail (JSONRPC_IS_OUTPUT_STREAM (self), FALSE);
  g_return_val_if_fail (message != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  main_context = g_main_context_ref_thread_default ();

  task = g_task_new (NULL, NULL, NULL, NULL);
  g_task_set_source_tag (task, jsonrpc_output_stream_write_message);

  jsonrpc_output_stream_write_message_async (self,
                                             message,
                                             cancellable,
                                             jsonrpc_output_stream_write_message_sync_cb,
                                             task);

  while (!g_task_get_completed (task))
    g_main_context_iteration (main_context, TRUE);

  return g_task_propagate_boolean (task, error);
}

gboolean
jsonrpc_output_stream_get_use_gvariant (JsonrpcOutputStream *self)
{
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);

  g_return_val_if_fail (JSONRPC_IS_OUTPUT_STREAM (self), FALSE);

  return priv->use_gvariant;
}

void
jsonrpc_output_stream_set_use_gvariant (JsonrpcOutputStream *self,
                                        gboolean             use_gvariant)
{
  JsonrpcOutputStreamPrivate *priv = jsonrpc_output_stream_get_instance_private (self);

  g_return_if_fail (JSONRPC_IS_OUTPUT_STREAM (self));

  use_gvariant = !!use_gvariant;

  if (priv->use_gvariant != use_gvariant)
    {
      priv->use_gvariant = use_gvariant;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_GVARIANT]);
    }
}
