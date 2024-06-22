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

#ifndef LSP_PROGRESS_H
#define LSP_PROGRESS_H 1

#include "lsp-server.h"

#include <glib.h>


typedef struct
{
	gint token_int;
	gchar *token_str;
} LspProgressToken;


void lsp_progress_create(LspServer *server, LspProgressToken token);

void lsp_progress_process_notification(LspServer *srv, GVariant *params);

void lsp_progress_free_all(LspServer *server);

#endif  /* LSP_PROGRESS_H */
