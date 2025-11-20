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

#include "lsp-semtokens.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"
#include "lsp-sync.h"

#include <jsonrpc-glib.h>

#define CACHE_KEY "lsp_semtokens_key"

typedef struct {
	guint start;
	guint delete_count;
	GArray *data;
} SemanticTokensEdit;


extern GeanyPlugin *geany_plugin;

extern GeanyData *geany_data;

typedef struct {
	GArray *tokens;
	gchar *tokens_str;
	gchar *result_id;
} CachedData;


static gint style_index;

static guint keyword_hash = 0;


static void cached_data_free(CachedData *data)
{
	g_array_free(data->tokens, TRUE);
	g_free(data->tokens_str);
	g_free(data->result_id);
	g_free(data);
}


void lsp_semtokens_init(gint ft_id)
{
	guint i;
	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];
		if (doc->file_type->id == ft_id)
			plugin_set_document_data(geany_plugin, doc, CACHE_KEY, NULL);
	}
}


void lsp_semtokens_style_init(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	ScintillaObject *sci;

	if (!srv)
		return;

	sci = doc->editor->sci;

	style_index = 0;
	if (!EMPTY(srv->config.semantic_tokens_type_style))
		style_index = lsp_utils_set_indicator_style(sci, srv->config.semantic_tokens_type_style);
}


void lsp_semtokens_destroy(GeanyDocument *doc)
{
	plugin_set_document_data(geany_plugin, doc, CACHE_KEY, NULL);
}


static SemanticTokensEdit *sem_tokens_edit_new(void)
{
	SemanticTokensEdit *edit = g_new0(SemanticTokensEdit, 1);
	edit->data = g_array_sized_new(FALSE, FALSE, sizeof(guint), 20);
	return edit;
}


static void sem_tokens_edit_free(SemanticTokensEdit *edit)
{
	g_array_free(edit->data, TRUE);
	g_free(edit);
}


static void sem_tokens_edit_apply(CachedData *data, SemanticTokensEdit *edit)
{
	g_return_if_fail(edit->start + edit->delete_count <= data->tokens->len);

	g_array_remove_range(data->tokens, edit->start, edit->delete_count);
	g_array_insert_vals(data->tokens, edit->start, edit->data->data, edit->data->len);
}


static const gchar *get_cached(GeanyDocument *doc)
{
	CachedData *data;

	if (style_index > 0)
		return "";

	data = plugin_get_document_data(geany_plugin, doc, CACHE_KEY);
	if (!data || !data->tokens_str)
		return "";

	return data->tokens_str;
}


static void highlight_keywords(LspServer *srv, GeanyDocument *doc)
{
	const gchar *keywords = get_cached(doc);
	guint new_hash;

	keywords = keywords != NULL ? keywords : "";
	new_hash = g_str_hash(keywords);

	if (keyword_hash != new_hash)
	{
		SSM(doc->editor->sci, SCI_SETKEYWORDS, srv->config.semantic_tokens_lexer_kw_index, (sptr_t) keywords);
		SSM(doc->editor->sci, SCI_COLOURISE, (uptr_t) 0, -1);
		keyword_hash = new_hash;
	}
}


static gchar *process_tokens(GArray *tokens, GeanyDocument *doc, guint64 token_mask)
{
	GHashTable *type_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ScintillaObject *sci = doc->editor->sci;
	guint delta_line = 0;
	guint delta_char = 0;
	guint len = 0;
	guint token_type = 0;
	LspPosition last_pos = {0, 0};
	gboolean first = TRUE;
	GList *keys, *item;
	GString *type_str;
	gint i;

	if (style_index > 0)
	{
		sci_indicator_set(doc->editor->sci, style_index);
		sci_indicator_clear(doc->editor->sci, 0, sci_get_length(doc->editor->sci));
	}

	for (i = 0; i < tokens->len; i++)
	{
		guint v = g_array_index(tokens, guint, i);

		switch (i % 5)
		{
			case 0:
				delta_line = v;
				break;
			case 1:
				delta_char = v;
				break;
			case 2:
				len = v;
				break;
			case 3:
				token_type = 1 << v;
				break;
		}

		if (i % 5 == 4)
		{
			last_pos.line += delta_line;
			if (delta_line == 0)
				last_pos.character += delta_char;
			else
				last_pos.character = delta_char;

			if (token_type & token_mask)
			{
				LspPosition end_pos = last_pos;
				gint sci_pos_start, sci_pos_end;
				gchar *str;

				end_pos.character += len;
				sci_pos_start = lsp_utils_lsp_pos_to_scintilla(sci, last_pos);
				sci_pos_end = lsp_utils_lsp_pos_to_scintilla(sci, end_pos);

				if (style_index > 0)
					editor_indicator_set_on_range(doc->editor, style_index, sci_pos_start, sci_pos_end);

				str = sci_get_contents_range(sci, sci_pos_start, sci_pos_end);
				if (str)
					g_hash_table_insert(type_table, str, NULL);
			}
		}
	}

	keys = g_hash_table_get_keys(type_table);
	type_str = g_string_new("");

	foreach_list(item, keys)
	{
		if (!first)
			g_string_append_c(type_str, ' ');
		g_string_append(type_str, item->data);
		first = FALSE;
	}

	g_list_free(keys);
	g_hash_table_destroy(type_table);

	return g_string_free(type_str, FALSE);
}


