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

#ifndef LSP_SIGNATURE_H
#define LSP_SIGNATURE_H 1

#include "lsp-server.h"

#include <glib.h>

void lsp_signature_send_request(LspServer *server, GeanyDocument *doc, gboolean force);

void lsp_signature_show_prev(void);
void lsp_signature_show_next(void);

void lsp_signature_hide_calltip(GeanyDocument *doc);
gboolean lsp_signature_showing_calltip(GeanyDocument *doc);

#endif  /* LSP_SIGNATURE_H */
