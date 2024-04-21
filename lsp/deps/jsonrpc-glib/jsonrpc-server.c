/* jsonrpc-server.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#define G_LOG_DOMAIN "jsonrpc-server"

#include "config.h"

#include <stdlib.h>

#include "jsonrpc-input-stream.h"
#include "jsonrpc-marshalers.h"
#include "jsonrpc-output-stream.h"
#include "jsonrpc-server.h"

/**
 * JsonrpcServer:
 * 
 * A server for JSON-RPC communication
 *
 * The #JsonrpcServer class can help you implement a JSON-RPC server. You can
 * accept connections and then communicate with clients using the
 * [class@Client] API.
 */

typedef struct
{
  GHashTable *clients;
  GArray     *handlers;
  guint       last_handler_id;
} JsonrpcServerPrivate;

typedef struct
{
  const gchar          *method;
  JsonrpcServerHandler  handler;
  gpointer              handler_data;
  GDestroyNotify        handler_data_destroy;
  guint                 handler_id;
} JsonrpcServerHandlerData;

G_DEFINE_TYPE_WITH_PRIVATE (JsonrpcServer, jsonrpc_server, G_TYPE_OBJECT)

enum {
  HANDLE_CALL,
  NOTIFICATION,
  CLIENT_ACCEPTED,
  CLIENT_CLOSED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
jsonrpc_server_clear_handler_data (JsonrpcServerHandlerData *data)
{
  if (data->handler_data_destroy)
    data->handler_data_destroy (data->handler_data);
}

static gint
locate_handler_by_method (const void *key,
                          const void *element)
{
  const gchar *method = key;
  const JsonrpcServerHandlerData *data = element;

  return g_strcmp0 (method, data->method);
}

static gboolean
jsonrpc_server_real_handle_call (JsonrpcServer *self,
                                 JsonrpcClient *client,
                                 const gchar   *method,
                                 GVariant      *id,
                                 GVariant      *params)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);
  JsonrpcServerHandlerData *data;

  g_assert (JSONRPC_IS_SERVER (self));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (method != NULL);
  g_assert (id != NULL);

  data = bsearch (method, (gpointer)priv->handlers->data,
                  priv->handlers->len, sizeof (JsonrpcServerHandlerData),
                  locate_handler_by_method);

  if (data != NULL)
    {
      data->handler (self, client, method, id, params, data->handler_data);
      return TRUE;
    }

  return FALSE;
}

static void
jsonrpc_server_dispose (GObject *object)
{
  JsonrpcServer *self = (JsonrpcServer *)object;
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);

  g_clear_pointer (&priv->clients, g_hash_table_unref);
  g_clear_pointer (&priv->handlers, g_array_unref);

  G_OBJECT_CLASS (jsonrpc_server_parent_class)->dispose (object);
}