static void process_full_result(GeanyDocument *doc, GVariant *result, guint64 token_mask)
{
	GVariantIter *iter = NULL;
	const gchar *result_id = NULL;

	JSONRPC_MESSAGE_PARSE(result,
		"resultId", JSONRPC_MESSAGE_GET_STRING(&result_id)
	);
	JSONRPC_MESSAGE_PARSE(result,
		"data", JSONRPC_MESSAGE_GET_ITER(&iter)
	);

	if (iter)
	{
		GVariant *val = NULL;
		CachedData *data = plugin_get_document_data(geany_plugin, doc, CACHE_KEY);

		if (data == NULL)
		{
			data = g_new0(CachedData, 1);
			data->tokens = g_array_sized_new(FALSE, FALSE, sizeof(guint), 1000);
			plugin_set_document_data_full(geany_plugin, doc, CACHE_KEY, data, (GDestroyNotify)cached_data_free);
		}

		g_free(data->result_id);
		data->result_id = g_strdup(result_id);
		data->tokens->len = 0;

		while (g_variant_iter_loop(iter, "v", &val))
		{
			guint v = g_variant_get_int64(val);
			g_array_append_val(data->tokens, v);
		}

		g_free(data->tokens_str);
		data->tokens_str = process_tokens(data->tokens, doc, token_mask);

		g_variant_iter_free(iter);
	}
}


static gint sort_edits(gconstpointer a, gconstpointer b)
{
	const SemanticTokensEdit *e1 = *((SemanticTokensEdit **) a);
	const SemanticTokensEdit *e2 = *((SemanticTokensEdit **) b);

	return e2->start - e1->start;
}


static gboolean process_delta_result(GeanyDocument *doc, GVariant *result, guint64 token_mask)
{
	GVariantIter *iter = NULL;
	const gchar *result_id = NULL;
	CachedData *data = NULL;
	gboolean ret = FALSE;

	JSONRPC_MESSAGE_PARSE(result,
		"resultId", JSONRPC_MESSAGE_GET_STRING(&result_id),
		"edits", JSONRPC_MESSAGE_GET_ITER(&iter)
	);

	data = plugin_get_document_data(geany_plugin, doc, CACHE_KEY);

	if (data && (!iter || !result_id))
	{
		// something got wrong - let's delete our cached result so the next request
		// is full instead of delta which may be out of sync
		plugin_set_document_data(geany_plugin, doc, CACHE_KEY, NULL);
	}
	else if (data && iter && result_id)
	{
		GPtrArray *edits = g_ptr_array_new_full(4, (GDestroyNotify)sem_tokens_edit_free);
		SemanticTokensEdit *edit;
		GVariant *val = NULL;
		guint i;
 
		while (g_variant_iter_loop(iter, "v", &val))
		{
			GVariantIter *iter2 = NULL;
			GVariant *val2 = NULL;
			gint64 delete_count = 0;
			gint64 start = 0;
			gboolean success;

			success = JSONRPC_MESSAGE_PARSE(val,
				"start", JSONRPC_MESSAGE_GET_INT64(&start),
				"deleteCount", JSONRPC_MESSAGE_GET_INT64(&delete_count),
				"data", JSONRPC_MESSAGE_GET_ITER(&iter2)
			);

			if (success)
			{
				edit = sem_tokens_edit_new();
				edit->start = start;
				edit->delete_count = delete_count;

				while (g_variant_iter_loop(iter2, "v", &val2))
				{
					guint v = g_variant_get_int64(val2);
					g_array_append_val(edit->data, v);
				}

				g_ptr_array_add(edits, edit);
			}

			if (iter2)
				g_variant_iter_free(iter2);
		}

		g_ptr_array_sort(edits, sort_edits);

		foreach_ptr_array(edit, i, edits)
			sem_tokens_edit_apply(data, edit);

		g_free(data->tokens_str);
		data->tokens_str = process_tokens(data->tokens, doc, token_mask);
		g_free(data->result_id);
		data->result_id = g_strdup(result_id);

		g_ptr_array_free(edits, TRUE);

		ret = TRUE;
	}

	if (iter)
		g_variant_iter_free(iter);

	return ret;
}


