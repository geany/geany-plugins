/* jsonrpc-glib.h
 *
 * Copyright (C) 2016 Christian Hergert
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef JSONRPC_GLIB_H
#define JSONRPC_GLIB_H

#include <glib.h>

G_BEGIN_DECLS

#define JSONRPC_GLIB_INSIDE
# include "jsonrpc-client.h"
# include "jsonrpc-input-stream.h"
# include "jsonrpc-message.h"
# include "jsonrpc-output-stream.h"
# include "jsonrpc-server.h"
# include "jsonrpc-version.h"
# include "jsonrpc-version-macros.h"
#undef JSONRPC_GLIB_INSIDE

G_END_DECLS

#endif /* JSONRPC_GLIB_H */
