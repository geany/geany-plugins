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
	gchar *filter_text;
	gchar *insert_text;
	gchar *detail;
	gchar *documentation;
	LspTextEdit *text_edit;
	GPtrArray * additional_edits;
	gboolean is_snippet;
	GVariant *raw_symbol;
	gboolean resolved;
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
	gboolean use_label;
	const gchar *word_chars;
} SortData;


typedef struct
{
	LspAutocompleteSymbol *symbol;
	GeanyDocument *doc;
} ResolveData;


static GPtrArray *displayed_autocomplete_symbols = NULL;
static gint sent_request_id = 0;
static gint received_request_id = 0;
static gint discard_up_to_request_id = 0;
static gboolean statusbar_modified = FALSE;


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
	g_free(sym->filter_text);
	g_free(sym->insert_text);
	g_free(sym->detail);
	g_free(sym->documentation);
	lsp_utils_free_lsp_text_edit(sym->text_edit);
	if (sym->additional_edits)
		g_ptr_array_free(sym->additional_edits, TRUE);
	g_variant_unref(sym->raw_symbol);
	g_free(sym);
}


static const gchar *get_label(LspAutocompleteSymbol *sym, gboolean use_label)
{
	if (use_label && sym->label)
		return sym->label;

	if (sym->text_edit && sym->text_edit->new_text)
		return sym->text_edit->new_text;
	if (sym->insert_text)
		return sym->insert_text;
	if (sym->label)
		return sym->label;

	return "";
}


static gchar *get_symbol_label(LspServer *server, LspAutocompleteSymbol *sym)
{
	gchar *label = g_strdup(get_label(sym, server->config.autocomplete_use_label));
	gchar *pos;

	// remove stuff after newlines (we don't want them in the popup plus \n
	// is used as the autocompletion list separator)
	pos = strchr(label, '\n');
	if (pos)
		*pos = '\0';
	pos = strchr(label, '\r');
	if (pos)
		*pos = '\0';

	// ? used by Scintilla for icon specification
	pos = strchr(label, '?');
	if (pos)
		*pos = ' ';
	pos = strchr(label, '\t');
	if (pos)
		*pos = ' ';

	return label;
}


static guint get_ident_prefixlen(const gchar *word_chars, GeanyDocument *doc, gint pos)
{
	ScintillaObject *sci = doc->editor->sci;
	gint num = 0;

	while (pos > 0)
	{
		gint new_pos = SSM(sci, SCI_POSITIONBEFORE, pos, 0);
		gchar c = sci_get_char_at(sci, new_pos);

		if (pos - new_pos == 1)
		{
			if (!strchr(word_chars, c))
				break;
		}
		else if (pos - new_pos == 2)
		{
			gchar c2 = sci_get_char_at(sci, new_pos + 1);

			// multibyte sequence - we consider everything except \r\n as
			// visible characters (SCI_POSITIONBEFORE skips \r\n in one step
			// which then breaks autocompletion with CRLF line ends)
			if ((c == '\r' && c2 == '\n') || (c == '\n' && c2 == '\r'))
				break;
		}
		num++;
		pos = new_pos;
	}

	return num;
}


