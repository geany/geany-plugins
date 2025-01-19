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

#ifndef LSP_DIAGNOSTICS_H
#define LSP_DIAGNOSTICS_H 1

#include "lsp-server.h"

#include <glib.h>

void lsp_diagnostics_common_destroy(void);

void lsp_diagnostics_init(LspServer *srv);
void lsp_diagnostics_free(LspServer *srv);

void lsp_diagnostics_show_calltip(gint pos);
void lsp_diagnostics_hide_calltip(GeanyDocument *doc);

void lsp_diagnostics_show_all(gboolean current_doc_only);

void lsp_diagnostics_received(LspServer *srv, GVariant* diags);
void lsp_diagnostics_redraw(GeanyDocument *doc);
void lsp_diagnostics_clear(LspServer *srv, GeanyDocument *doc);

void lsp_diagnostics_style_init(GeanyDocument *doc);

gboolean lsp_diagnostics_has_diag(gint pos);
GVariant *lsp_diagnostics_get_diag_raw(gint pos);

void lsp_diagnostics_goto_next_diag(gint pos);
void lsp_diagnostics_goto_prev_diag(gint pos);

#endif  /* LSP_DIAGNOSTICS_H */
