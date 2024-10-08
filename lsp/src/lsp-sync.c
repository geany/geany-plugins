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

#include "lsp-sync.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"
#include "lsp-diagnostics.h"
#include "lsp-highlight.h"
#include "lsp-semtokens.h"
#include "lsp-workspace-folders.h"

#include <jsonrpc-glib.h>

#define VERSION_NUM_KEY "lsp_sync_version_num"

extern GeanyPlugin *geany_plugin;

static GHashTable *open_docs = NULL;


void lsp_sync_init()
{
	if (!open_docs)
		open_docs = g_hash_table_new(NULL, NULL);
	g_hash_table_remove_all(open_docs);
}


static guint get_next_doc_version_num(GeanyDocument *doc)
{
	guint num = GPOINTER_TO_UINT(plugin_get_document_data(geany_plugin, doc, VERSION_NUM_KEY));

	num++;
	plugin_set_document_data(geany_plugin, doc, VERSION_NUM_KEY, GUINT_TO_POINTER(num));
	return num;
}


gboolean lsp_sync_is_document_open(GeanyDocument *doc)
{
	return g_hash_table_lookup(open_docs, doc) != NULL;
}


void lsp_sync_text_document_did_open(LspServer *server, GeanyDocument *doc)
{
	GVariant *node;
	gchar *doc_uri;
	gchar *lang_id = NULL;
	gchar *doc_text;
	guint doc_version;

	if (lsp_sync_is_document_open(doc))
		return;

	lsp_workspace_folders_doc_open(doc);

	g_hash_table_add(open_docs, doc);

	lsp_server_get_ft(doc, &lang_id);
	doc_uri = lsp_utils_get_doc_uri(doc);
	doc_text = sci_get_contents(doc->editor->sci, -1);
	doc_version = get_next_doc_version_num(doc);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"languageId", JSONRPC_MESSAGE_PUT_STRING(lang_id),
			"version", JSONRPC_MESSAGE_PUT_INT32(doc_version),
			"text", JSONRPC_MESSAGE_PUT_STRING(doc_text),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_rpc_notify(server, "textDocument/didOpen", node, NULL, NULL);

	g_free(doc_uri);
	g_free(lang_id);
	g_free(doc_text);

	g_variant_unref(node);
}


void lsp_sync_text_document_did_close(LspServer *server, GeanyDocument *doc)
{
	GVariant *node;
	gchar *doc_uri;

	if (!lsp_sync_is_document_open(doc))
		return;

	doc_uri = lsp_utils_get_doc_uri(doc);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	g_hash_table_remove(open_docs, doc);

	lsp_rpc_notify(server, "textDocument/didClose", node, NULL, NULL);

	lsp_workspace_folders_doc_closed(doc);

	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_sync_text_document_did_save(LspServer *server, GeanyDocument *doc)
{
	gchar *doc_uri;
	GVariant *node;

	if (!server->send_did_save)
		return;

	doc_uri = lsp_utils_get_doc_uri(doc);

	if (server->include_text_on_save)
	{
		gchar *doc_text = sci_get_contents(doc->editor->sci, -1);
		node = JSONRPC_MESSAGE_NEW (
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}",
			"text", JSONRPC_MESSAGE_PUT_STRING(doc_text)
		);
		g_free(doc_text);
	}
	else
	{
		node = JSONRPC_MESSAGE_NEW (
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}"
		);
	}

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_rpc_notify(server, "textDocument/didSave", node, NULL, NULL);

	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_sync_text_document_did_change(LspServer *server, GeanyDocument *doc,
	LspPosition pos_start, LspPosition pos_end, gchar *text)
{
	GVariant *node;
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);
	guint doc_version = get_next_doc_version_num(doc);

	if (server->use_incremental_sync)
	{
		gint range = lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, pos_end) - 
			lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, pos_start);

		node = JSONRPC_MESSAGE_NEW (
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
				"version", JSONRPC_MESSAGE_PUT_INT32(doc_version),
			"}",
			"contentChanges", "[", "{",
				"range", "{",
					"start", "{",
						"line", JSONRPC_MESSAGE_PUT_INT32(pos_start.line),
						"character", JSONRPC_MESSAGE_PUT_INT32(pos_start.character),
					"}",
					"end", "{",
						"line", JSONRPC_MESSAGE_PUT_INT32(pos_end.line),
						"character", JSONRPC_MESSAGE_PUT_INT32(pos_end.character),
					"}",
				"}",
				// not required but the lemminx server crashes without it
				"rangeLength", JSONRPC_MESSAGE_PUT_INT32(range),
				"text", JSONRPC_MESSAGE_PUT_STRING(text),
			"}", "]"
		);
	}
	else
	{
		node = JSONRPC_MESSAGE_NEW (
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
				"version", JSONRPC_MESSAGE_PUT_INT32(doc_version),
			"}",
			"contentChanges", "[", "{",
				"text", JSONRPC_MESSAGE_PUT_STRING(text),
			"}", "]"
		);
	}

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_rpc_notify(server, "textDocument/didChange", node, NULL, NULL);

	g_free(doc_uri);

	g_variant_unref(node);
}
