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

#ifndef LSP_SYNC_H
#define LSP_SYNC_H 1


#include "lsp-server.h"
#include "lsp-utils.h"

void lsp_sync_init(LspServer *server);
void lsp_sync_free(LspServer *server);

void lsp_sync_text_document_did_open(LspServer *server, GeanyDocument *doc);
void lsp_sync_text_document_did_close(LspServer *server, GeanyDocument *doc);
void lsp_sync_text_document_did_save(LspServer *server, GeanyDocument *doc);
void lsp_sync_text_document_did_change(LspServer *server, GeanyDocument *doc,
	LspPosition pos_start, LspPosition pos_end, gchar *text);

gboolean lsp_sync_is_document_open(LspServer *server, GeanyDocument *doc);

#endif  /* LSP_SYNC_H */
