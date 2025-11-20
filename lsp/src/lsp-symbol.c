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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lsp-symbol.h"


typedef struct LspSymbol
{
	gchar *name;
	gchar *detail;
	gchar *scope;
	gchar *file;
	GeanyFiletypeID ft_id;
	gulong line;
	gulong pos;
	glong kind;
	guint icon;

	gint refcount; /* the reference count of the symbol */
} LspSymbol;


LspSymbol *lsp_symbol_new(const gchar *name, const gchar *detail, const gchar *scope, const gchar *file,
	GeanyFiletypeID ft_id, glong kind, gulong line, gulong pos, guint icon)
{
	LspSymbol *sym = g_slice_new0(LspSymbol);
	sym->refcount = 1;

	sym->name = g_strdup(name);
	sym->detail = g_strdup(detail);
	sym->scope = g_strdup(scope);
	sym->file = g_strdup(file);
	sym->ft_id = ft_id;
	sym->kind = kind;
	sym->line = line;
	sym->pos = pos;
	sym->icon = icon;

	return sym;
}


LspSymbol *lsp_symbol_new_from_tag(TMTag *tag)
{
	LspSymbol *sym = g_slice_new0(LspSymbol);
	sym->refcount = 1;
	return sym;
}


static void symbol_destroy(LspSymbol *sym)
{
	g_free(sym->name);
	g_free(sym->detail);
	g_free(sym->scope);
	g_free(sym->file);
}


GType lsp_symbol_get_type(void)
{
	static GType gtype = 0;
	if (G_UNLIKELY (gtype == 0))
	{
		gtype = g_boxed_type_register_static("LspSymbol", (GBoxedCopyFunc)lsp_symbol_ref,
											 (GBoxedFreeFunc)lsp_symbol_unref);
	}
	return gtype;
}


void lsp_symbol_unref(LspSymbol *sym)
{
	if (sym && g_atomic_int_dec_and_test(&sym->refcount))
	{
		symbol_destroy(sym);
		g_slice_free(LspSymbol, sym);
	}
}


LspSymbol *lsp_symbol_ref(LspSymbol *sym)
{
	g_atomic_int_inc(&sym->refcount);
	return sym;
}


gulong lsp_symbol_get_line(const LspSymbol *sym)
{
	return sym->line;
}


gulong lsp_symbol_get_pos(const LspSymbol *sym)
{
	return sym->pos;
}


const gchar *lsp_symbol_get_scope(const LspSymbol *sym)
{
	return sym->scope;
}


const gchar *lsp_symbol_get_name(const LspSymbol *sym)
{
	return sym->name;
}


const gchar *lsp_symbol_get_file(const LspSymbol *sym)
{
	return sym->file;
}


const gchar *lsp_symbol_get_detail(const LspSymbol *sym)
{
	return sym->detail;
}


glong lsp_symbol_get_kind(const LspSymbol *sym)
{
	return sym->kind;
}


TMIcon lsp_symbol_get_icon(const LspSymbol *sym)
{
	return sym->icon;
}


gchar *lsp_symbol_get_name_with_scope(const LspSymbol *sym)
{
	gchar *name = NULL;

	if (EMPTY(sym->scope))
		name = g_strdup(sym->name);
	else
		name = g_strconcat(sym->scope, LSP_SCOPE_SEPARATOR, sym->name, NULL);

	return name;
}


gchar *lsp_symbol_get_symtree_name(const LspSymbol *sym, gboolean include_scope)
{
	GString *buffer;

	if (include_scope && !EMPTY(sym->scope))
	{
		buffer = g_string_new(sym->scope);
		g_string_append(buffer, LSP_SCOPE_SEPARATOR);
		g_string_append(buffer, sym->name);
	}
	else
		buffer = g_string_new(sym->name);

	g_string_append_printf(buffer, " [%lu]", sym->line);

	return g_string_free(buffer, FALSE);
}


gchar *lsp_symbol_get_symtree_tooltip(const LspSymbol *sym, const gchar *encoding)
{
	return g_strdup(sym->detail);
}


gboolean lsp_symbol_equal(const LspSymbol *a, const LspSymbol *b)
{
	return a->line == b->line && a->pos == b->pos &&
		a->kind == b->kind && a->ft_id == b->ft_id &&
		g_strcmp0(a->name, b->name) == 0 &&
		g_strcmp0(a->file, b->file) == 0 &&
		g_strcmp0(a->scope, b->scope) == 0 &&
		g_strcmp0(a->detail, b->detail) == 0;
}