static void
jsonrpc_server_class_init (JsonrpcServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = jsonrpc_server_dispose;

  klass->handle_call = jsonrpc_server_real_handle_call;

  /**
   * JsonrpcServer::handle-call:
   * @self: A #JsonrpcServer
   * @client: A #JsonrpcClient
   * @method: The method that was called
   * @id: The identifier of the method call
   * @params: The parameters of the method call
   *
   * This method is emitted when the client requests a method call.
   *
   * If you return %TRUE from this function, you should reply to it (even upon
   * failure), using [method@Client.reply] or [method@Client.reply_async].
   *
   * Returns: %TRUE if the request was handled.
   *
   * Since: 3.26
   */
  signals [HANDLE_CALL] =
    g_signal_new ("handle-call",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonrpcServerClass, handle_call),
                  g_signal_accumulator_true_handled, NULL,
                  _jsonrpc_marshal_BOOLEAN__OBJECT_STRING_VARIANT_VARIANT,
                  G_TYPE_BOOLEAN,
                  4,
                  JSONRPC_TYPE_CLIENT,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_VARIANT,
                  G_TYPE_VARIANT);
  g_signal_set_va_marshaller (signals [HANDLE_CALL],
                              G_TYPE_FROM_CLASS (klass),
                              _jsonrpc_marshal_BOOLEAN__OBJECT_STRING_VARIANT_VARIANTv);

  /**
   * JsonrpcServer::notification:
   * @self: A #JsonrpcServer
   * @client: A #JsonrpcClient
   * @method: The notification name
   * @id: The params for the notification
   *
   * This signal is emitted when the client has sent a notification to us.
   *
   * Since: 3.26
   */
  signals [NOTIFICATION] =
    g_signal_new ("notification",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonrpcServerClass, notification),
                  NULL, NULL,
                  _jsonrpc_marshal_VOID__OBJECT_STRING_VARIANT,
                  G_TYPE_NONE,
                  3,
                  JSONRPC_TYPE_CLIENT,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_VARIANT);
  g_signal_set_va_marshaller (signals [NOTIFICATION],
                              G_TYPE_FROM_CLASS (klass),
                              _jsonrpc_marshal_VOID__OBJECT_STRING_VARIANTv);

  /**
   * JsonrpcServer::client-accepted:
   * @self: A #JsonrpcServer
   * @client: A #JsonrpcClient
   *
   * This signal is emitted when a new client has been accepted.
   *
   * Since: 3.28
   */
  signals [CLIENT_ACCEPTED] =
    g_signal_new ("client-accepted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonrpcServerClass, client_accepted),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  JSONRPC_TYPE_CLIENT);

  /**
   * JsonrpcServer::client-closed:
   * @self: A #JsonrpcServer
   * @client: A #JsonrpcClient
   *
   * This signal is emitted when a new client has been lost.
   *
   * Since: 3.30
   */
  signals [CLIENT_CLOSED] =
    g_signal_new ("client-closed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonrpcServerClass, client_closed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  JSONRPC_TYPE_CLIENT);
}

static void
jsonrpc_server_init (JsonrpcServer *self)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);

  priv->clients = g_hash_table_new_full (NULL, NULL, g_object_unref, NULL);

  priv->handlers = g_array_new (FALSE, FALSE, sizeof (JsonrpcServerHandlerData));
  g_array_set_clear_func (priv->handlers, (GDestroyNotify)jsonrpc_server_clear_handler_data);
}

/**
 * jsonrpc_server_new:
 *
 * Creates a new #JsonrpcServer.
 *
 * Returns: (transfer full): A newly created #JsonrpcServer instance.
 *
 * Since: 3.26
 */
JsonrpcServer *
jsonrpc_server_new (void)
{
  return g_object_new (JSONRPC_TYPE_SERVER, NULL);
}

static gboolean
dummy_func (gpointer data)
{
  return G_SOURCE_REMOVE;
}

static void
jsonrpc_server_client_failed (JsonrpcServer *self,
                              JsonrpcClient *client)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);

  g_assert (JSONRPC_IS_SERVER (self));
  g_assert (JSONRPC_IS_CLIENT (client));

  if (priv->clients != NULL &&
      g_hash_table_contains (priv->clients, client))
    {
      /* Release instance from main thread to ensure callers return
       * safely without having to be careful about incrementing ref
       */
      g_debug ("Lost connection to client [%p]", client);
      g_hash_table_steal (priv->clients, client);
      g_signal_emit (self, signals [CLIENT_CLOSED], 0, client);
      g_idle_add_full (G_MAXINT, dummy_func, client, g_object_unref);
    }
}

static gboolean
jsonrpc_server_client_handle_call (JsonrpcServer *self,
                                   const gchar   *method,
                                   JsonNode      *id,
                                   JsonNode      *params,
                                   JsonrpcClient *client)
{
  gboolean ret;

  g_assert (JSONRPC_IS_SERVER (self));
  g_assert (method != NULL);
  g_assert (id != NULL);
  g_assert (params != NULL);
  g_assert (JSONRPC_IS_CLIENT (client));

  g_signal_emit (self, signals [HANDLE_CALL], 0, client, method, id, params, &ret);

  return ret;
}

static void
jsonrpc_server_client_notification (JsonrpcServer *self,
                                    const gchar   *method,
                                    JsonNode      *params,
                                    JsonrpcClient *client)
{
  g_assert (JSONRPC_IS_SERVER (self));
  g_assert (method != NULL);
  g_assert (params != NULL);
  g_assert (JSONRPC_IS_CLIENT (client));

  g_signal_emit (self, signals [NOTIFICATION], 0, client, method, params);
}

/**
 * jsonrpc_server_accept_io_stream:
 * @self: A #JsonrpcServer
 * @io_stream: A #GIOStream
 *
 * This function accepts @io_stream as a new client to the #JsonrpcServer
 * by wrapping it in a #JsonrpcClient and starting the message accept
 * loop.
 *
 * Since: 3.26
 */
