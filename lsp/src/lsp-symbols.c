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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lsp-symbols.h"
#include "lsp-rpc.h"
#include "lsp-utils.h"
#include "lsp-sync.h"
#include "lsp-symbol-kinds.h"
#include "lsp-symbol.h"

#include <jsonrpc-glib.h>

#define CACHED_SYMBOLS_KEY "lsp_symbols_cached"

typedef struct {
	GeanyDocument *doc;
	LspCallback callback;
	gpointer user_data;
} LspSymbolUserData;

typedef struct {
	gint ft_id;
	LspWorkspaceSymbolRequestCallback callback;
	gpointer user_data;
} LspWorkspaceSymbolUserData;


extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;


static void arr_free(GPtrArray *arr)
{
	if (arr)
		g_ptr_array_free(arr, TRUE);
}


void lsp_symbols_destroy(GeanyDocument *doc)
{
	plugin_set_document_data_full(geany_plugin, doc, CACHED_SYMBOLS_KEY,
			NULL, (GDestroyNotify)arr_free);
}


static void parse_symbols(GPtrArray *symbols, GVariant *symbol_variant, const gchar *scope,
	const gchar *scope_sep, gboolean workspace)
{
	GeanyDocument *doc = document_get_current();
	GVariant *member = NULL;
	GVariantIter iter;

	g_variant_iter_init(&iter, symbol_variant);

	while (g_variant_iter_loop(&iter, "v", &member))
	{
		LspSymbol *sym;
		const gchar *name = NULL;
		const gchar *detail = NULL;
		const gchar *uri = NULL;
		const gchar *container_name = NULL;
		GVariant *loc_variant = NULL;
		GVariant *children = NULL;
		gchar *uri_str = NULL;
		gint64 kind = 0;
		gint line_num = 0;
		gint line_pos = 0;
		gchar *file_name = NULL;
		const gchar *sym_scope = NULL;

		if (!JSONRPC_MESSAGE_PARSE(member, "name", JSONRPC_MESSAGE_GET_STRING(&name)))
			continue;
		if (!JSONRPC_MESSAGE_PARSE(member, "kind", JSONRPC_MESSAGE_GET_INT64(&kind)))
			continue;

		if (JSONRPC_MESSAGE_PARSE(member, "selectionRange", JSONRPC_MESSAGE_GET_VARIANT(&loc_variant)))
		{
			LspRange range = lsp_utils_parse_range(loc_variant);
			line_num = range.start.line;
			line_pos = range.start.character;
		}
		else if (JSONRPC_MESSAGE_PARSE(member, "range", JSONRPC_MESSAGE_GET_VARIANT(&loc_variant)))
		{
			LspRange range = lsp_utils_parse_range(loc_variant);
			line_num = range.start.line;
			line_pos = range.start.character;
		}
		else if (JSONRPC_MESSAGE_PARSE(member, "location", JSONRPC_MESSAGE_GET_VARIANT(&loc_variant)))
		{
			LspLocation *loc = lsp_utils_parse_location(loc_variant);
			if (loc)
			{
				line_num = loc->range.start.line;
				line_pos = loc->range.start.character;
				if (loc->uri)
					uri_str = g_strdup(loc->uri);
				lsp_utils_free_lsp_location(loc);
			}
		}
		else if (!workspace)
		{
			if (loc_variant)
				g_variant_unref(loc_variant);
			continue;
		}

		if (workspace)
		{
			JSONRPC_MESSAGE_PARSE(member, "containerName", JSONRPC_MESSAGE_GET_STRING(&container_name));

			if (!uri_str)
			{
				if (JSONRPC_MESSAGE_PARSE(member, "location", "{",
						"uri", JSONRPC_MESSAGE_GET_STRING(&uri),
					"}"))
				{
					if (uri)
						uri_str = g_strdup(uri);
				}

				if (!uri_str)
					continue;
			}
		}

		JSONRPC_MESSAGE_PARSE(member, "detail", JSONRPC_MESSAGE_GET_STRING(&detail));

		if (scope)
			sym_scope = scope;
		else if (container_name)
			sym_scope = container_name;

		if (uri_str)
			file_name = lsp_utils_get_real_path_from_uri_utf8(uri_str);
		else if (doc && doc->real_path)
			file_name = utils_get_utf8_from_locale(doc->real_path);

		sym = lsp_symbol_new(name, detail, sym_scope, file_name, doc->file_type->id, kind,
			line_num + 1, line_pos, lsp_symbol_kinds_get_symbol_icon(kind));

		g_ptr_array_add(symbols, sym);

		if (JSONRPC_MESSAGE_PARSE(member, "children", JSONRPC_MESSAGE_GET_VARIANT(&children)))
		{
			gchar *new_scope;

			if (scope)
				new_scope = g_strconcat(scope, scope_sep, lsp_symbol_get_name(sym), NULL);
			else
				new_scope = g_strdup(lsp_symbol_get_name(sym));
			parse_symbols(symbols, children, new_scope, scope_sep, FALSE);
			g_free(new_scope);
		}

		if (loc_variant)
			g_variant_unref(loc_variant);
		if (children)
			g_variant_unref(children);
		g_free(uri_str);
		g_free(file_name);
	}
}


