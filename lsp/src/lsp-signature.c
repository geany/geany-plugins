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

#include "lsp-signature.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"

#include <jsonrpc-glib.h>


typedef struct {
	GeanyDocument *doc;
	gint pos;
	gboolean force;
} LspSignatureData;


static GPtrArray *signatures = NULL;
static gint displayed_signature = 0;
static ScintillaObject *calltip_sci;


static void show_signature(ScintillaObject *sci)
{
	gboolean have_arrow = FALSE;
	GString *str = g_string_new(NULL);

	if (displayed_signature > 0)
	{
		g_string_append_c(str, '\001');  /* up arrow */
		have_arrow = TRUE;
	}
	if (displayed_signature < signatures->len - 1)
	{
		g_string_append_c(str, '\002');  /* down arrow */
		have_arrow = TRUE;
	}
	if (have_arrow)
		g_string_append_c(str, ' ');
	g_string_append(str, signatures->pdata[displayed_signature]);

	lsp_utils_wrap_string(str->str, -1);
	calltip_sci = sci;
	SSM(sci, SCI_CALLTIPSHOW, sci_get_current_position(sci), (sptr_t) str->str);

	g_string_free(str, TRUE);
}


static void signature_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		GeanyDocument *current_doc = document_get_current();
		LspSignatureData *data = user_data;

		//printf("%s\n", lsp_utils_json_pretty_print(return_value));

		if (current_doc == data->doc)
		{
			if (!g_variant_is_of_type(return_value, G_VARIANT_TYPE_DICTIONARY) &&
				lsp_signature_showing_calltip(current_doc))
			{
				// null response
				lsp_signature_hide_calltip(current_doc);
			}
			else if (sci_get_current_position(current_doc->editor->sci) < data->pos + 10 &&
				(data->force || (!data->force && !SSM(current_doc->editor->sci, SCI_AUTOCACTIVE, 0, 0))))
			{
				GVariantIter *iter = NULL;
				gint64 active = 1;

				JSONRPC_MESSAGE_PARSE(return_value, "signatures", JSONRPC_MESSAGE_GET_ITER(&iter));
				JSONRPC_MESSAGE_PARSE(return_value, "activeSignature", JSONRPC_MESSAGE_GET_INT64(&active));

				if (signatures)
					g_ptr_array_free(signatures, TRUE);
				signatures = g_ptr_array_new_full(1, g_free);

				if (iter)
				{
					GVariant *member = NULL;

					while (g_variant_iter_loop(iter, "v", &member))
					{
						const gchar *label = NULL;

						JSONRPC_MESSAGE_PARSE(member, "label", JSONRPC_MESSAGE_GET_STRING(&label));

						if (label)
							g_ptr_array_add(signatures, g_strdup(label));
					}
				}

				displayed_signature = CLAMP(active, 1, signatures->len) - 1;

				if (signatures->len == 0)
					SSM(current_doc->editor->sci, SCI_CALLTIPCANCEL, 0, 0);
				else
					show_signature(current_doc->editor->sci);

				if (iter)
					g_variant_iter_free(iter);
			}
		}
	}

	g_free(user_data);
}


void lsp_signature_show_prev(void)
{
	GeanyDocument *doc = document_get_current();

	if (!doc || !signatures)
		return;

	if (displayed_signature > 0)
		displayed_signature--;
	show_signature(doc->editor->sci);
}


void lsp_signature_show_next(void)
{
	GeanyDocument *doc = document_get_current();

	if (!doc || !signatures)
		return;

	if (displayed_signature < signatures->len - 1)
		displayed_signature++;
	show_signature(doc->editor->sci);
}


void lsp_signature_send_request(LspServer *server, GeanyDocument *doc, gboolean force)
{
	GVariant *node;
	gchar *doc_uri;
	LspSignatureData *data;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar c = (pos > 0 && !force) ? sci_get_char_at(sci, SSM(sci, SCI_POSITIONBEFORE, pos, 0)) : '\0';
	const gchar *trigger_chars = server->signature_trigger_chars;

	if (!force && EMPTY(trigger_chars))
		return;

	if ((c == ')' && strchr(trigger_chars, '(') && !strchr(trigger_chars, ')')) ||
		(c == ']' && strchr(trigger_chars, '[') && !strchr(trigger_chars, ']')) ||
		(c == '>' && strchr(trigger_chars, '<') && !strchr(trigger_chars, '>')) ||
		(c == '}' && strchr(trigger_chars, '{') && !strchr(trigger_chars, '}')))
	{
		lsp_signature_hide_calltip(doc);
		return;
	}

	if (!force && !strchr(trigger_chars, c))
		return;

	doc_uri = lsp_utils_get_doc_uri(doc);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"position", "{",
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	data = g_new0(LspSignatureData, 1);
	data->doc = doc;
	data->pos = pos;
	data->force = force;

	lsp_rpc_call(server, "textDocument/signatureHelp", node,
		signature_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}


gboolean lsp_signature_showing_calltip(GeanyDocument *doc)
{
	return SSM(doc->editor->sci, SCI_CALLTIPACTIVE, 0, 0) &&
		calltip_sci == doc->editor->sci && signatures && signatures->len > 0;
}


void lsp_signature_hide_calltip(GeanyDocument *doc)
{
	if (calltip_sci == doc->editor->sci && signatures && signatures->len > 0)
	{
		SSM(doc->editor->sci, SCI_CALLTIPCANCEL, 0, 0);
		g_ptr_array_free(signatures, TRUE);
		signatures = NULL;
		calltip_sci = NULL;
	}
}
