/* jsonrpc-output-stream.h
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

#ifndef JSONRPC_OUTPUT_STREAM_H
#define JSONRPC_OUTPUT_STREAM_H

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "jsonrpc-version-macros.h"

G_BEGIN_DECLS

#define JSONRPC_TYPE_OUTPUT_STREAM (jsonrpc_output_stream_get_type())

JSONRPC_AVAILABLE_IN_3_26
G_DECLARE_DERIVABLE_TYPE (JsonrpcOutputStream, jsonrpc_output_stream, JSONRPC, OUTPUT_STREAM, GDataOutputStream)

struct _JsonrpcOutputStreamClass
{
  GDataOutputStreamClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
};

JSONRPC_AVAILABLE_IN_3_26
JsonrpcOutputStream *jsonrpc_output_stream_new                  (GOutputStream        *base_stream);
JSONRPC_AVAILABLE_IN_3_26
gboolean             jsonrpc_output_stream_get_use_gvariant     (JsonrpcOutputStream  *self);
JSONRPC_AVAILABLE_IN_3_26
void                 jsonrpc_output_stream_set_use_gvariant     (JsonrpcOutputStream  *self,
                                                                 gboolean              use_gvariant);
JSONRPC_AVAILABLE_IN_3_26
gboolean             jsonrpc_output_stream_write_message        (JsonrpcOutputStream  *self,
                                                                 GVariant             *message,
                                                                 GCancellable         *cancellable,
                                                                 GError              **error);
JSONRPC_AVAILABLE_IN_3_26
void                 jsonrpc_output_stream_write_message_async  (JsonrpcOutputStream  *self,
                                                                 GVariant             *message,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
JSONRPC_AVAILABLE_IN_3_26
gboolean             jsonrpc_output_stream_write_message_finish (JsonrpcOutputStream  *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);

G_END_DECLS

#endif /* JSONRPC_OUTPUT_STREAM_H */
