/* jsonrpc-input-stream.h
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

#ifndef JSONRPC_INPUT_STREAM_H
#define JSONRPC_INPUT_STREAM_H

#include <gio/gio.h>

#include "jsonrpc-version-macros.h"

G_BEGIN_DECLS

#define JSONRPC_TYPE_INPUT_STREAM (jsonrpc_input_stream_get_type())

JSONRPC_AVAILABLE_IN_3_26
G_DECLARE_DERIVABLE_TYPE (JsonrpcInputStream, jsonrpc_input_stream, JSONRPC, INPUT_STREAM, GDataInputStream)

struct _JsonrpcInputStreamClass
{
  GDataInputStreamClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

JSONRPC_AVAILABLE_IN_3_26
JsonrpcInputStream *jsonrpc_input_stream_new                 (GInputStream         *base_stream);
JSONRPC_AVAILABLE_IN_3_26
gboolean            jsonrpc_input_stream_read_message        (JsonrpcInputStream   *self,
                                                              GCancellable         *cancellable,
                                                              GVariant            **message,
                                                              GError              **error);
JSONRPC_AVAILABLE_IN_3_26
void                jsonrpc_input_stream_read_message_async  (JsonrpcInputStream   *self,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
JSONRPC_AVAILABLE_IN_3_26
gboolean            jsonrpc_input_stream_read_message_finish (JsonrpcInputStream   *self,
                                                              GAsyncResult         *result,
                                                              GVariant            **message,
                                                              GError              **error);

G_END_DECLS

#endif /* JSONRPC_INPUT_STREAM_H */