void lsp_autocomplete_item_selected(LspServer *server, GeanyDocument *doc, guint index)
{
	ScintillaObject *sci = doc->editor->sci;
	gint sel_num = SSM(sci, SCI_GETSELECTIONS, 0, 0);
	LspAutocompleteSymbol *sym;

	if (!displayed_autocomplete_symbols || index >= displayed_autocomplete_symbols->len)
		return;

	sym = displayed_autocomplete_symbols->pdata[index];
	/* The sent_request_id == received_request_id detects the condition when
	 * user typed a character and pressed enter immediately afterwards in which
	 * case the autocompletion list doesn't contain up-to-date text edits.
	 * In this case we have to fall back to insert text based autocompletion
	 * below. */
	if (sel_num == 1 && sym->text_edit && sent_request_id == received_request_id)
	{
		if (server->config.autocomplete_apply_additional_edits && sym->additional_edits)
			lsp_utils_apply_text_edits(sci, sym->text_edit, sym->additional_edits, sym->is_snippet);
		else
			lsp_utils_apply_text_edit(sci, sym->text_edit, sym->is_snippet);
	}
	else
	{
		gchar *insert_text = sym->insert_text ? sym->insert_text : sym->label;

		if (insert_text)
		{
			gint i;

			for (i = 0; i < sel_num; i++)
			{
				gint pos = SSM(sci, SCI_GETSELECTIONNCARET, i, 0);
				guint rootlen = get_ident_prefixlen(server->config.word_chars, doc, pos);
				LspTextEdit text_edit;

				text_edit.new_text = insert_text;
				text_edit.range.start = lsp_utils_scintilla_pos_to_lsp(sci, pos - rootlen);
				text_edit.range.end = lsp_utils_scintilla_pos_to_lsp(sci, pos);

				lsp_utils_apply_text_edit(sci, &text_edit, sym->is_snippet);
			}
		}

		/* See comment above, prevents re-showing the autocompletion popup
		 * in this case. */
		if (sent_request_id != received_request_id)
			lsp_autocomplete_discard_pending_requests();
	}
}


void lsp_autocomplete_clear_statusbar(void)
{
	if (statusbar_modified)
		ui_set_statusbar(FALSE, " ");
	statusbar_modified = FALSE;
}


static void resolve_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	ResolveData *data = user_data;
	LspServer *server = lsp_server_get_if_running(data->doc);

	if (!error && server && data->doc == document_get_current() && displayed_autocomplete_symbols &&
		g_ptr_array_find(displayed_autocomplete_symbols, data->symbol, NULL))
	{
		const gchar *documentation = NULL;

		if (!JSONRPC_MESSAGE_PARSE(return_value, "documentation", JSONRPC_MESSAGE_GET_STRING(&documentation)))
		{
			JSONRPC_MESSAGE_PARSE(return_value, "documentation", "{",
				"value", JSONRPC_MESSAGE_GET_STRING(&documentation),
			"}");
		}

		if (documentation)
		{
			gint current_selection = SSM(data->doc->editor->sci, SCI_AUTOCGETCURRENT, 0, 0);
			gchar *label;

			g_free(data->symbol->documentation);
			data->symbol->documentation = g_strdup(documentation);
			data->symbol->resolved = TRUE;

			if (current_selection < displayed_autocomplete_symbols->len)
			{
				LspAutocompleteSymbol *sym = displayed_autocomplete_symbols->pdata[current_selection];

				label = get_symbol_label(server, sym);
				if (sym == data->symbol)
					lsp_autocomplete_selection_changed(data->doc, label);  // reshow
				g_free(label);
			}
		}

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));
	}

	g_free(data);
}


LspAutocompleteSymbol *find_symbol(GeanyDocument *doc, const gchar *text)
{
	LspServer *srv = lsp_server_get(doc);
	LspAutocompleteSymbol *sym = NULL;
	guint i;

	if (!srv || !displayed_autocomplete_symbols)
		return NULL;

	foreach_ptr_array(sym, i, displayed_autocomplete_symbols)
	{
		gchar *label = get_symbol_label(srv, sym);
		gboolean should_return = FALSE;

		if (g_strcmp0(label, text) == 0)
			should_return = TRUE;

		g_free(label);

		if (should_return)
			return sym;
	}

	return NULL;
}


void lsp_autocomplete_selection_changed(GeanyDocument *doc, const gchar *text)
{
	LspAutocompleteSymbol *sym = find_symbol(doc, text);
	LspServer *srv = lsp_server_get(doc);

	if (!sym || !srv || !srv->config.autocomplete_show_documentation)
		return;

	if (!sym->resolved && srv->supports_completion_resolve)
	{
		ResolveData *data = g_new0(ResolveData, 1);
		data->doc = doc;
		data->symbol = sym;
		lsp_rpc_call(srv, "completionItem/resolve", sym->raw_symbol, resolve_cb, data);
	}
	else if (!sym->documentation)
		lsp_autocomplete_clear_statusbar();
	else
	{
		GString *str;

		g_strstrip(sym->documentation);
		str = g_string_new(sym->documentation);
		g_string_replace(str, "\n\n", " | ", -1);
		g_string_replace(str, "\n", " ", -1);
		g_string_replace(str, "  ", " ", -1);
		if (!EMPTY(str->str))
		{
			ui_set_statusbar(FALSE, "%s", str->str);
			statusbar_modified = TRUE;
		}
		else
			lsp_autocomplete_clear_statusbar();

		g_string_free(str, TRUE);
	}
}


