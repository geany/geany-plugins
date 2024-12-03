/*
 * Copyright 2024 Jiri Techet <techet@gmail.com>
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

#ifndef LSP_WORKSPACE_FOLDERS_H
#define LSP_WORKSPACE_FOLDERS_H 1

#include "lsp-server.h"

void lsp_workspace_folders_init(LspServer *srv);
void lsp_workspace_folders_free(LspServer *srv);

void lsp_workspace_folders_add_project_root(LspServer *srv);

void lsp_workspace_folders_doc_open(GeanyDocument *doc);
void lsp_workspace_folders_doc_closed(GeanyDocument *doc);

GPtrArray *lsp_workspace_folders_get(LspServer *srv);

#endif   /* LSP_WORKSPACE_FOLDERS */
