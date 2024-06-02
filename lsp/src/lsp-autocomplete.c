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

#include "lsp-autocomplete.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"
#include "lsp-server.h"
#include "lsp-symbol-kinds.h"

#include <jsonrpc-glib.h>
#include <ctype.h>
#include <glib.h>


typedef struct
{
	gchar *label;
	LspCompletionKind kind;
	gchar *sort_text;
	gchar *insert_text;
	gchar *detail;
	LspTextEdit *text_edit;
	GPtrArray * additional_edits;
} LspAutocompleteSymbol;


typedef struct
{
	GeanyDocument *doc;
	gint request_id;
} LspAutocompleteAsyncData;


typedef struct
{
	gint pass;
	gchar *prefix;
} SortData;


static GPtrArray *displayed_autocomplete_symbols = NULL;
static gint sent_request_id = 0;
static gint received_request_id = 0;
static gint discard_up_to_request_id = 0;


void lsp_autocomplete_discard_pending_requests()
{
	discard_up_to_request_id = sent_request_id;
}


void lsp_autocomplete_set_displayed_symbols(GPtrArray *symbols)
{
	if (displayed_autocomplete_symbols)
		g_ptr_array_free(displayed_autocomplete_symbols, TRUE);
	displayed_autocomplete_symbols = symbols;
}


static void free_autocomplete_symbol(gpointer data)
{
	LspAutocompleteSymbol *sym = data;
	g_free(sym->label);
	g_free(sym->sort_text);
	g_free(sym->insert_text);
	g_free(sym->detail);
	lsp_utils_free_lsp_text_edit(sym->text_edit);
	if (sym->additional_edits)
		g_ptr_array_free(sym->additional_edits, TRUE);
	g_free(sym);
}


static const gchar *get_symbol_label(LspServer *server, LspAutocompleteSymbol *sym)
{
	if (server->config.autocomplete_use_label && sym->label)
		return sym->label;

	if (sym->text_edit && sym->text_edit->new_text)
		return sym->text_edit->new_text;
	if (sym->insert_text)
		return sym->insert_text;
	if (sym->label)
		return sym->label;

	return "";
}


static guint get_ident_prefixlen(GeanyDocument *doc, gint pos)
{
	//TODO: use configured wordchars (also change in Geany)
	const gchar *wordchars = GEANY_WORDCHARS;
	GeanyFiletypeID ft = doc->file_type->id;
	ScintillaObject *sci = doc->editor->sci;
	gint num = 0;

	if (ft == GEANY_FILETYPES_LATEX)
		wordchars = GEANY_WORDCHARS"\\"; /* add \ to word chars if we are in a LaTeX file */
	else if (ft == GEANY_FILETYPES_CSS)
		wordchars = GEANY_WORDCHARS"-"; /* add - because they are part of property names */

	while (pos > 0)
	{
		gint new_pos = SSM(sci, SCI_POSITIONBEFORE, pos, 0);
		if (pos - new_pos == 1)
		{
			gchar c = sci_get_char_at(sci, new_pos);
			if (!strchr(wordchars, c))
				break;
		}
		num++;
		pos = new_pos;
	}

	return num;
}


#if 0
static gboolean add_newline_idle(gpointer user_data)
{
	GeanyDocument *doc = user_data;

	if (doc != document_get_current())
		return FALSE;

	SSM(doc->editor->sci, SCI_NEWLINE, 0, 0);
	return FALSE;
}
#endif


void lsp_autocomplete_item_selected(LspServer *server, GeanyDocument *doc, guint index)
{
	ScintillaObject *sci = doc->editor->sci;
	LspAutocompleteSymbol *sym;
	//gint full_len = sci_get_length(sci);

	if (!displayed_autocomplete_symbols || index >= displayed_autocomplete_symbols->len)
		return;

	sym = displayed_autocomplete_symbols->pdata[index];
	if (sym->text_edit)
	{
		if (server->config.autocomplete_apply_additional_edits && sym->additional_edits)
			lsp_utils_apply_text_edits(sci, sym->text_edit, sym->additional_edits);
		else
			lsp_utils_apply_text_edit(sci, sym->text_edit, TRUE);
	}
	else
	{
		gint pos = sci_get_current_position(sci);
		guint rootlen = get_ident_prefixlen(doc, pos);
		gchar *insert_text = sym->insert_text ? sym->insert_text : sym->label;
		if (insert_text && strlen(insert_text) >= rootlen)
		{
			SSM(sci, SCI_DELETERANGE, pos - rootlen, rootlen);
			pos = sci_get_current_position(sci);
			sci_insert_text(sci, pos, insert_text);
			sci_set_current_position(sci, pos + strlen(insert_text), TRUE);
		}
	}

#if 0
	if (full_len == sci_get_length(sci))
	{
		// autocompletion didn't change length of the document, possibly the whole
		// word was already typed - insert newline but do this on idle otherwise
		// this causes some infinite recursion in Scintilla because this is invoked
		// from its event notification function
		g_idle_add(add_newline_idle, doc);
	}
#endif
}


