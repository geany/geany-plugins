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

#ifndef LSP_SYMBOL_KINDS_H
#define LSP_SYMBOL_KINDS_H 1

#include <gtk/gtk.h>
#include <geanyplugin.h>


typedef enum {
	LspSymbolKindFile = 1,
	LspSymbolKindModule,
	LspSymbolKindNamespace,
	LspSymbolKindPackage,
	LspSymbolKindClass,
	LspSymbolKindMethod,
	LspSymbolKindProperty,
	LspSymbolKindField,
	LspSymbolKindConstructor,
	LspSymbolKindEnum,
	LspSymbolKindInterface,
	LspSymbolKindFunction,
	LspSymbolKindVariable,
	LspSymbolKindConstant,
	LspSymbolKindString,
	LspSymbolKindNumber,
	LspSymbolKindBoolean,
	LspSymbolKindArray,
	LspSymbolKindObject,
	LspSymbolKindKey,
	LspSymbolKindNull,
	LspSymbolKindEnumMember,
	LspSymbolKindStruct,
	LspSymbolKindEvent,
	LspSymbolKindOperator,
	LspSymbolKindTypeParameter,  // 26
	LSP_SYMBOL_KIND_NUM = LspSymbolKindTypeParameter
} LspSymbolKind;


// keep in sync with LspSymbolKind above
#define LSP_SYMBOL_KINDS \
	JSONRPC_MESSAGE_PUT_INT32(1),\
	JSONRPC_MESSAGE_PUT_INT32(2),\
	JSONRPC_MESSAGE_PUT_INT32(3),\
	JSONRPC_MESSAGE_PUT_INT32(4),\
	JSONRPC_MESSAGE_PUT_INT32(5),\
	JSONRPC_MESSAGE_PUT_INT32(6),\
	JSONRPC_MESSAGE_PUT_INT32(7),\
	JSONRPC_MESSAGE_PUT_INT32(8),\
	JSONRPC_MESSAGE_PUT_INT32(9),\
	JSONRPC_MESSAGE_PUT_INT32(10),\
	JSONRPC_MESSAGE_PUT_INT32(11),\
	JSONRPC_MESSAGE_PUT_INT32(12),\
	JSONRPC_MESSAGE_PUT_INT32(13),\
	JSONRPC_MESSAGE_PUT_INT32(14),\
	JSONRPC_MESSAGE_PUT_INT32(15),\
	JSONRPC_MESSAGE_PUT_INT32(16),\
	JSONRPC_MESSAGE_PUT_INT32(17),\
	JSONRPC_MESSAGE_PUT_INT32(18),\
	JSONRPC_MESSAGE_PUT_INT32(19),\
	JSONRPC_MESSAGE_PUT_INT32(20),\
	JSONRPC_MESSAGE_PUT_INT32(21),\
	JSONRPC_MESSAGE_PUT_INT32(22),\
	JSONRPC_MESSAGE_PUT_INT32(23),\
	JSONRPC_MESSAGE_PUT_INT32(24),\
	JSONRPC_MESSAGE_PUT_INT32(25),\
	JSONRPC_MESSAGE_PUT_INT32(26)


typedef enum {
	LspCompletionKindText = 1,
	LspCompletionKindMethod,
	LspCompletionKindFunction,
	LspCompletionKindConstructor,
	LspCompletionKindField,
	LspCompletionKindVariable,
	LspCompletionKindClass,
	LspCompletionKindInterface,
	LspCompletionKindModule,
	LspCompletionKindProperty,
	LspCompletionKindUnit,
	LspCompletionKindValue,
	LspCompletionKindEnum,
	LspCompletionKindKeyword,
	LspCompletionKindSnippet,
	LspCompletionKindColor,
	LspCompletionKindFile,
	LspCompletionKindReference,
	LspCompletionKindFolder,
	LspCompletionKindEnumMember,
	LspCompletionKindConstant,
	LspCompletionKindStruct,
	LspCompletionKindEvent,
	LspCompletionKindOperator,
	LspCompletionKindTypeParameter,  // 25
	LSP_COMPLETION_KIND_NUM = LspCompletionKindTypeParameter
} LspCompletionKind;


// keep in sync with LspCompletionKind above
#define LSP_COMPLETION_KINDS \
	JSONRPC_MESSAGE_PUT_INT32(1),\
	JSONRPC_MESSAGE_PUT_INT32(2),\
	JSONRPC_MESSAGE_PUT_INT32(3),\
	JSONRPC_MESSAGE_PUT_INT32(4),\
	JSONRPC_MESSAGE_PUT_INT32(5),\
	JSONRPC_MESSAGE_PUT_INT32(6),\
	JSONRPC_MESSAGE_PUT_INT32(7),\
	JSONRPC_MESSAGE_PUT_INT32(8),\
	JSONRPC_MESSAGE_PUT_INT32(9),\
	JSONRPC_MESSAGE_PUT_INT32(10),\
	JSONRPC_MESSAGE_PUT_INT32(11),\
	JSONRPC_MESSAGE_PUT_INT32(12),\
	JSONRPC_MESSAGE_PUT_INT32(13),\
	JSONRPC_MESSAGE_PUT_INT32(14),\
	JSONRPC_MESSAGE_PUT_INT32(15),\
	JSONRPC_MESSAGE_PUT_INT32(16),\
	JSONRPC_MESSAGE_PUT_INT32(17),\
	JSONRPC_MESSAGE_PUT_INT32(18),\
	JSONRPC_MESSAGE_PUT_INT32(19),\
	JSONRPC_MESSAGE_PUT_INT32(20),\
	JSONRPC_MESSAGE_PUT_INT32(21),\
	JSONRPC_MESSAGE_PUT_INT32(22),\
	JSONRPC_MESSAGE_PUT_INT32(23),\
	JSONRPC_MESSAGE_PUT_INT32(24),\
	JSONRPC_MESSAGE_PUT_INT32(25)


TMIcon lsp_symbol_kinds_get_completion_icon(LspCompletionKind kind);
TMIcon lsp_symbol_kinds_get_symbol_icon(LspSymbolKind kind);

LspSymbolKind lsp_symbol_kinds_tm_to_lsp(TMTagType type);

#endif  /* LSP_SYMBOL_KINDS_H */