void
jsonrpc_server_accept_io_stream (JsonrpcServer *self,
                                 GIOStream     *io_stream)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);
  JsonrpcClient *client;

  g_return_if_fail (JSONRPC_IS_SERVER (self));
  g_return_if_fail (G_IS_IO_STREAM (io_stream));

  client = jsonrpc_client_new (io_stream);

  g_signal_connect_object (client,
                           "failed",
                           G_CALLBACK (jsonrpc_server_client_failed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (client,
                           "handle-call",
                           G_CALLBACK (jsonrpc_server_client_handle_call),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (client,
                           "notification",
                           G_CALLBACK (jsonrpc_server_client_notification),
                           self,
                           G_CONNECT_SWAPPED);

  g_hash_table_insert (priv->clients, client, NULL);

  jsonrpc_client_start_listening (client);

  g_signal_emit (self, signals [CLIENT_ACCEPTED], 0, client);
}

static gint
sort_by_method (gconstpointer a,
                gconstpointer b)
{
  const JsonrpcServerHandlerData *data_a = a;
  const JsonrpcServerHandlerData *data_b = b;

  return g_strcmp0 (data_a->method, data_b->method);
}

/**
 * jsonrpc_server_add_handler:
 * @self: A #JsonrpcServer
 * @method: A method to handle
 * @handler: (closure handler_data) (destroy handler_data_destroy): A handler to
 *   execute when an incoming method matches @methods
 * @handler_data: User data for @handler
 * @handler_data_destroy: A destroy callback for @handler_data
 *
 * Adds a new handler that will be dispatched when a matching @method arrives.
 *
 * Returns: A handler id that can be used to remove the handler with
 *   [method@Server.remove_handler].
 *
 * Since: 3.26
 */
guint
jsonrpc_server_add_handler (JsonrpcServer        *self,
                            const gchar          *method,
                            JsonrpcServerHandler  handler,
                            gpointer              handler_data,
                            GDestroyNotify        handler_data_destroy)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);
  JsonrpcServerHandlerData data;

  g_return_val_if_fail (JSONRPC_IS_SERVER (self), 0);
  g_return_val_if_fail (handler != NULL, 0);

  data.method = g_intern_string (method);
  data.handler = handler;
  data.handler_data = handler_data;
  data.handler_data_destroy = handler_data_destroy;
  data.handler_id = ++priv->last_handler_id;

  g_array_append_val (priv->handlers, data);
  g_array_sort (priv->handlers, sort_by_method);

  return data.handler_id;
}

/**
 * jsonrpc_server_remove_handler:
 * @self: A #JsonrpcServer
 * @handler_id: A handler returned from [method@Server.add_handler]
 *
 * Removes a handler that was previously registered with [method@Server.add_handler].
 *
 * Since: 3.26
 */
void
jsonrpc_server_remove_handler (JsonrpcServer *self,
                               guint          handler_id)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);

  g_return_if_fail (JSONRPC_IS_SERVER (self));
  g_return_if_fail (handler_id != 0);

  for (guint i = 0; i < priv->handlers->len; i++)
    {
      const JsonrpcServerHandlerData *data = &g_array_index (priv->handlers, JsonrpcServerHandlerData, i);

      if (data->handler_id == handler_id)
        {
          g_array_remove_index (priv->handlers, i);
          break;
        }
    }
}

/**
 * jsonrpc_server_foreach:
 * @self: A #JsonrpcServer
 * @foreach_func: (scope call): A callback for each client
 * @user_data: Closure data for @foreach_func
 *
 * Calls @foreach_func for every client connected.
 *
 * Since: 3.28
 */
void
jsonrpc_server_foreach (JsonrpcServer *self,
                        GFunc          foreach_func,
                        gpointer       user_data)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);
  g_autofree gpointer *keys = NULL;
  guint len;

  g_return_if_fail (JSONRPC_IS_SERVER (self));
  g_return_if_fail (foreach_func != NULL);

  keys = g_hash_table_get_keys_as_array (priv->clients, &len);

  for (guint i = 0; i < len; i++)
    {
      JsonrpcClient *client = keys[i];
      g_assert (JSONRPC_IS_CLIENT (client));
      foreach_func (client, user_data);
    }
}