static void show_tags_list(LspServer *server, GeanyDocument *doc, GPtrArray *symbols)
{
	guint i;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	GString *words = g_string_sized_new(2000);
	const gchar *label;

	for (i = 0; i < symbols->len; i++)
	{
		LspAutocompleteSymbol *symbol = symbols->pdata[i];
		guint icon_id = lsp_symbol_kinds_get_completion_icon(symbol->kind);
		gchar buf[10];

		if (i > server->config.autocomplete_window_max_entries)
			break;

		if (i > 0)
			g_string_append_c(words, '\n');

		label = get_symbol_label(server, symbol);
		g_string_append(words, label);

		sprintf(buf, "?%u", icon_id + 1);
		g_string_append(words, buf);
	}

	lsp_autocomplete_set_displayed_symbols(symbols);
	SSM(sci, SCI_AUTOCSETORDER, SC_ORDER_CUSTOM, 0);
#ifdef SC_AUTOCOMPLETE_SELECT_FIRST_ITEM
	SSM(sci, SCI_AUTOCSETOPTIONS, SC_AUTOCOMPLETE_SELECT_FIRST_ITEM, 0);
#endif
	SSM(sci, SCI_AUTOCSETMULTI, SC_MULTIAUTOC_EACH, 0);
	SSM(sci, SCI_AUTOCSETAUTOHIDE, FALSE, 0);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, server->config.autocomplete_window_max_displayed, 0);
	SSM(sci, SCI_AUTOCSETMAXWIDTH, server->config.autocomplete_window_max_width, 0);
	SSM(sci, SCI_AUTOCSHOW, get_ident_prefixlen(doc, pos), (sptr_t) words->str);

	//make sure Scintilla selects the first item - see https://sourceforge.net/p/scintilla/bugs/2403/
	label = get_symbol_label(server, symbols->pdata[0]);
	SSM(sci, SCI_AUTOCSELECT, 0, (sptr_t)label);

	g_string_free(words, TRUE);
}


static gint sort_autocomplete_symbols(gconstpointer a, gconstpointer b, gpointer user_data)
{
	LspAutocompleteSymbol *sym1 = *((LspAutocompleteSymbol **)a);
	LspAutocompleteSymbol *sym2 = *((LspAutocompleteSymbol **)b);
	SortData *sort_data = user_data;
	gchar *label1 = NULL;
	gchar *label2 = NULL;

	if (sym1->text_edit && sym1->text_edit->new_text)
		label1 = sym1->text_edit->new_text;
	else if (sym1->insert_text)
		label1 = sym1->insert_text;
	else if (sym1->label)
		label1 = sym1->label;

	if (sym2->text_edit && sym2->text_edit->new_text)
		label2 = sym2->text_edit->new_text;
	else if (sym2->insert_text)
		label2 = sym2->insert_text;
	else if (sym2->label)
		label2 = sym2->label;

	/*
	if (sort_data->pass == 2)
	{
		if (sym1->kind == LspCompletionKindKeyword && sym2->kind != LspCompletionKindKeyword)
			return -1;

		if (sym1->kind != LspCompletionKindKeyword && sym2->kind == LspCompletionKindKeyword)
			return 1;

		if (sym1->kind == LspCompletionKindSnippet && sym2->kind != LspCompletionKindSnippet)
			return -1;

		if (sym1->kind != LspCompletionKindSnippet && sym2->kind == LspCompletionKindSnippet)
			return 1;
	}
	*/

	if (sort_data->pass == 2 && label1 && label2 && sort_data->prefix)
	{
		if (g_strcmp0(label1, sort_data->prefix) == 0 && g_strcmp0(label2, sort_data->prefix) != 0)
			return -1;

		if (g_strcmp0(label1, sort_data->prefix) != 0 && g_strcmp0(label2, sort_data->prefix) == 0)
			return 1;

		if (g_str_has_prefix(label1, sort_data->prefix) && !g_str_has_prefix(label2, sort_data->prefix))
			return -1;

		if (!g_str_has_prefix(label1, sort_data->prefix) && g_str_has_prefix(label2, sort_data->prefix))
			return 1;
	}

	if (sym1->sort_text && sym2->sort_text)
		return g_strcmp0(sym1->sort_text, sym2->sort_text);

	if (label1 && label2)
		return g_strcmp0(label1, label2);

	return 0;
}


