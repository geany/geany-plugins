/* jsonrpc-server.h
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

#ifndef JSONRPC_SERVER_H
#define JSONRPC_SERVER_H

#include <gio/gio.h>

#include "jsonrpc-client.h"
#include "jsonrpc-version-macros.h"

G_BEGIN_DECLS

#define JSONRPC_TYPE_SERVER (jsonrpc_server_get_type())

JSONRPC_AVAILABLE_IN_3_26
G_DECLARE_DERIVABLE_TYPE (JsonrpcServer, jsonrpc_server, JSONRPC, SERVER, GObject)

struct _JsonrpcServerClass
{
  GObjectClass parent_class;

  gboolean (*handle_call)     (JsonrpcServer *self,
                               JsonrpcClient *client,
                               const gchar   *method,
                               GVariant      *id,
                               GVariant      *params);
  void     (*notification)    (JsonrpcServer *self,
                               JsonrpcClient *client,
                               const gchar   *method,
                               GVariant      *params);
  void     (*client_accepted) (JsonrpcServer *self,
                               JsonrpcClient *client);
  void     (*client_closed)   (JsonrpcServer *self,
                               JsonrpcClient *client);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
};

typedef void (*JsonrpcServerHandler) (JsonrpcServer *self,
                                      JsonrpcClient *client,
                                      const gchar   *method,
                                      GVariant      *id,
                                      GVariant      *params,
                                      gpointer       user_data);

JSONRPC_AVAILABLE_IN_3_26
JsonrpcServer *jsonrpc_server_new              (void);
JSONRPC_AVAILABLE_IN_3_26
void           jsonrpc_server_accept_io_stream (JsonrpcServer        *self,
                                                GIOStream            *io_stream);
JSONRPC_AVAILABLE_IN_3_26
guint          jsonrpc_server_add_handler      (JsonrpcServer        *self,
                                                const gchar          *method,
                                                JsonrpcServerHandler  handler,
                                                gpointer              handler_data,
                                                GDestroyNotify        handler_data_destroy);
JSONRPC_AVAILABLE_IN_3_26
void           jsonrpc_server_remove_handler   (JsonrpcServer        *self,
                                                guint                 handler_id);
JSONRPC_AVAILABLE_IN_3_28
void           jsonrpc_server_foreach          (JsonrpcServer        *self,
                                                GFunc                 foreach_func,
                                                gpointer              user_data);

G_END_DECLS

#endif /* JSONRPC_SERVER_H */