void lsp_autocomplete_style_init(GeanyDocument *doc)
{
	ScintillaObject *sci = doc->editor->sci;
	LspServer *srv = lsp_server_get_if_running(doc);

	// make sure to revert to default Geany behavior when autocompletion not
	// available
	SSM(sci, SCI_AUTOCSETAUTOHIDE, TRUE, 0);

	if (!srv || !srv->config.autocomplete_enable)
		return;

	SSM(sci, SCI_AUTOCSETORDER, SC_ORDER_CUSTOM, 0);
	SSM(sci, SCI_AUTOCSETMULTI, SC_MULTIAUTOC_EACH, 0);
	SSM(sci, SCI_AUTOCSETAUTOHIDE, FALSE, 0);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, srv->config.autocomplete_window_max_displayed, 0);
	SSM(sci, SCI_AUTOCSETMAXWIDTH, srv->config.autocomplete_window_max_width, 0);
	SSM(sci, SCI_SETMULTIPASTE, TRUE, 0);
// TODO: remove eventually
#ifdef SC_AUTOCOMPLETE_SELECT_FIRST_ITEM
	SSM(sci, SCI_AUTOCSETOPTIONS, SC_AUTOCOMPLETE_SELECT_FIRST_ITEM, 0);
#endif
}


static void show_tags_list(LspServer *server, GeanyDocument *doc, GPtrArray *symbols)
{
	guint i;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	GString *words = g_string_sized_new(2000);
	gchar *label;
	gchar *first_label = NULL;

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
		if (!first_label)
			first_label = g_strdup(label);
		g_string_append(words, label);

		sprintf(buf, "?%u", icon_id + 1);
		g_string_append(words, buf);

		g_free(label);
	}

	lsp_autocomplete_set_displayed_symbols(symbols);
	SSM(sci, SCI_AUTOCSHOW, get_ident_prefixlen(server->config.word_chars, doc, pos), (sptr_t) words->str);
	if (first_label)
	{
		lsp_autocomplete_selection_changed(doc, first_label);
		g_free(first_label);
	}

// TODO: remove eventually
#ifndef SC_AUTOCOMPLETE_SELECT_FIRST_ITEM
	if (SSM(sci, SCI_AUTOCGETCURRENT, 0, 0) != 0)
	{
		//make sure Scintilla selects the first item - see https://sourceforge.net/p/scintilla/bugs/2403/
		label = get_symbol_label(server, symbols->pdata[0]);
		SSM(sci, SCI_AUTOCSELECT, 0, (sptr_t)label);
		g_free(label);
	}
#endif

	g_string_free(words, TRUE);
}


static gint strstr_delta(const gchar *s1, const gchar *s2)
{
	const gchar *pos = strstr(s1, s2);
	if (!pos)
		return -1;
	return pos - s1;
}


static gboolean has_identifier_chars(const gchar *s, const gchar *word_chars)
{
	gint i;

	for (i = 0; s[i]; i++)
	{
		if (!strchr(word_chars, s[i]))
			return FALSE;
	}
	return TRUE;
}


static void get_letter_counts(gint *counts, const gchar *str)
{
	gint i;

	for (i = 0; str[i]; i++)
	{
		gchar c = str[i];
		if (c >= 'a' && c <= 'z')
			counts[c-'a']++;
	}
}


// fuzzy filtering - only require the same letters appear in name and prefix and
// that the first two letters of prefix appear as a substring in name. Most
// servers filter by themselves and this avoids filtering-out good suggestions
// when the typed string is just slightly misspelled. For servers that don't
// filter by themselves this filters the the strings that are totally out and
// together with sorting presents reasonable suggestions
static gboolean should_filter(const gchar *name, const gchar *prefix)
{
	gint name_letters['z'-'a'+1] = {0};
	gint prefix_letters['z'-'a'+1] = {0};
	gchar c;

	get_letter_counts(name_letters, name);
	get_letter_counts(prefix_letters, prefix);

	for (c = 'a'; c <= 'z'; c++)
	{
		if (name_letters[c-'a'] < prefix_letters[c-'a'])
			return TRUE;
	}

	if (strlen(prefix) >= 2)
	{
		gchar pref[] = {prefix[0], prefix[1], '\0'};
		if (!strstr(name, pref))
			return TRUE;
	}

	return FALSE;
}