static void process_response(LspServer *server, GVariant *response, GeanyDocument *doc)
{
	//gboolean is_incomplete = FALSE;
	GVariantIter *iter = NULL;
	GVariant *member = NULL;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	gint prefixlen = get_ident_prefixlen(doc, pos);
	SortData sort_data = { 1, NULL };
	GPtrArray *symbols, *symbols_filtered;
	GHashTable *entry_set;
	gint i;

	JSONRPC_MESSAGE_PARSE(response, 
		//"isIncomplete", JSONRPC_MESSAGE_GET_BOOLEAN(&is_incomplete),
		"items", JSONRPC_MESSAGE_GET_ITER(&iter));

	if (!iter)
		return;

	symbols = g_ptr_array_new_full(0, NULL);  // not freeing symbols here

	while (g_variant_iter_loop(iter, "v", &member))
	{
		LspAutocompleteSymbol *sym;
		GVariant *text_edit = NULL;
		GVariantIter *additional_edits = NULL;
		const gchar *label = NULL;
		const gchar *insert_text = NULL;
		const gchar *sort_text = NULL;
		const gchar *detail = NULL;
		gint64 kind = 0;

		JSONRPC_MESSAGE_PARSE(member, "kind", JSONRPC_MESSAGE_GET_INT64(&kind));

		if (kind == LspCompletionKindSnippet)
			continue;  // not supported right now

		JSONRPC_MESSAGE_PARSE(member, "label", JSONRPC_MESSAGE_GET_STRING(&label));
		JSONRPC_MESSAGE_PARSE(member, "insertText", JSONRPC_MESSAGE_GET_STRING(&insert_text));
		JSONRPC_MESSAGE_PARSE(member, "sortText", JSONRPC_MESSAGE_GET_STRING(&sort_text));
		JSONRPC_MESSAGE_PARSE(member, "detail", JSONRPC_MESSAGE_GET_STRING(&detail));
		JSONRPC_MESSAGE_PARSE(member, "textEdit", JSONRPC_MESSAGE_GET_VARIANT(&text_edit));
		JSONRPC_MESSAGE_PARSE(member, "additionalTextEdits", JSONRPC_MESSAGE_GET_ITER(&additional_edits));

		sym = g_new0(LspAutocompleteSymbol, 1);
		sym->label = g_strdup(label);
		sym->insert_text = g_strdup(insert_text);
		sym->sort_text = g_strdup(sort_text);
		sym->detail = g_strdup(detail);
		sym->kind = kind;
		sym->text_edit = lsp_utils_parse_text_edit(text_edit);
		sym->additional_edits = lsp_utils_parse_text_edits(additional_edits);

		g_ptr_array_add(symbols, sym);

		if (text_edit)
			g_variant_unref(text_edit);

		if (additional_edits)
			g_variant_iter_free(additional_edits);
	}

	/* sort based on sorting provided by LSP server */
	g_ptr_array_sort_with_data(symbols, sort_autocomplete_symbols, &sort_data);

	symbols_filtered = g_ptr_array_new_full(symbols->len, free_autocomplete_symbol);
	entry_set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	/* remove duplicates */
	for (i = 0; i < symbols->len; i++)
	{
		LspAutocompleteSymbol *sym = symbols->pdata[i];
		const gchar *display_label = get_symbol_label(server, sym);

		if (g_hash_table_contains(entry_set, display_label))
			free_autocomplete_symbol(sym);
		else
		{
			g_ptr_array_add(symbols_filtered, sym);
			g_hash_table_insert(entry_set, g_strdup(display_label), NULL);
		}
	}

	g_ptr_array_free(symbols, TRUE);
	symbols = symbols_filtered;

	if (prefixlen > 0)
		sort_data.prefix = sci_get_contents_range(sci, pos - prefixlen, pos);
	sort_data.pass = 2;
	/* sort with symbols matching the typed prefix first */
	g_ptr_array_sort_with_data(symbols, sort_autocomplete_symbols, &sort_data);

	if (symbols->len > 0)
		show_tags_list(server, doc, symbols);
	else
	{
		g_ptr_array_free(symbols, TRUE);
		SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
	}

	g_variant_iter_free(iter);
	g_hash_table_destroy(entry_set);
	g_free(sort_data.prefix);
}


