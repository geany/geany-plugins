/*
 * Copyright 2023 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef LSP_RPC_H
#define LSP_RPC_H 1


#include "lsp-server.h"


typedef void (*LspRpcCallback) (GVariant *return_value, GError *error, gpointer user_data);


struct LspRpc;
typedef struct LspRpc LspRpc;


LspRpc *lsp_rpc_new(LspServer *srv, GIOStream *stream);
void lsp_rpc_destroy(LspRpc *rpc);

void lsp_rpc_call(LspServer *srv, const gchar *method, GVariant *params,
	LspRpcCallback callback, gpointer user_data);

void lsp_rpc_call_startup_shutdown(LspServer *srv, const gchar *method, GVariant *params,
	LspRpcCallback callback, gpointer user_data);

void lsp_rpc_notify(LspServer *srv, const gchar *method, GVariant *params,
	LspRpcCallback callback, gpointer user_data);


#endif  /* LSP_RPC_H */