static gboolean filter_autocomplete_symbols(LspAutocompleteSymbol *sym, const gchar *text,
	gboolean use_label)
{
	const gchar *filter_text;

	if (EMPTY(text))
		return FALSE;

	filter_text = sym->filter_text ? sym->filter_text : get_label(sym, use_label);

	return GPOINTER_TO_INT(lsp_utils_lowercase_cmp((LspUtilsCmpFn)should_filter, filter_text, text));
}


static gint sort_autocomplete_symbols(gconstpointer a, gconstpointer b, gpointer user_data)
{
	LspAutocompleteSymbol *sym1 = *((LspAutocompleteSymbol **)a);
	LspAutocompleteSymbol *sym2 = *((LspAutocompleteSymbol **)b);
	SortData *sort_data = user_data;
	const gchar *label1 = get_label(sym1, sort_data->use_label);
	const gchar *label2 = get_label(sym2, sort_data->use_label);

	if (sort_data->pass == 2 && label1 && label2 && sort_data->prefix)
	{
		const gchar *wc = sort_data->word_chars;
		gint diff1, diff2;

		if (g_strcmp0(label1, sort_data->prefix) == 0 && g_strcmp0(label2, sort_data->prefix) != 0)
			return -1;
		if (g_strcmp0(label1, sort_data->prefix) != 0 && g_strcmp0(label2, sort_data->prefix) == 0)
			return 1;

		if (g_str_has_prefix(label1, sort_data->prefix) && !g_str_has_prefix(label2, sort_data->prefix))
			return -1;
		if (!g_str_has_prefix(label1, sort_data->prefix) && g_str_has_prefix(label2, sort_data->prefix))
			return 1;

		// case insensitive variants
		if (utils_str_casecmp(label1, sort_data->prefix) == 0 && utils_str_casecmp(label2, sort_data->prefix) != 0)
			return -1;
		if (utils_str_casecmp(label1, sort_data->prefix) != 0 && utils_str_casecmp(label2, sort_data->prefix) == 0)
			return 1;

		if (lsp_utils_lowercase_cmp((LspUtilsCmpFn)g_str_has_prefix, label1, sort_data->prefix) &&
			!lsp_utils_lowercase_cmp((LspUtilsCmpFn)g_str_has_prefix, label2, sort_data->prefix))
			return -1;
		if (!lsp_utils_lowercase_cmp((LspUtilsCmpFn)g_str_has_prefix, label1, sort_data->prefix) &&
			lsp_utils_lowercase_cmp((LspUtilsCmpFn)g_str_has_prefix, label2, sort_data->prefix))
			return 1;

		// anywhere within string, any case, earlier occurrence wins
		diff1 = GPOINTER_TO_INT(lsp_utils_lowercase_cmp(
			(LspUtilsCmpFn)strstr_delta, label1, sort_data->prefix));
		diff2 = GPOINTER_TO_INT(lsp_utils_lowercase_cmp(
			(LspUtilsCmpFn)strstr_delta, label2, sort_data->prefix));
		if (diff1 != -1 && diff2 == -1)
			return -1;
		if (diff1 == -1 && diff2 != -1)
			return 1;
		if (diff1 != -1 && diff2 != -1 && diff1 != diff2)
			return diff1 - diff2;

		if (has_identifier_chars(label1, wc) && !has_identifier_chars(label2, wc))
			return -1;
		if (!has_identifier_chars(label1, wc) && has_identifier_chars(label2, wc))
			return 1;
	}

	if (sym1->sort_text && sym2->sort_text)
		return g_strcmp0(sym1->sort_text, sym2->sort_text);

	if (label1 && label2)
		return utils_str_casecmp(label1, label2);

	return 0;
}