static void semtokens_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		GeanyDocument *doc = user_data;
		LspServer *srv;

		srv = DOC_VALID(doc) ? lsp_server_get(doc) : NULL;

		if (srv)
		{
			gboolean success = TRUE;
			GVariantIter *iter = NULL;

			//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

			JSONRPC_MESSAGE_PARSE(return_value,
				"data", JSONRPC_MESSAGE_GET_ITER(&iter)
			);

			if (iter)
			{
				process_full_result(doc, return_value, srv->semantic_token_mask);
				g_variant_iter_free(iter);
			}
			else
				success = process_delta_result(doc, return_value, srv->semantic_token_mask);

			if (success)
				highlight_keywords(srv, doc);
		}
	}
}


void lsp_semtokens_send_request(GeanyDocument *doc)
{
	LspServer *server = lsp_server_get(doc);
	gchar *doc_uri;
	GVariant *node;
	CachedData *cached_data;
	gboolean delta;

	if (!doc || !server)
		return;

	doc_uri = lsp_utils_get_doc_uri(doc);

	/* Geany requests symbols before firing "document-activate" signal so we may
	 * need to request document opening here */
	lsp_sync_text_document_did_open(server, doc);

	cached_data = plugin_get_document_data(geany_plugin, doc, CACHE_KEY);
	delta = cached_data != NULL && cached_data->result_id &&
		server->config.semantic_tokens_supports_delta &&
		!server->config.semantic_tokens_force_full;

	if (delta)
	{
		node = JSONRPC_MESSAGE_NEW(
			"previousResultId", JSONRPC_MESSAGE_PUT_STRING(cached_data->result_id),
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}"
		);
		lsp_rpc_call(server, "textDocument/semanticTokens/full/delta", node,
			semtokens_cb, doc);
	}
	else if (server->config.semantic_tokens_range_only)
	{
		guint last_pos = SSM(doc->editor->sci, SCI_GETLENGTH, 0, 0);
		LspPosition pos = lsp_utils_scintilla_pos_to_lsp(doc->editor->sci, last_pos);

		node = JSONRPC_MESSAGE_NEW(
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}",
			"range", "{",
				"start", "{",
					"line", JSONRPC_MESSAGE_PUT_INT32(0),
					"character", JSONRPC_MESSAGE_PUT_INT32(0),
				"}",
				"end", "{",
					"line", JSONRPC_MESSAGE_PUT_INT32(pos.line),
					"character", JSONRPC_MESSAGE_PUT_INT32(pos.character),
				"}",
			"}"
		);
		lsp_rpc_call(server, "textDocument/semanticTokens/range", node,
			semtokens_cb, doc);
	}
	else
	{
		node = JSONRPC_MESSAGE_NEW(
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}"
		);
		lsp_rpc_call(server, "textDocument/semanticTokens/full", node,
			semtokens_cb, doc);
	}

	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_semtokens_clear(GeanyDocument *doc)
{
	if (!doc)
		return;

	plugin_set_document_data(geany_plugin, doc, CACHE_KEY, NULL);
	keyword_hash = 0;

	if (style_index > 0)
	{
		sci_indicator_set(doc->editor->sci, style_index);
		sci_indicator_clear(doc->editor->sci, 0, sci_get_length(doc->editor->sci));
	}
}
