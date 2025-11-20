/* jsonrpc-client.c
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

//#define G_LOG_DOMAIN "jsonrpc-client"

#include "config.h"

/**
 * JsonrpcClient:
 *
 * A client for JSON-RPC communication
 *
 * The #JsonrpcClient class provides a convenient API to coordinate with a
 * JSON-RPC server. You can provide the underlying [class@Gio.IOStream] to communicate
 * with allowing you to control the negotiation of how you setup your
 * communications channel. One such method might be to use a [class@Gio.Subprocess] and
 * communicate over stdin and stdout.
 *
 * Because JSON-RPC allows for out-of-band notifications from the server to
 * the client, it is important that the consumer of this API calls
 * [method@Client.close] or [method@Client.close_async] when they no longer
 * need the client. This is because #JsonrpcClient contains an asynchronous
 * read-loop to process incoming messages. Until [method@Client.close] or
 * [method@Client.close_async] have been called, this read loop will prevent
 * the object from finalizing (being freed).
 *
 * To make an RPC call, use [method@Client.call] or
 * [method@Client.call_async] and provide the method name and the parameters
 * as a [struct@GLib.Variant] for call.
 *
 * It is a programming error to mix synchronous and asynchronous API calls
 * of the #JsonrpcClient class.
 *
 * For synchronous calls, #JsonrpcClient will use the thread-default
 * [struct@GLib.MainContext]. If you have special needs here ensure you've set the context
 * before calling into any #JsonrpcClient API.
 *
 * Since: 3.26
 */

#include <glib.h>

#include "jsonrpc-client.h"
#include "jsonrpc-input-stream.h"
#include "jsonrpc-input-stream-private.h"
#include "jsonrpc-marshalers.h"
#include "jsonrpc-output-stream.h"

typedef struct
{
  /*
   * The invocations field contains a hashtable that maps request ids to
   * the GTask that is awaiting their completion. The tasks are removed
   * from the hashtable automatically upon completion by connecting to
   * the GTask::completed signal. When reading a message from the input
   * stream, we use the request id as a string to lookup the inflight
   * invocation. The result is passed as the result of the task.
   */
  GHashTable *invocations;

  /*
   * We hold an extra reference to the GIOStream pair to make things
   * easier to construct and ensure that the streams are in tact in
   * case they are poorly implemented.
   */
  GIOStream *io_stream;

  /*
   * The input_stream field contains our wrapper input stream around the
   * underlying input stream provided by JsonrpcClient::io-stream. This
   * allows us to conveniently write GVariant  instances.
   */
  JsonrpcInputStream *input_stream;

  /*
   * The output_stream field contains our wrapper output stream around the
   * underlying output stream provided by JsonrpcClient::io-stream. This
   * allows us to convieniently read GVariant instances.
   */
  JsonrpcOutputStream *output_stream;

  /*
   * This cancellable is used for our async read loops so that we can
   * cancel the operation to shutdown the client. Otherwise, we would
   * indefinitely leak our client due to the self-reference on our
   * read loop user_data parameter.
   */
  GCancellable *read_loop_cancellable;

  /*
   * Every JSONRPC invocation needs a request id. This is a monotonic
   * integer that we encode as a string to the server.
   */
  gint64 sequence;

  /*
   * This bit indicates if we have sent a call yet. Once we send our
   * first call, we start our read loop which will allow us to also
   * dispatch notifications out of band.
   */
  guint is_first_call : 1;

  /*
   * This bit is set when the program has called jsonrpc_client_close()
   * or jsonrpc_client_close_async(). When the read loop returns, it
   * will check for this and discontinue further asynchronous reads.
   */
  guint in_shutdown : 1;

  /*
   * If we have panic'd, this will be set to TRUE so that we can short
   * circuit on future operations sooner.
   */
  guint failed : 1;

  /*
   * Only set once we've emitted the ::failed signal (so we only do that
   * a single time).
   */
  guint emitted_failed : 1;

  /*
   * If we should try to use gvariant encoding when communicating with
   * our peer. This is helpful to be able to lower parser and memory
   * overhead.
   */
  guint use_gvariant : 1;
} JsonrpcClientPrivate;

typedef struct
{
  GHashTable *invocations;
  GError *error;
} PanicData;

G_DEFINE_TYPE_WITH_PRIVATE (JsonrpcClient, jsonrpc_client, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_IO_STREAM,
  PROP_USE_GVARIANT,
  N_PROPS
};