static void symbols_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	LspSymbolUserData *data = user_data;

	if (!error && g_variant_is_of_type(return_value, G_VARIANT_TYPE_ARRAY))
	{
		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		if (data->doc == document_get_current())
		{
			GPtrArray *cached_symbols = g_ptr_array_new_full(0, (GDestroyNotify)lsp_symbol_unref);

			parse_symbols(cached_symbols, return_value, NULL, LSP_SCOPE_SEPARATOR, FALSE);

			plugin_set_document_data_full(geany_plugin, data->doc, CACHED_SYMBOLS_KEY,
				cached_symbols, (GDestroyNotify)arr_free);
		}
	}

	data->callback(data->user_data);

	g_free(user_data);
}


GPtrArray *lsp_symbols_doc_get_cached(GeanyDocument *doc)
{
	if (!doc)
		return NULL;

	return plugin_get_document_data(geany_plugin, doc, CACHED_SYMBOLS_KEY);
}


void lsp_symbols_doc_request(GeanyDocument *doc, LspCallback callback,
	gpointer user_data)
{
	LspServer *server = lsp_server_get(doc);
	LspSymbolUserData *data;
	GVariant *node;
	gchar *doc_uri;

	if (!doc || !doc->real_path || !server)
		return;

	data = g_new0(LspSymbolUserData, 1);
	data->user_data = user_data;
	data->doc = doc;
	data->callback = callback;

	doc_uri = lsp_utils_get_doc_uri(doc);

	/* Geany requests symbols before firing "document-activate" signal so we may
	 * need to request document opening here */
	lsp_sync_text_document_did_open(server, doc);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_rpc_call(server, "textDocument/documentSymbol", node,
		symbols_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}


static void workspace_symbols_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	LspWorkspaceSymbolUserData *data = user_data;
	GPtrArray *ret = g_ptr_array_new_full(0, (GDestroyNotify)lsp_symbol_unref);

	if (!error && g_variant_is_of_type(return_value, G_VARIANT_TYPE_ARRAY))
	{
		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		//scope separator doesn't matter here
		parse_symbols(ret, return_value, NULL, "", TRUE);
	}

	data->callback(ret, data->user_data);

	g_ptr_array_free(ret, TRUE);
	g_free(user_data);
}


void lsp_symbols_workspace_request(GeanyDocument *doc, const gchar *query,
	LspWorkspaceSymbolRequestCallback callback, gpointer user_data)
{
	LspServer *server = lsp_server_get(doc);
	LspWorkspaceSymbolUserData *data;
	GVariant *node;

	if (!server)
		return;

	data = g_new0(LspWorkspaceSymbolUserData, 1);
	data->user_data = user_data;
	data->callback = callback;
	data->ft_id = doc->file_type->id;

	node = JSONRPC_MESSAGE_NEW (
		"query", JSONRPC_MESSAGE_PUT_STRING(query)
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_rpc_call(server, "workspace/symbol", node,
		workspace_symbols_cb, data);

	g_variant_unref(node);
}