static gboolean should_add(GPtrArray *symbols, const gchar *prefix)
{
	LspAutocompleteSymbol *sym;
	const gchar *label;

	if (symbols->len == 0)
		return FALSE;

	if (symbols->len > 1)
		return TRUE;

	// don't add single value with what's already typed unless it's a snippet
	sym = symbols->pdata[0];
	label = get_label(sym, FALSE);
	if (g_strcmp0(label, prefix) != 0)
		return TRUE;

	return sym->is_snippet;
}


static void process_response(LspServer *server, GVariant *response, GeanyDocument *doc)
{
	//gboolean is_incomplete = FALSE;
	GVariantIter *iter = NULL;
	GVariant *member = NULL;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	gint prefixlen = get_ident_prefixlen(server->config.word_chars, doc, pos);
	SortData sort_data = { 1, NULL, server->config.autocomplete_use_label, server->config.word_chars };
	GPtrArray *symbols, *symbols_filtered;
	GHashTable *entry_set;
	gint i;

	JSONRPC_MESSAGE_PARSE(response, 
		//"isIncomplete", JSONRPC_MESSAGE_GET_BOOLEAN(&is_incomplete),
		"items", JSONRPC_MESSAGE_GET_ITER(&iter));

	if (!iter && g_variant_is_of_type(response, G_VARIANT_TYPE_ARRAY))
		iter = g_variant_iter_new(response);

	if (!iter)
	{
		SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
		return;
	}

	symbols = g_ptr_array_new_full(0, NULL);  // not freeing symbols here

	while (g_variant_iter_next(iter, "v", &member))
	{
		LspAutocompleteSymbol *sym;
		GVariant *text_edit = NULL;
		GVariantIter *additional_edits = NULL;
		const gchar *label = NULL;
		const gchar *insert_text = NULL;
		const gchar *sort_text = NULL;
		const gchar *filter_text = NULL;
		const gchar *detail = NULL;
		const gchar *documentation = NULL;
		gint64 kind = 0;
		gint64 format = 0;

		JSONRPC_MESSAGE_PARSE(member, "kind", JSONRPC_MESSAGE_GET_INT64(&kind));

		if (kind == LspCompletionKindSnippet && !server->config.autocomplete_use_snippets)
		{
			g_variant_unref(member);
			continue;
		}

		JSONRPC_MESSAGE_PARSE(member, "insertText", JSONRPC_MESSAGE_GET_STRING(&insert_text));
		JSONRPC_MESSAGE_PARSE(member, "insertTextFormat", JSONRPC_MESSAGE_GET_INT64(&format));

		if (!server->config.autocomplete_use_snippets && format == 2 &&
			// Lua server flags as snippet without actually being a snippet
			insert_text && strchr(insert_text, '$'))
		{
			g_variant_unref(member);
			continue;
		}

		JSONRPC_MESSAGE_PARSE(member, "label", JSONRPC_MESSAGE_GET_STRING(&label));
		JSONRPC_MESSAGE_PARSE(member, "sortText", JSONRPC_MESSAGE_GET_STRING(&sort_text));
		JSONRPC_MESSAGE_PARSE(member, "filterText", JSONRPC_MESSAGE_GET_STRING(&filter_text));
		JSONRPC_MESSAGE_PARSE(member, "detail", JSONRPC_MESSAGE_GET_STRING(&detail));
		JSONRPC_MESSAGE_PARSE(member, "textEdit", JSONRPC_MESSAGE_GET_VARIANT(&text_edit));
		JSONRPC_MESSAGE_PARSE(member, "additionalTextEdits", JSONRPC_MESSAGE_GET_ITER(&additional_edits));

		if (!JSONRPC_MESSAGE_PARSE(member, "documentation", JSONRPC_MESSAGE_GET_STRING(&documentation)))
		{
			JSONRPC_MESSAGE_PARSE(member, "documentation", "{",
				"value", JSONRPC_MESSAGE_GET_STRING(&documentation),
			"}");
		}

		sym = g_new0(LspAutocompleteSymbol, 1);
		sym->label = g_strdup(label);
		sym->insert_text = g_strdup(insert_text);
		sym->sort_text = g_strdup(sort_text);
		sym->detail = g_strdup(detail);
		sym->documentation = g_strdup(documentation);
		sym->kind = kind;
		sym->text_edit = lsp_utils_parse_text_edit(text_edit);
		sym->additional_edits = lsp_utils_parse_text_edits(additional_edits);
		sym->is_snippet = (format == 2);
		sym->raw_symbol = member;

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

	if (prefixlen > 0)
		sort_data.prefix = sci_get_contents_range(sci, pos - prefixlen, pos);

	/* remove duplicates and items not matching filtering criteria */
	for (i = 0; i < symbols->len; i++)
	{
		LspAutocompleteSymbol *sym = symbols->pdata[i];
		gchar *display_label = get_symbol_label(server, sym);

		if (g_hash_table_contains(entry_set, display_label) ||
			filter_autocomplete_symbols(sym, sort_data.prefix, sort_data.use_label))
		{
			free_autocomplete_symbol(sym);
			g_free(display_label);
		}
		else
		{
			g_ptr_array_add(symbols_filtered, sym);
			g_hash_table_insert(entry_set, display_label, NULL);
		}
	}

	g_ptr_array_free(symbols, TRUE);
	symbols = symbols_filtered;

	sort_data.pass = 2;
	/* sort with symbols matching the typed prefix first */
	g_ptr_array_sort_with_data(symbols, sort_autocomplete_symbols, &sort_data);

	if (should_add(symbols, sort_data.prefix))
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
	GVariant *node;
	gchar *doc_uri;
	LspAutocompleteAsyncData *data;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	gint pos_before = SSM(sci, SCI_POSITIONBEFORE, pos, 0);
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gint lexer = sci_get_lexer(sci);
	gint style = sci_get_style_at(sci, pos_before);
	gint style_before = sci_get_style_at(sci, SSM(sci, SCI_POSITIONBEFORE, pos_before, 0));
	gboolean is_trigger_char = FALSE;
	gchar c = pos > 0 ? sci_get_char_at(sci, pos_before) : '\0';
	gchar c_str[2] = {c, '\0'};
	gint prefixlen = get_ident_prefixlen(server->config.word_chars, doc, pos);

	// also check position before the just typed characters (i.e. 2 positions
	// before pos) - at least for Python comments typing at EOL probably doesn't
	// have up-to-date styling information
	if ((!server->config.autocomplete_in_strings &&
		(highlighting_is_string_style(lexer, style) || highlighting_is_string_style(lexer, style_before))) ||
		(highlighting_is_comment_style(lexer, style) || highlighting_is_comment_style(lexer, style_before)))
	{
		SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
		return;
	}

	if (prefixlen == 0)
	{
		if (!EMPTY(server->config.autocomplete_trigger_sequences) &&
			!ends_with_sequence(sci, server->config.autocomplete_trigger_sequences))
		{
			SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
			return;
		}

		if (EMPTY(server->autocomplete_trigger_chars) ||
			!strchr(server->autocomplete_trigger_chars, c) ||
			c == ' ')  // Lua LSP has the stupid idea of putting ' ' into trigger chars
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
		if (pos != next_pos &&  // not at EOF
			(prefixlen + (next_pos - pos) == get_ident_prefixlen(server->config.word_chars, doc, next_pos)))
		{
			SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
			return;  /* avoid autocompletion in the middle of a word */
		}

		if (!EMPTY(server->config.autocomplete_hide_after_words))
		{
			gchar **comps = g_strsplit(server->config.autocomplete_hide_after_words, ";", -1);
			gchar *prefix = sci_get_contents_range(sci, pos - prefixlen, pos);
			gboolean found = FALSE;
			gchar **comp;

			foreach_strv(comp, comps)
			{
				if (utils_str_casecmp(*comp, prefix) == 0)
				{
					found = TRUE;
					break;
				}
			}
			g_free(prefix);
			g_strfreev(comps);

			if (found)
			{
				SSM(doc->editor->sci, SCI_AUTOCCANCEL, 0, 0);
				return;
			}
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
			"triggerCharacter", JSONRPC_MESSAGE_PUT_STRING(is_trigger_char ? c_str : NULL),
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