enum {
  FAILED,
  HANDLE_CALL,
  NOTIFICATION,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

/*
 * Check to see if this looks like a jsonrpc 2.0 reply of any kind.
 */
static gboolean
is_jsonrpc_reply (GVariantDict *dict)
{
  const gchar *value = NULL;

  g_assert (dict != NULL);

  return (g_variant_dict_contains (dict, "jsonrpc") &&
          g_variant_dict_lookup (dict, "jsonrpc", "&s", &value) &&
          g_str_equal (value, "2.0"));
}

/*
 * Check to see if this looks like a notification reply.
 */
static gboolean
is_jsonrpc_notification (GVariantDict *dict)
{
  const gchar *method = NULL;

  g_assert (dict != NULL);

  return (!g_variant_dict_contains (dict, "id") &&
          g_variant_dict_contains (dict, "method") &&
          g_variant_dict_lookup (dict, "method", "&s", &method) &&
          method != NULL && *method != '\0');
}

/*
 * Check to see if this looks like a proper result for an RPC.
 */
static gboolean
is_jsonrpc_result (GVariantDict *dict)
{
  g_assert (dict != NULL);

  return (g_variant_dict_contains (dict, "id") &&
          g_variant_dict_contains (dict, "result"));
}


/*
 * Check to see if this looks like a proper method call for an RPC.
 */
static gboolean
is_jsonrpc_call (GVariantDict *dict)
{
  const gchar *method = NULL;

  g_assert (dict != NULL);

  return (g_variant_dict_contains (dict, "id") &&
          g_variant_dict_contains (dict, "method") &&
          g_variant_dict_lookup (dict, "method", "&s", &method) &&
          g_variant_dict_contains (dict, "params"));
}

static gboolean
error_invocations_from_idle (gpointer data)
{
  PanicData *pd = data;
  GHashTableIter iter;
  GTask *task;

  g_assert (pd != NULL);
  g_assert (pd->invocations != NULL);
  g_assert (pd->error != NULL);

  g_hash_table_iter_init (&iter, pd->invocations);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&task))
    {
      if (!g_task_get_completed (task))
        g_task_return_error (task, g_error_copy (pd->error));
    }

  g_clear_pointer (&pd->invocations, g_hash_table_unref);
  g_clear_pointer (&pd->error, g_error_free);
  g_slice_free (PanicData, pd);

  return G_SOURCE_REMOVE;
}

static void
cancel_pending_from_main (JsonrpcClient *self,
                          const GError  *error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  PanicData *pd;

  g_assert (JSONRPC_IS_CLIENT (self));
  g_assert (error != NULL);

  /*
   * Defer the completion of all tasks (with errors) until we've made it
   * back to the main loop. Otherwise, we get into difficult to determine
   * re-entrancy cases.
   */
  pd = g_slice_new0 (PanicData);
  pd->invocations = g_steal_pointer (&priv->invocations);
  pd->error = g_error_copy (error);
  g_idle_add_full (G_MAXINT, error_invocations_from_idle, pd, NULL);

  /* Keep a hashtable around for code that expects a pointer there */
  priv->invocations = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
}

static gboolean
emit_failed_from_main (JsonrpcClient *self)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  g_assert (JSONRPC_IS_CLIENT (self));

  if (!priv->emitted_failed)
    {
      priv->emitted_failed = TRUE;
      g_signal_emit (self, signals [FAILED], 0);
    }

  return G_SOURCE_REMOVE;
}

/*
 * jsonrpc_client_panic:
 *
 * This function should be called to "tear down everything" and ensure we
 * cleanup.
 */
static void
jsonrpc_client_panic (JsonrpcClient *self,
                      const GError  *error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(JsonrpcClient) hold = NULL;

  g_assert (JSONRPC_IS_CLIENT (self));
  g_assert (error != NULL);

  hold = g_object_ref (self);

  priv->failed = TRUE;

  cancel_pending_from_main (self, error);

  /* Now close the connection */
  jsonrpc_client_close (self, NULL, NULL);

  /*
   * Clear our input and output streams so that new calls
   * fail immediately due to not being connected.
   */
  g_clear_object (&priv->input_stream);
  g_clear_object (&priv->output_stream);

  /*
   * Queue a "failed" signal from a main loop callback so that we don't
   * get the client into weird stuff from signal callbacks here.
   */
  if (!priv->emitted_failed)
    g_idle_add_full (G_MAXINT,
                     (GSourceFunc)emit_failed_from_main,
                     g_object_ref (self),
                     g_object_unref);
}

/*
 * jsonrpc_client_check_ready:
 *
 * Checks to see if the client is in a position to make requests.
 *
 * Returns: %TRUE if the client is ready for RPCs; otherwise %FALSE
 *   and @error is set.
 */
static gboolean
jsonrpc_client_check_ready (JsonrpcClient  *self,
                            GError        **error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  g_assert (JSONRPC_IS_CLIENT (self));

  if (priv->failed || priv->in_shutdown || priv->output_stream == NULL || priv->input_stream == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "No stream available to deliver invocation");
      return FALSE;
    }

  return TRUE;
}

