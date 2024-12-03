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

#ifndef LSP_SYMBOL_H
#define LSP_SYMBOL_H 1

#include "lsp-symbol-kinds.h"

#include <geanyplugin.h>
#include <glib.h>

#define LSP_SCOPE_SEPARATOR "->"

struct LspSymbol;
typedef struct LspSymbol LspSymbol;

/* The GType for a LspSymbol */
#define LSP_TYPE_SYMBOL (lsp_symbol_get_type())

GType lsp_symbol_get_type(void) G_GNUC_CONST;
void lsp_symbol_unref(LspSymbol *sym);
LspSymbol *lsp_symbol_ref(LspSymbol *sym);

LspSymbol *lsp_symbol_new(const gchar *name, const gchar *detail, const gchar *scope, const gchar *file,
	GeanyFiletypeID ft_id, glong kind, gulong line, gulong pos, guint icon);

gulong lsp_symbol_get_line(const LspSymbol *sym);
gulong lsp_symbol_get_pos(const LspSymbol *sym);
glong lsp_symbol_get_kind(const LspSymbol *sym);
TMIcon lsp_symbol_get_icon(const LspSymbol *sym);
const gchar *lsp_symbol_get_scope(const LspSymbol *sym);
const gchar *lsp_symbol_get_name(const LspSymbol *sym);
const gchar *lsp_symbol_get_file(const LspSymbol *sym);
const gchar *lsp_symbol_get_detail(const LspSymbol *sym);

gchar *lsp_symbol_get_name_with_scope(const LspSymbol *sym);

gchar *lsp_symbol_get_symtree_name(const LspSymbol *sym, gboolean include_scope);
gchar *lsp_symbol_get_symtree_tooltip(const LspSymbol *sym, const gchar *encoding);

gboolean lsp_symbol_equal(const LspSymbol *a, const LspSymbol *b);

G_END_DECLS

#endif  /* LSP_SYMBOL_H */
