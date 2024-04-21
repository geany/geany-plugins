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

#include <glib.h>


typedef struct
{
	gchar *name;
	gchar *file_name;
	gchar *scope;
	gchar *tooltip;
	gint line;
	TMIcon icon;
} LspSymbol;


void lsp_symbol_free(LspSymbol *symbol);


#ifndef HAVE_GEANY_PLUGIN_EXTENSION_DOC_SYMBOLS

#include <geanyplugin.h>


TMTag *tm_tag_new(void);

void tm_tag_unref(TMTag *tag);

TMTag *tm_tag_ref(TMTag *tag);
#endif

#endif  /* LSP_SYMBOL_H */