static void
jsonrpc_client_constructed (GObject *object)
{
  JsonrpcClient *self = (JsonrpcClient *)object;
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  GInputStream *input_stream;
  GOutputStream *output_stream;

  G_OBJECT_CLASS (jsonrpc_client_parent_class)->constructed (object);

  if (priv->io_stream == NULL)
    {
      g_warning ("%s requires a GIOStream to communicate. Disabling.",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  input_stream = g_io_stream_get_input_stream (priv->io_stream);
  output_stream = g_io_stream_get_output_stream (priv->io_stream);

  priv->input_stream = jsonrpc_input_stream_new (input_stream);
  priv->output_stream = jsonrpc_output_stream_new (output_stream);
}

static void
jsonrpc_client_dispose (GObject *object)
{
  JsonrpcClient *self = (JsonrpcClient *)object;
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  g_clear_pointer (&priv->invocations, g_hash_table_unref);

  g_clear_object (&priv->input_stream);
  g_clear_object (&priv->output_stream);
  g_clear_object (&priv->io_stream);
  g_clear_object (&priv->read_loop_cancellable);

  G_OBJECT_CLASS (jsonrpc_client_parent_class)->dispose (object);
}

static void
jsonrpc_client_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  JsonrpcClient *self = JSONRPC_CLIENT (object);

  switch (prop_id)
    {
    case PROP_USE_GVARIANT:
      g_value_set_boolean (value, jsonrpc_client_get_use_gvariant (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
jsonrpc_client_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  JsonrpcClient *self = JSONRPC_CLIENT (object);
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_IO_STREAM:
      priv->io_stream = g_value_dup_object (value);
      break;

    case PROP_USE_GVARIANT:
      jsonrpc_client_set_use_gvariant (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
jsonrpc_client_class_init (JsonrpcClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = jsonrpc_client_constructed;
  object_class->dispose = jsonrpc_client_dispose;
  object_class->get_property = jsonrpc_client_get_property;
  object_class->set_property = jsonrpc_client_set_property;

  /**
   * JsonrpcClient:io-stream:
   *
   * The "io-stream" property is the [class@Gio.IOStream] to use for communicating
   * with a JSON-RPC peer.
   *
   * Since: 3.26
   */
  properties [PROP_IO_STREAM] =
    g_param_spec_object ("io-stream",
                         "IO Stream",
                         "The stream to communicate over",
                         G_TYPE_IO_STREAM,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * JsonrpcClient:use-gvariant:
   *
   * The "use-gvariant" property denotes if [struct@GLib.Variant] should be used to
   * communicate with the peer instead of JSON. You should only set this
   * if you know the peer is also a Jsonrpc-GLib based client.
   *
   * Setting this property allows the peers to communicate using GVariant
   * instead of JSON. This means that we can access the messages without
   * expensive memory allocations and parsing costs associated with JSON.
   * [struct@GLib.Variant] is much more optimal for memory-bassed message passing.
   *
   * Since: 3.26
   */
  properties [PROP_USE_GVARIANT] =
    g_param_spec_boolean ("use-gvariant",
                          "Use GVariant",
                          "If GVariant encoding should be used",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * JsonrpcClient::failed:
   *
   * The "failed" signal is called when the client has failed communication
   * or the connection has been knowingly closed.
   *
   * Since: 3.28
   */
  signals [FAILED] =
    g_signal_new ("failed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonrpcClientClass, failed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * JsonrpcClient::handle-call:
   * @self: A #JsonrpcClient
   * @method: The method name
   * @id: The "id" field of the JSONRPC message
   * @params: (nullable): The "params" field of the JSONRPC message
   *
   * This signal is emitted when an RPC has been received from the peer we
   * are connected to. Return %TRUE if you have handled this message, even
   * asynchronously. If no handler has returned %TRUE an error will be
   * synthesized.
   *
   * If you handle the message, you are responsible for replying to the peer
   * in a timely manner using [method@Client.reply] or [method@Client.reply_async].
   *
   * Additionally, since 3.28 you may connect to the "detail" of this signal
   * to handle a specific method call. Use the method name as the detail of
   * the signal.
   *
   * Since: 3.26
   */
  signals [HANDLE_CALL] =
    g_signal_new ("handle-call",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (JsonrpcClientClass, handle_call),
                  g_signal_accumulator_true_handled, NULL,
                  _jsonrpc_marshal_BOOLEAN__STRING_VARIANT_VARIANT,
                  G_TYPE_BOOLEAN,
                  3,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_VARIANT,
                  G_TYPE_VARIANT);
  g_signal_set_va_marshaller (signals [HANDLE_CALL],
                              G_TYPE_FROM_CLASS (klass),
                              _jsonrpc_marshal_BOOLEAN__STRING_VARIANT_VARIANTv);

  /**
   * JsonrpcClient::notification:
   * @self: A #JsonrpcClient
   * @method: The method name of the notification
   * @params: (nullable): Params for the notification
   *
   * This signal is emitted when a notification has been received from a
   * peer. Unlike [signal@Client::handle-call], this does not have an "id"
   * parameter because notifications do not have ids. They do not round
   * trip.
   *
   * Since: 3.26
   */
  signals [NOTIFICATION] =
    g_signal_new ("notification",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (JsonrpcClientClass, notification),
                  NULL, NULL,
                  _jsonrpc_marshal_VOID__STRING_VARIANT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_VARIANT);
  g_signal_set_va_marshaller (signals [NOTIFICATION],
                              G_TYPE_FROM_CLASS (klass),
                              _jsonrpc_marshal_VOID__STRING_VARIANTv);
}

static void
jsonrpc_client_init (JsonrpcClient *self)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  priv->invocations = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  priv->is_first_call = TRUE;
  priv->read_loop_cancellable = g_cancellable_new ();
}

/**
 * jsonrpc_client_new:
 * @io_stream: A [class@Gio.IOStream]
 *
 * Creates a new #JsonrpcClient instance.
 *
 * If you want to communicate with a process using stdin/stdout, consider using
 * [class@Gio.Subprocess] to launch the process and create a [class@Gio.SimpleIOStream] using the
 * [method@Gio.Subprocess.get_stdin_pipe] and [method@Gio.Subprocess.get_stdout_pipe].
 *
 * Returns: (transfer full): A newly created #JsonrpcClient
 *
 * Since: 3.26
 */
JsonrpcClient *
jsonrpc_client_new (GIOStream *io_stream)
{
  g_return_val_if_fail (G_IS_IO_STREAM (io_stream), NULL);

  return g_object_new (JSONRPC_TYPE_CLIENT,
                       "io-stream", io_stream,
                       NULL);
}

static void
jsonrpc_client_remove_from_invocations (JsonrpcClient *self,
                                        GTask         *task)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  gpointer id;

  g_assert (JSONRPC_IS_CLIENT (self));
  g_assert (G_IS_TASK (task));

  id = g_task_get_task_data (task);

  g_hash_table_remove (priv->invocations, id);
}

static void
jsonrpc_client_call_notify_completed (JsonrpcClient *self,
                                      GParamSpec    *pspec,
                                      GTask         *task)
{
  g_assert (JSONRPC_IS_CLIENT (self));
  g_assert (pspec != NULL);
  g_assert (g_str_equal (pspec->name, "completed"));
  g_assert (G_IS_TASK (task));

  jsonrpc_client_remove_from_invocations (self, task);
}

static void
jsonrpc_client_call_write_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  JsonrpcOutputStream *stream = (JsonrpcOutputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  JsonrpcClient *self;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (JSONRPC_IS_CLIENT (self));

  if (!jsonrpc_output_stream_write_message_finish (stream, result, &error))
    {
      /* Panic will cancel our task, no need to return error here */
      jsonrpc_client_panic (self, error);
      return;
    }

  /* We don't need to complete the task because it will get completed when the
   * server replies with our return value. This is performed using an
   * asynchronous read that will pump through incoming messages.
   */
}

static void
jsonrpc_client_call_read_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  JsonrpcInputStream *stream = (JsonrpcInputStream *)object;
  g_autoptr(JsonrpcClient) self = user_data;
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GVariant) message = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariantDict) dict = NULL;

  g_assert (JSONRPC_IS_INPUT_STREAM (stream));
  g_assert (JSONRPC_IS_CLIENT (self));

  if (!jsonrpc_input_stream_read_message_finish (stream, result, &message, &error))
    {
      /* Handle jsonrpc_client_close() conditions gracefully. */
      if (priv->in_shutdown &&
          g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      /*
       * If we fail to read a message, that means we couldn't even receive
       * a message describing the error. All we can do in this case is panic
       * and shutdown the whole client.
       */
      jsonrpc_client_panic (self, error);
      return;
    }

  g_assert (message != NULL);

  /* If we received a gvariant-based message, upgrade connection */
  if (_jsonrpc_input_stream_get_has_seen_gvariant (stream))
    jsonrpc_client_set_use_gvariant (self, TRUE);

  /* Make sure we got a proper type back from the variant. */
  if (!g_variant_is_of_type (message, G_VARIANT_TYPE_VARDICT))
    {
      error = g_error_new_literal (G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Improper reply from peer, not a vardict");
      jsonrpc_client_panic (self, error);
      return;
    }

  dict = g_variant_dict_new (message);

  /*
   * If the message is malformed, we'll also need to perform another read.
   * We do this to try to be relaxed against failures. That seems to be
   * the JSONRPC way, although I'm not sure I like the idea.
   */
  if (dict == NULL || !is_jsonrpc_reply (dict))
    {
      error = g_error_new_literal (G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Improper reply from peer");
      jsonrpc_client_panic (self, error);
      return;
    }

  /*
   * If the response does not have an "id" field, then it is a "notification"
   * and we need to emit the "notificiation" signal.
   */
  if (is_jsonrpc_notification (dict))
    {
      g_autoptr(GVariant) params = NULL;
      const gchar *method_name = NULL;

      if (g_variant_dict_lookup (dict, "method", "&s", &method_name))
        {
          GQuark detail = g_quark_try_string (method_name);

          params = g_variant_dict_lookup_value (dict, "params", NULL);
          g_signal_emit (self, signals [NOTIFICATION], detail, method_name, params);
        }

      goto begin_next_read;
    }

  if (is_jsonrpc_result (dict))
    {
      g_autoptr(GVariant) params = NULL;
      gint64 id = -1;
      GTask *task;

      if (!g_variant_dict_lookup (dict, "id", "x", &id) ||
          NULL == (task = g_hash_table_lookup (priv->invocations, GINT_TO_POINTER (id))))
        {
          error = g_error_new_literal (G_IO_ERROR,
                                       G_IO_ERROR_INVALID_DATA,
                                       "Reply to missing or invalid task");
          jsonrpc_client_panic (self, error);
          return;
        }

      if (NULL != (params = g_variant_dict_lookup_value (dict, "result", NULL)))
        g_task_return_pointer (task, g_steal_pointer (&params), (GDestroyNotify)g_variant_unref);
      else
        g_task_return_pointer (task, NULL, NULL);

      goto begin_next_read;
    }

  /*
   * If this is a method call, emit the handle-call signal.
   */
  if (is_jsonrpc_call (dict))
    {
      g_autoptr(GVariant) id = NULL;
      g_autoptr(GVariant) params = NULL;
      const gchar *method_name = NULL;
      gboolean ret = FALSE;
      GQuark detail;

      if (!g_variant_dict_lookup (dict, "method", "&s", &method_name) ||
          NULL == (id = g_variant_dict_lookup_value (dict, "id", NULL)))
        {
          error = g_error_new_literal (G_IO_ERROR,
                                       G_IO_ERROR_INVALID_DATA,
                                       "Call contains invalid method or id field");
          jsonrpc_client_panic (self, error);
          return;
        }

      params = g_variant_dict_lookup_value (dict, "params", NULL);

      g_assert (method_name != NULL);
      g_assert (id != NULL);

      detail = g_quark_try_string (method_name);
      g_signal_emit (self, signals [HANDLE_CALL], detail, method_name, id, params, &ret);

      if (ret == FALSE)
        jsonrpc_client_reply_error_async (self, id, JSONRPC_CLIENT_ERROR_METHOD_NOT_FOUND,
                                          "The method does not exist or is not available",
                                          NULL, NULL, NULL);

      goto begin_next_read;
    }

  /*
   * If we got an error destined for one of our inflight invocations, then
   * we need to dispatch it now.
   */

  if (g_variant_dict_contains (dict, "id") &&
      g_variant_dict_contains (dict, "error"))
    {
      g_autoptr(GVariant) error_variant = NULL;
      g_autofree gchar *errstr = NULL;
      const char *errmsg = NULL;
      gint64 id = -1;
      gint64 errcode = -1;

      error_variant = g_variant_dict_lookup_value (dict, "error", NULL);

      if (error_variant != NULL &&
          g_variant_lookup (error_variant, "message", "&s", &errmsg) &&
          g_variant_lookup (error_variant, "code", "x", &errcode))
        errstr = g_strdup_printf ("%s (%d)", errmsg, (int)errcode);
      else
        errstr = g_variant_print (error_variant, FALSE);

      error = g_error_new_literal (JSONRPC_CLIENT_ERROR, errcode, errstr);

      if (g_variant_dict_lookup (dict, "id", "x", &id))
        {
          GTask *task = g_hash_table_lookup (priv->invocations, GINT_TO_POINTER (id));

          if (task != NULL)
            g_task_return_error (task, g_steal_pointer (&error));
          else
            g_warning ("Received error for task %"G_GINT64_FORMAT" which is unknown", id);

          goto begin_next_read;
        }

      /*
       * Generic error, not tied to any specific task we had in flight. So
       * take this as a failure case and panic on the line.
       */
      jsonrpc_client_panic (self, error);
      return;
    }

  g_warning ("Unhandled RPC from peer!");

begin_next_read:
  if (priv->input_stream != NULL &&
      priv->in_shutdown == FALSE &&
      priv->failed == FALSE)
    jsonrpc_input_stream_read_message_async (priv->input_stream,
                                             priv->read_loop_cancellable,
                                             jsonrpc_client_call_read_cb,
                                             g_steal_pointer (&self));
}

static void
jsonrpc_client_call_sync_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  JsonrpcClient *self = (JsonrpcClient *)object;
  GTask *task = user_data;
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (self, result, &return_value, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&return_value), (GDestroyNotify)g_variant_unref);
}

/**
 * jsonrpc_client_call:
 * @self: A #JsonrpcClient
 * @method: The name of the method to call
 * @params: (transfer none) (nullable): A [struct@GLib.Variant] of parameters or %NULL
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @return_value: (nullable) (out): A location for a [struct@GLib.Variant]
 *
 * Synchronously calls @method with @params on the remote peer.
 *
 * once a reply has been received, or failure, this function will return.
 * If successful, @return_value will be set with the reslut field of
 * the response.
 *
 * If @params is floating then this function consumes the reference.
 *
 * Returns: %TRUE on success; otherwise %FALSE and @error is set.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_call (JsonrpcClient  *self,
                     const gchar    *method,
                     GVariant       *params,
                     GCancellable   *cancellable,
                     GVariant      **return_value,
                     GError        **error)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GMainContext) main_context = NULL;
  g_autoptr(GVariant) local_return_value = NULL;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (method != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  main_context = g_main_context_ref_thread_default ();

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, jsonrpc_client_call);

  jsonrpc_client_call_async (self,
                             method,
                             params,
                             cancellable,
                             jsonrpc_client_call_sync_cb,
                             task);

  while (!g_task_get_completed (task))
    g_main_context_iteration (main_context, TRUE);

  local_return_value = g_task_propagate_pointer (task, error);
  ret = local_return_value != NULL;

  if (return_value != NULL)
    *return_value = g_steal_pointer (&local_return_value);

  return ret;
}

/**
 * jsonrpc_client_call_with_id_async:
 * @self: A #JsonrpcClient
 * @method: The name of the method to call
 * @params: (transfer none) (nullable): A [struct@GLib.Variant] of parameters or %NULL
 * @id: (out) (transfer full) (optional): A location for a [struct@GLib.Variant]
 *   describing the identifier used for the method call, or %NULL.
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: Callback to executed upon completion
 * @user_data: User data for @callback
 *
 * Asynchronously calls @method with @params on the remote peer.
 *
 * Upon completion or failure, @callback is executed and it should
 * call [method@Client.call_finish] to complete the request and release
 * any memory held.
 *
 * This function is similar to [method@Client.call_async] except that
 * it allows the caller to get the id of the command which might be useful
 * in systems where you can cancel the operation (such as the Language
 * Server Protocol).
 *
 * If @params is floating, the floating reference is consumed.
 *
 * Since: 3.30
 */
void
jsonrpc_client_call_with_id_async (JsonrpcClient       *self,
                                   const gchar         *method,
                                   GVariant            *params,
                                   GVariant           **id,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GVariant) message = NULL;
  g_autoptr(GVariant) sunk_variant = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  GVariantDict dict;
  gint64 idval;

  g_return_if_fail (JSONRPC_IS_CLIENT (self));
  g_return_if_fail (method != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (id != NULL)
    *id = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_client_call_async);

  if (params == NULL)
    params = g_variant_new_maybe (G_VARIANT_TYPE_VARIANT, NULL);

  /* If we got a floating reference, we should consume it */
  if (g_variant_is_floating (params))
    sunk_variant = g_variant_ref_sink (params);

  if (!jsonrpc_client_check_ready (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (jsonrpc_client_call_notify_completed),
                           self,
                           G_CONNECT_SWAPPED);

  idval = ++priv->sequence;

  g_task_set_task_data (task, GINT_TO_POINTER (idval), NULL);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "jsonrpc", "s", "2.0");
  g_variant_dict_insert (&dict, "id", "x", idval);
  g_variant_dict_insert (&dict, "method", "s", method);
  g_variant_dict_insert_value (&dict, "params", params);

  message = g_variant_take_ref (g_variant_dict_end (&dict));

  g_hash_table_insert (priv->invocations, GINT_TO_POINTER (idval), g_object_ref (task));

  jsonrpc_output_stream_write_message_async (priv->output_stream,
                                             message,
                                             cancellable,
                                             jsonrpc_client_call_write_cb,
                                             g_steal_pointer (&task));

  if (priv->is_first_call)
    jsonrpc_client_start_listening (self);

  if (id != NULL)
    *id = g_variant_take_ref (g_variant_new_int64 (idval));
}

/**
 * jsonrpc_client_call_async:
 * @self: A #JsonrpcClient
 * @method: The name of the method to call
 * @params: (transfer none) (nullable): A [struct@GLib.Variant] of parameters or %NULL
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: a callback to executed upon completion
 * @user_data: user data for @callback
 *
 * Asynchronously calls @method with @params on the remote peer.
 *
 * Upon completion or failure, @callback is executed and it should
 * call [method@Client.call_finish] to complete the request and release
 * any memory held.
 *
 * If @params is floating, the floating reference is consumed.
 *
 * Since: 3.26
 */
void
jsonrpc_client_call_async (JsonrpcClient       *self,
                           const gchar         *method,
                           GVariant            *params,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  jsonrpc_client_call_with_id_async (self, method, params, NULL, cancellable, callback, user_data);
}

/**
 * jsonrpc_client_call_finish:
 * @self: A #JsonrpcClient.
 * @result: A #GAsyncResult provided to the callback in [method@Client.call_async]
 * @return_value: (out) (nullable): A location for a [struct@GLib.Variant] or %NULL
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous call to [method@Client.call_async].
 *
 * Returns: %TRUE if successful and @return_value is set, otherwise %FALSE and @error is set.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_call_finish (JsonrpcClient  *self,
                            GAsyncResult   *result,
                            GVariant      **return_value,
                            GError        **error)
{
  g_autoptr(GVariant) local_return_value = NULL;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  local_return_value = g_task_propagate_pointer (G_TASK (result), error);
  ret = local_return_value != NULL;

  if (return_value != NULL)
    *return_value = g_steal_pointer (&local_return_value);

  return ret;
}

GQuark
jsonrpc_client_error_quark (void)
{
  return g_quark_from_static_string ("jsonrpc-client-error-quark");
}

static void
jsonrpc_client_send_notification_write_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  JsonrpcOutputStream *stream = (JsonrpcOutputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_output_stream_write_message_finish (stream, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * jsonrpc_client_send_notification:
 * @self: A #JsonrpcClient
 * @method: The name of the method to call
 * @params: (transfer none) (nullable): A [struct@GLib.Variant] of parameters or %NULL
 * @cancellable: (nullable): A #GCancellable or %NULL
 *
 * Synchronously calls @method with @params on the remote peer.
 *
 * This function will not wait or expect a reply from the peer.
 *
 * If @params is floating then the reference is consumed.
 *
 * Returns: %TRUE on success; otherwise %FALSE and @error is set.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_send_notification (JsonrpcClient  *self,
                                  const gchar    *method,
                                  GVariant       *params,
                                  GCancellable   *cancellable,
                                  GError        **error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GVariant) message = NULL;
  GVariantDict dict;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (method != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!jsonrpc_client_check_ready (self, error))
    return FALSE;

  /* Use empty maybe type for NULL params. The floating reference will
   * be consumed below in g_variant_dict_insert_value(). */
  if (params == NULL)
    params = g_variant_new_maybe (G_VARIANT_TYPE_VARIANT, NULL);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "jsonrpc", "s", "2.0");
  g_variant_dict_insert (&dict, "method", "s", method);
  g_variant_dict_insert_value (&dict, "params", params);

  message = g_variant_take_ref (g_variant_dict_end (&dict));

  ret = jsonrpc_output_stream_write_message (priv->output_stream, message, cancellable, error);

  return ret;
}

/**
 * jsonrpc_client_send_notification_async:
 * @self: A #JsonrpcClient
 * @method: The name of the method to call
 * @params: (transfer none) (nullable): A [struct@GLib.Variant] of parameters or %NULL
 * @cancellable: (nullable): A #GCancellable or %NULL
 *
 * Asynchronously calls @method with @params on the remote peer.
 *
 * This function will not wait or expect a reply from the peer.
 *
 * This function is useful when the caller wants to be notified that
 * the bytes have been delivered to the underlying stream. This does
 * not indicate that the peer has received them.
 *
 * If @params is floating then the reference is consumed.
 *
 * Since: 3.26
 */
void
jsonrpc_client_send_notification_async (JsonrpcClient       *self,
                                        const gchar         *method,
                                        GVariant            *params,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GVariant) message = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  GVariantDict dict;

  g_return_if_fail (JSONRPC_IS_CLIENT (self));
  g_return_if_fail (method != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_client_send_notification_async);

  if (!jsonrpc_client_check_ready (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (params == NULL)
    params = g_variant_new_maybe (G_VARIANT_TYPE_VARIANT, NULL);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "jsonrpc", "s", "2.0");
  g_variant_dict_insert (&dict, "method", "s", method);
  g_variant_dict_insert_value (&dict, "params", params);

  message = g_variant_take_ref (g_variant_dict_end (&dict));

  jsonrpc_output_stream_write_message_async (priv->output_stream,
                                             message,
                                             cancellable,
                                             jsonrpc_client_send_notification_write_cb,
                                             g_steal_pointer (&task));
}

/**
 * jsonrpc_client_send_notification_finish:
 * @self: A #JsonrpcClient
 *
 * Completes an asynchronous call to [method@Client.send_notification_async].
 *
 * Successful completion of this function only indicates that the request
 * has been written to the underlying buffer, not that the peer has received
 * the notification.
 *
 * Returns: %TRUE if the bytes have been flushed to the [class@Gio.IOStream]; otherwise
 *   %FALSE and @error is set.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_send_notification_finish (JsonrpcClient  *self,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * jsonrpc_client_close:
 * @self: A #JsonrpcClient
 *
 * Closes the underlying streams and cancels any inflight operations of the
 * #JsonrpcClient.
 *
 * This is important to call when you are done with the
 * client so that any outstanding operations that have caused @self to
 * hold additional references are cancelled.
 *
 * Failure to call this method results in a leak of #JsonrpcClient.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_close (JsonrpcClient  *self,
                      GCancellable   *cancellable,
                      GError        **error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GHashTable) invocations = NULL;
  g_autoptr(GError) local_error = NULL;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!jsonrpc_client_check_ready (self, error))
    return FALSE;

  priv->in_shutdown = TRUE;

  if (!g_cancellable_is_cancelled (priv->read_loop_cancellable))
    g_cancellable_cancel (priv->read_loop_cancellable);

  /* This can fail from "pending operations", but we will always cancel
   * our tasks. But we should let the caller know either way.
   */
  ret = g_io_stream_close (priv->io_stream, cancellable, error);

  /*
   * Closing the input stream will fail, so just rely on the callback
   * from the async function to complete/close the stream.
   */
  local_error = g_error_new_literal (G_IO_ERROR,
                                     G_IO_ERROR_CLOSED,
                                     "The underlying stream was closed");
  cancel_pending_from_main (self, local_error);

  emit_failed_from_main (self);

  return ret;
}

/**
 * jsonrpc_client_close_async:
 * @self: A #JsonrpcClient.
 *
 * Asynchronous version of [method@Client.close].
 *
 * Currently this operation is implemented synchronously, but in the future may
 * be converted to using asynchronous operations.
 *
 * Since: 3.26
 */
void
jsonrpc_client_close_async (JsonrpcClient       *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (JSONRPC_IS_CLIENT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_client_close_async);

  /*
   * In practice, none of our close operations should block (unless they were
   * a FUSE fd or something like that. So we'll just perform them synchronously
   * for now.
   */
  jsonrpc_client_close (self, cancellable, NULL);

  g_task_return_boolean (task, TRUE);
}

/**
 * jsonrpc_client_close_finish:
 * @self: A #JsonrpcClient.
 *
 * Completes an asynchronous request of [method@Client.close_async].
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_close_finish (JsonrpcClient  *self,
                             GAsyncResult   *result,
                             GError        **error)
{
  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
jsonrpc_client_reply_error_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  JsonrpcOutputStream *stream = (JsonrpcOutputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_output_stream_write_message_finish (stream, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * jsonrpc_client_reply_error_async:
 * @self: A #JsonrpcClient
 * @id: (transfer none): A [struct@GLib.Variant] containing the call id
 * @code: The error code
 * @message: (nullable): An optional error message
 * @cancellable: (nullable): A #GCancellable, or %NULL
 * @callback: (nullable): A #GAsyncReadyCallback or %NULL
 * @user_data: Closure data for @callback
 *
 * Asynchronously replies to the peer, sending a JSON-RPC error message.
 *
 * Call [method@Client.reply_error_finish] to get the result of this operation.
 *
 * If @id is floating, it's floating reference is consumed.
 *
 * Since: 3.28
 */
void
jsonrpc_client_reply_error_async (JsonrpcClient       *self,
                                  GVariant            *id,
                                  gint                 code,
                                  const gchar         *message,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) vreply = NULL;
  GVariantDict reply;
  GVariantDict error_dict;

  g_return_if_fail (JSONRPC_IS_CLIENT (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (message == NULL)
    message = "An error occurred";

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_client_reply_error_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  if (!jsonrpc_client_check_ready (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_variant_dict_init (&error_dict, NULL);
  g_variant_dict_insert (&error_dict, "code", "i", code);
  g_variant_dict_insert (&error_dict, "message", "s", message);

  g_variant_dict_init (&reply, NULL);
  g_variant_dict_insert (&reply, "jsonrpc", "s", "2.0");
  g_variant_dict_insert_value (&reply, "id", id);
  g_variant_dict_insert_value (&reply, "error", g_variant_dict_end (&error_dict));

  vreply = g_variant_take_ref (g_variant_dict_end (&reply));

  jsonrpc_output_stream_write_message_async (priv->output_stream,
                                             vreply,
                                             cancellable,
                                             jsonrpc_client_reply_error_cb,
                                             g_steal_pointer (&task));
}

gboolean
jsonrpc_client_reply_error_finish (JsonrpcClient  *self,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * jsonrpc_client_reply:
 * @self: A #JsonrpcClient
 * @id: (transfer none): The id of the message to reply
 * @result: (transfer none) (nullable): The return value or %NULL
 * @cancellable: (nullable): A #GCancellable, or %NULL
 * @error: A #GError, or %NULL
 *
 * Synchronous variant of [method@Client.reply_async].
 *
 * If @id or @result are floating, there floating references are consumed.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_reply (JsonrpcClient  *self,
                      GVariant       *id,
                      GVariant       *result,
                      GCancellable   *cancellable,
                      GError        **error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GVariant) message = NULL;
  GVariantDict dict;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (id != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!jsonrpc_client_check_ready (self, error))
    return FALSE;

  if (result == NULL)
    result = g_variant_new_maybe (G_VARIANT_TYPE_VARIANT, NULL);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "jsonrpc", "s", "2.0");
  g_variant_dict_insert_value (&dict, "id", id);
  g_variant_dict_insert_value (&dict, "result", result);

  message = g_variant_take_ref (g_variant_dict_end (&dict));

  ret = jsonrpc_output_stream_write_message (priv->output_stream, message, cancellable, error);

  return ret;
}

static void
jsonrpc_client_reply_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  JsonrpcOutputStream *stream = (JsonrpcOutputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_output_stream_write_message_finish (stream, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * jsonrcp_client_reply_async:
 * @self: A #JsonrpcClient
 * @id: (transfer none): The id of the message to reply
 * @result: (transfer none) (nullable): The return value or %NULL
 * @cancellable: A #GCancellable, or %NULL
 * @callback: (nullable): A #GAsyncReadyCallback or %NULL
 * @user_data: Closure data for @callback
 *
 * This function will reply to a method call identified by @id with the
 * result provided. If @result is %NULL, then a null JSON node is returned
 * for the "result" field of the JSONRPC message.
 *
 * JSONRPC allows either peer to call methods on each other, so this
 * method is provided allowing #JsonrpcClient to be used for either
 * side of communications.
 *
 * If no signal handler has handled [signal@Client::handle-call] then
 * an error will be synthesized to the peer.
 *
 * Call [method@Client.reply_finish] to complete the operation. Note
 * that since the peer does not reply to replies, completion of this
 * asynchronous message does not indicate that the peer has received
 * the message.
 *
 * If @id or @result are floating, there floating references are consumed.
 *
 * Since: 3.26
 */
void
jsonrpc_client_reply_async (JsonrpcClient       *self,
                            GVariant            *id,
                            GVariant            *result,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  g_autoptr(GVariant) message = NULL;
  g_autoptr(GError) error = NULL;
  GVariantDict dict;

  g_return_if_fail (JSONRPC_IS_CLIENT (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_client_reply_async);

  if (!jsonrpc_client_check_ready (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (result == NULL)
    result = g_variant_new_maybe (G_VARIANT_TYPE_VARIANT, NULL);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "jsonrpc", "s", "2.0");
  g_variant_dict_insert_value (&dict, "id", id);
  g_variant_dict_insert_value (&dict, "result", result);

  message = g_variant_take_ref (g_variant_dict_end (&dict));

  jsonrpc_output_stream_write_message_async (priv->output_stream,
                                             message,
                                             cancellable,
                                             jsonrpc_client_reply_cb,
                                             g_steal_pointer (&task));
}

/**
 * jsonrpc_client_reply_finish:
 * @self: A #JsonrpcClient
 * @result: A #GAsyncResult
 * @error: A location for a #GError or %NULL
 *
 * Completes an asynchronous request to [method@Client.reply_async].
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_reply_finish (JsonrpcClient  *self,
                             GAsyncResult   *result,
                             GError        **error)
{
  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * jsonrpc_client_start_listening:
 * @self: A #JsonrpcClient
 *
 * This function requests that client start processing incoming
 * messages from the peer.
 *
 * Since: 3.26
 */
void
jsonrpc_client_start_listening (JsonrpcClient *self)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  g_return_if_fail (JSONRPC_IS_CLIENT (self));

  /*
   * If this is our very first message, then we need to start our
   * async read loop. This will allow us to receive notifications
   * out-of-band and intermixed with RPC calls.
   */

  if (priv->is_first_call)
    {
      priv->is_first_call = FALSE;

      /*
       * Because we take a reference here in our read loop, it is important
       * that the user calls jsonrpc_client_close() or
       * jsonrpc_client_close_async() so that we can cancel the operation and
       * allow it to cleanup any outstanding references.
       */
      jsonrpc_input_stream_read_message_async (priv->input_stream,
                                               priv->read_loop_cancellable,
                                               jsonrpc_client_call_read_cb,
                                               g_object_ref (self));
    }
}

/**
 * jsonrpc_client_get_use_gvariant:
 * @self: A #JsonrpcClient
 *
 * Gets the [property@Client:use-gvariant] property.
 *
 * Indicates if [struct@GLib.Variant] is being used to communicate with the peer.
 *
 * Returns: %TRUE if [struct@GLib.Variant] is being used; otherwise %FALSE.
 *
 * Since: 3.26
 */
gboolean
jsonrpc_client_get_use_gvariant (JsonrpcClient *self)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);

  return priv->use_gvariant;
}

/**
 * jsonrpc_client_set_use_gvariant:
 * @self: A #JsonrpcClient
 * @use_gvariant: If [struct@GLib.Variant] should be used
 *
 * Sets the [property@Client:use-gvariant] property.
 *
 * This function sets if [struct@GLib.Variant] should be used to communicate with the
 * peer. Doing so can allow for more efficient communication by avoiding
 * expensive parsing overhead and memory allocations. However, it requires
 * that the peer also supports [struct@GLib.Variant] encoding.
 *
 * Since: 3.26
 */
void
jsonrpc_client_set_use_gvariant (JsonrpcClient *self,
                                 gboolean       use_gvariant)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  g_return_if_fail (JSONRPC_IS_CLIENT (self));

  use_gvariant = !!use_gvariant;

  if (priv->use_gvariant != use_gvariant)
    {
      priv->use_gvariant = use_gvariant;
      if (priv->output_stream != NULL)
        jsonrpc_output_stream_set_use_gvariant (priv->output_stream, use_gvariant);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_GVARIANT]);
    }
}
