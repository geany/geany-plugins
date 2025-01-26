/* jsonrpc-input-stream.c
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

//#define G_LOG_DOMAIN "jsonrpc-input-stream"

#include "config.h"

#include <errno.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "jsonrpc-input-stream.h"
#include "jsonrpc-input-stream-private.h"

typedef struct
{
  gssize        content_length;
  gchar        *buffer;
  GVariantType *gvariant_type;
  gint16        priority;
  guint         use_gvariant : 1;
} ReadState;

typedef struct
{
  gssize max_size_bytes;
  guint  has_seen_gvariant : 1;
} JsonrpcInputStreamPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (JsonrpcInputStream, jsonrpc_input_stream, G_TYPE_DATA_INPUT_STREAM)

static gboolean jsonrpc_input_stream_debug;

static void
read_state_free (gpointer data)
{
  ReadState *state = data;

  g_clear_pointer (&state->buffer, g_free);
  g_clear_pointer (&state->gvariant_type, g_free);
  g_slice_free (ReadState, state);
}

static void
jsonrpc_input_stream_class_init (JsonrpcInputStreamClass *klass)
{
  jsonrpc_input_stream_debug = !!g_getenv ("JSONRPC_DEBUG");
}

static void
jsonrpc_input_stream_init (JsonrpcInputStream *self)
{
  JsonrpcInputStreamPrivate *priv = jsonrpc_input_stream_get_instance_private (self);

  /* 16 MB */
  priv->max_size_bytes = 16 * 1024 * 1024;

  g_data_input_stream_set_newline_type (G_DATA_INPUT_STREAM (self),
                                        G_DATA_STREAM_NEWLINE_TYPE_ANY);
}

JsonrpcInputStream *
jsonrpc_input_stream_new (GInputStream *base_stream)
{
  return g_object_new (JSONRPC_TYPE_INPUT_STREAM,
                       "base-stream", base_stream,
                       NULL);
}

static void
jsonrpc_input_stream_read_body_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  JsonrpcInputStream *self = (JsonrpcInputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) message = NULL;
  ReadState *state;
  gsize n_read;

  g_assert (JSONRPC_IS_INPUT_STREAM (self));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  if (!g_input_stream_read_all_finish (G_INPUT_STREAM (self), result, &n_read, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if ((gssize)n_read != state->content_length)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Failed to read %"G_GSSIZE_FORMAT" bytes",
                               state->content_length);
      return;
    }

  state->buffer [state->content_length] = '\0';

  if G_UNLIKELY (jsonrpc_input_stream_debug && state->use_gvariant == FALSE)
    g_message ("<<< %s", state->buffer);

  if (state->use_gvariant)
    {
      g_autoptr(GBytes) bytes = NULL;

      bytes = g_bytes_new_take (g_steal_pointer (&state->buffer), state->content_length);
      message = g_variant_new_from_bytes (state->gvariant_type ?  state->gvariant_type
                                                               : G_VARIANT_TYPE_VARDICT,
                                          bytes, FALSE);

      if G_UNLIKELY (jsonrpc_input_stream_debug && state->use_gvariant)
        {
          g_autofree gchar *debugstr = g_variant_print (message, TRUE);
          g_message ("<<< %s", debugstr);
        }
    }
  else
    {
      message = json_gvariant_deserialize_data (state->buffer, state->content_length, NULL, &error);
      g_clear_pointer (&state->buffer, g_free);
    }

  g_assert (state->buffer == NULL);
  g_assert (message != NULL || error != NULL);

  /* Don't let message be floating */
  if (message != NULL)
    g_variant_take_ref (message);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task,
                           g_steal_pointer (&message),
                           (GDestroyNotify)g_variant_unref);
}