static void autocomplete_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		GeanyDocument *current_doc = document_get_current();
		LspAutocompleteAsyncData *data = user_data;
		GeanyDocument *doc = data->doc;

		if (current_doc == doc && data->request_id > received_request_id &&
			data->request_id > discard_up_to_request_id)
		{
			LspServer *srv = lsp_server_get(doc);
			received_request_id = data->request_id;
			process_response(srv, return_value, doc);
			//printf("%s\n", lsp_utils_json_pretty_print(return_value));
		}
	}

	g_free(user_data);
}


static gboolean ends_with_sequence(ScintillaObject *sci, gchar** seqs)
{
	gint pos = sci_get_current_position(sci);
	guint max = 0;
	gchar **str;
	gchar *prev_str;
	gboolean ret = FALSE;

	if (!seqs)
		return FALSE;

	foreach_strv(str, seqs)
		max = MAX(max, strlen(*str));

	prev_str = sci_get_contents_range(sci, pos - max > 0 ? pos - max : 0, pos);

	foreach_strv(str, seqs)
	{
		if (g_str_has_suffix(prev_str, *str))
		{
			ret = TRUE;
			break;
		}
	}

	g_free(prev_str);
	return ret;
}


void lsp_autocomplete_completion(LspServer *server, GeanyDocument *doc, gboolean force)
{
	//gboolean static first_time = TRUE;
	GVariant *node;
	gchar *doc_uri;
	LspAutocompleteAsyncData *data;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gint lexer = sci_get_lexer(sci);
	gint style = sci_get_style_at(sci, pos);
	gboolean is_trigger_char = FALSE;
	gchar c = pos > 0 ? sci_get_char_at(sci, SSM(sci, SCI_POSITIONBEFORE, pos, 0)) : '\0';
	gchar c_str[2] = {c, '\0'};

	// highlighting_is_code_style(lexer, style) also checks for preprocessor
	// style which we might not want here as LSP servers might support it
	if (highlighting_is_comment_style(lexer, style) ||
		highlighting_is_string_style(lexer, style))
	{
		SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
		return;
	}

	if (get_ident_prefixlen(doc, pos) == 0)
	{
		if (server->config.autocomplete_trigger_sequences &&
			!ends_with_sequence(sci, server->config.autocomplete_trigger_sequences))
		{
			SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
			return;
		}

		if (!server->autocomplete_trigger_chars || !strchr(server->autocomplete_trigger_chars, c))
		{
			SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
			return;
		}
		else
			is_trigger_char = !force;
	}
	else
	{
		gint next_pos = SSM(sci, SCI_POSITIONAFTER, pos, 0);
		/* if we are inside an identifier also after the next char */
		if (get_ident_prefixlen(doc, pos) + (next_pos - pos) == get_ident_prefixlen(doc, next_pos))
		{
			SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
			return;  /* avoid autocompletion in the middle of a word */
		}
	}

	doc_uri = lsp_utils_get_doc_uri(doc);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"position", "{",
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}",
		"context", "{",
			"triggerKind", JSONRPC_MESSAGE_PUT_INT32(is_trigger_char ? 2 : 1),
			"triggerCharacter", JSONRPC_MESSAGE_PUT_STRING(c_str),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));
	data = g_new0(LspAutocompleteAsyncData, 1);
	data->doc = doc;
	data->request_id = ++sent_request_id;

	lsp_rpc_call(server, "textDocument/completion", node,
		autocomplete_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}