static void
jsonrpc_input_stream_read_headers_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  JsonrpcInputStream *self = (JsonrpcInputStream *)object;
  JsonrpcInputStreamPrivate *priv = jsonrpc_input_stream_get_instance_private (self);
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *line = NULL;
  GCancellable *cancellable = NULL;
  ReadState *state;
  gsize length = 0;

  g_assert (JSONRPC_IS_INPUT_STREAM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);
  cancellable = g_task_get_cancellable (task);

  line = g_data_input_stream_read_line_finish_utf8 (G_DATA_INPUT_STREAM (self), result, &length, &error);

  if (line == NULL)
    {
      if (error != NULL)
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No data to read from peer");
      return;
    }

  if (strncasecmp ("Content-Length: ", line, 16) == 0)
    {
      const gchar *lenptr = line + 16;
      gint64 content_length;

      content_length = g_ascii_strtoll (lenptr, NULL, 10);

      if (((content_length == G_MININT64 || content_length == G_MAXINT64) && errno == ERANGE) ||
          (content_length < 0) ||
          (content_length == G_MAXSSIZE) ||
          (content_length > priv->max_size_bytes))
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Invalid Content-Length received from peer");
          return;
        }

      state->content_length = content_length;
    }

  if (strncasecmp ("Content-Type: ", line, 14) == 0)
    {
      if (NULL != strstr (line, "application/gvariant"))
        state->use_gvariant = TRUE;
    }

  if (strncasecmp ("X-GVariant-Type: ", line, 17) == 0)
    {
      const gchar *type_string = line + 17;

      if (!g_variant_type_string_is_valid (type_string))
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Invalid X-GVariant-Type received from peer");
          return;
        }

      g_clear_pointer (&state->gvariant_type, g_free);
      state->gvariant_type = (GVariantType *)g_strdup (type_string);
    }

  /*
   * If we are at the end of the headers, we can make progress towards
   * parsing the JSON content. Otherwise we need to continue parsing
   * the next header.
   */

  if (line[0] == '\0')
    {
      if (state->content_length <= 0)
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Invalid or missing Content-Length header from peer");
          return;
        }

      state->buffer = g_malloc (state->content_length + 1);
      g_input_stream_read_all_async (G_INPUT_STREAM (self),
                                     state->buffer,
                                     state->content_length,
                                     state->priority,
                                     cancellable,
                                     jsonrpc_input_stream_read_body_cb,
                                     g_steal_pointer (&task));
      return;
    }

  g_data_input_stream_read_line_async (G_DATA_INPUT_STREAM (self),
                                       state->priority,
                                       cancellable,
                                       jsonrpc_input_stream_read_headers_cb,
                                       g_steal_pointer (&task));
}

void
jsonrpc_input_stream_read_message_async (JsonrpcInputStream  *self,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  ReadState *state;

  g_return_if_fail (JSONRPC_IS_INPUT_STREAM (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (ReadState);
  state->content_length = -1;
  state->priority = G_PRIORITY_LOW;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_input_stream_read_message_async);
  g_task_set_task_data (task, state, read_state_free);
  g_task_set_priority (task, state->priority);

  g_data_input_stream_read_line_async (G_DATA_INPUT_STREAM (self),
                                       state->priority,
                                       cancellable,
                                       jsonrpc_input_stream_read_headers_cb,
                                       g_steal_pointer (&task));
}

gboolean
jsonrpc_input_stream_read_message_finish (JsonrpcInputStream  *self,
                                          GAsyncResult        *result,
                                          GVariant           **message,
                                          GError             **error)
{
  JsonrpcInputStreamPrivate *priv = jsonrpc_input_stream_get_instance_private (self);
  g_autoptr(GVariant) local_message = NULL;
  ReadState *state;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_INPUT_STREAM (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  /* track if we've seen an application/gvariant */
  state = g_task_get_task_data (G_TASK (result));
  priv->has_seen_gvariant |= state->use_gvariant;

  local_message = g_task_propagate_pointer (G_TASK (result), error);
  ret = local_message != NULL;

  if (message != NULL)
    {
      /* Unbox the variant if it is in a wrapper */
      if (local_message && g_variant_is_of_type (local_message, G_VARIANT_TYPE_VARIANT))
        *message = g_variant_get_variant (local_message);
      else
        *message = g_steal_pointer (&local_message);
    }

  return ret;
}

static void
jsonrpc_input_stream_read_message_sync_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  JsonrpcInputStream *self = (JsonrpcInputStream *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) message = NULL;
  GTask *task = user_data;

  g_assert (JSONRPC_IS_INPUT_STREAM (self));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_input_stream_read_message_finish (self, result, &message, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&message), (GDestroyNotify)g_variant_unref);
}

gboolean
jsonrpc_input_stream_read_message (JsonrpcInputStream  *self,
                                   GCancellable        *cancellable,
                                   GVariant           **message,
                                   GError             **error)
{
  g_autoptr(GMainContext) main_context = NULL;
  g_autoptr(GVariant) local_message = NULL;
  g_autoptr(GTask) task = NULL;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_INPUT_STREAM (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  main_context = g_main_context_ref_thread_default ();

  task = g_task_new (NULL, NULL, NULL, NULL);
  g_task_set_source_tag (task, jsonrpc_input_stream_read_message);

  jsonrpc_input_stream_read_message_async (self,
                                           cancellable,
                                           jsonrpc_input_stream_read_message_sync_cb,
                                           task);

  while (!g_task_get_completed (task))
    g_main_context_iteration (main_context, TRUE);

  local_message = g_task_propagate_pointer (task, error);
  ret = local_message != NULL;

  if (message != NULL)
    *message = g_steal_pointer (&local_message);

  return ret;
}

gboolean
_jsonrpc_input_stream_get_has_seen_gvariant (JsonrpcInputStream *self)
{
  JsonrpcInputStreamPrivate *priv = jsonrpc_input_stream_get_instance_private (self);

  g_return_val_if_fail (JSONRPC_IS_INPUT_STREAM (self), FALSE);

  return priv->has_seen_gvariant;
}
