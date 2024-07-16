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

#include "lsp-diagnostics.h"
#include "lsp-utils.h"

#include <jsonrpc-glib.h>

static GHashTable *diag_table = NULL;
static ScintillaObject *calltip_sci;


typedef struct {
	LspRange range;
	gchar *code;
	gchar *source;
	gchar *message;
	gint severity;
	GVariant *diag_raw;
} LspDiag;


typedef enum {
	LSP_DIAG_SEVERITY_MIN = 1,
	LspError = 1,
	LspWarning,
	LspInfo,
	LspHint,
	LSP_DIAG_SEVERITY_MAX
} LspDiagSeverity;


static gint style_indices[LSP_DIAG_SEVERITY_MAX];


static void diag_free(LspDiag *diag)
{
	g_free(diag->code);
	g_free(diag->source);
	g_free(diag->message);
	g_variant_unref(diag->diag_raw);
	g_free(diag);
}


static void array_free(GPtrArray *arr)
{
	g_ptr_array_free(arr, TRUE);
}


void lsp_diagnostics_init(void)
{
	if (!diag_table)
		diag_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)array_free);
	g_hash_table_remove_all(diag_table);
}


void lsp_diagnostics_destroy(void)
{
	if (diag_table)
		g_hash_table_destroy(diag_table);
	diag_table = NULL;
	calltip_sci = NULL;
}


static LspDiag *get_diag(gint pos, gint where)
{
	GeanyDocument *doc = document_get_current();
	LspDiag *previous_diag = NULL;
	GPtrArray *diags;
	gint i;

	if (!doc || !doc->real_path)
		return NULL;

	diags = g_hash_table_lookup(diag_table, doc->real_path);
	if (!diags)
		return NULL;

	for (i = 0; i < diags->len; i++)
	{
		ScintillaObject *sci = doc->editor->sci;
		LspDiag *diag = diags->pdata[i];
		gint start_pos = lsp_utils_lsp_pos_to_scintilla(sci, diag->range.start);
		gint end_pos = lsp_utils_lsp_pos_to_scintilla(sci, diag->range.end);

		if (start_pos == end_pos)
		{
			start_pos = SSM(sci, SCI_POSITIONBEFORE, start_pos, 0);
			end_pos = SSM(sci, SCI_POSITIONAFTER, end_pos, 0);
		}

		if (where == 0)  // at the position
		{
			if (pos >= start_pos && pos <= end_pos)
				return diag;
		}
		else if (where == 1)  // after position
		{
			if (start_pos > pos)
				return diag;
		}
		else if (where == -1)  // before position
		{
			if (end_pos < pos)
				previous_diag = diag;
			else
				break;
		}
	}

	if (previous_diag)
		return previous_diag;

	return NULL;
}


gboolean lsp_diagnostics_has_diag(gint pos)
{
	return get_diag(pos, 0) != NULL;
}


GVariant *lsp_diagnostics_get_diag_raw(gint pos)
{
	LspDiag *diag = get_diag(pos, 0);

	if (diag)
		return diag->diag_raw;
	return NULL;
}


void lsp_diagnostics_goto_next_diag(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspDiag *diag = get_diag(pos, 1);

	if (doc && diag)
	{
		gint start_pos = lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, diag->range.start);
		sci_set_current_position(doc->editor->sci, start_pos, TRUE);
	}
}


void lsp_diagnostics_goto_prev_diag(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspDiag *diag = get_diag(pos, -1);

	if (doc && diag)
	{
		gint start_pos = lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, diag->range.start);
		sci_set_current_position(doc->editor->sci, start_pos, TRUE);
	}
}


static gboolean is_diagnostics_disabled_for(GeanyDocument *doc, LspServerConfig *cfg)
{
	gboolean is_disabled = FALSE;
	gint i = 0;
	gchar **comps;
	gchar *fname;

	if (!cfg->diagnostics_disable_for)
		return FALSE;

	comps = g_strsplit(cfg->diagnostics_disable_for, ";", -1);
	fname = utils_get_utf8_from_locale(doc->real_path);

	for (i = 0; comps && comps[i] && !is_disabled; i++)
	{
		// TODO: possibly precompile the glob and store somewhere if performance is a problem
		if (g_pattern_match_simple(comps[i], fname))
			is_disabled = TRUE;
	}

	g_strfreev(comps);
	g_free(fname);

	return is_disabled;
}


void lsp_diagnostics_show_calltip(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServerConfig *cfg = lsp_server_get_config(doc);
	LspDiag *diag = get_diag(pos, 0);
	gchar *first = NULL;
	gchar *second;

	if (!doc || !diag || !cfg || is_diagnostics_disabled_for(doc, cfg))
		return;

	second = diag->message;

	if (diag->code && diag->source)
		first = g_strconcat(diag->code, " (", diag->source, ")", NULL);
	else if (diag->code)
		first = g_strdup(diag->code);
	else if (diag->source)
		first = g_strdup(diag->source);

	if (first || second)
	{
		ScintillaObject *sci = doc->editor->sci;
		gchar *msg;

		if (first && second)
			msg = g_strconcat(first, "\n---\n", second, NULL);
		else if (first)
			msg = g_strdup(first);
		else
			msg = g_strdup(second);

		lsp_utils_wrap_string(msg, -1);

		calltip_sci = sci;
		SSM(sci, SCI_CALLTIPSHOW, pos, (sptr_t) msg);
		g_free(msg);
	}

	g_free(first);
}


static void clear_indicators(ScintillaObject *sci)
{
	gint severity;

	for (severity = LSP_DIAG_SEVERITY_MIN; severity < LSP_DIAG_SEVERITY_MAX; severity++)
	{
		sci_indicator_set(sci, style_indices[severity]);
		sci_indicator_clear(sci, 0, sci_get_length(sci));
	}
}


void lsp_diagnostics_redraw(GeanyDocument *doc)
{
	LspServerConfig *cfg = lsp_server_get_config(doc);
	ScintillaObject *sci;
	GPtrArray *diags;
	gint last_start_pos = 0, last_end_pos = 0;
	gint i;

	if (!doc || !doc->real_path || !cfg)
		return;

	if (is_diagnostics_disabled_for(doc, cfg))
		return;

	sci = doc->editor->sci;

	clear_indicators(sci);

	diags = g_hash_table_lookup(diag_table, doc->real_path);
	if (!diags || !cfg->diagnostics_enable)
		return;

	for (i = 0; i < diags->len; i++)
	{
		LspDiag *diag = diags->pdata[i];
		gint start_pos = lsp_utils_lsp_pos_to_scintilla(sci, diag->range.start);
		gint end_pos = lsp_utils_lsp_pos_to_scintilla(sci, diag->range.end);

		if (start_pos == end_pos)
		{
			start_pos = SSM(sci, SCI_POSITIONBEFORE, start_pos, 0);
			end_pos = SSM(sci, SCI_POSITIONAFTER, end_pos, 0);
		}

		if (start_pos != last_start_pos || end_pos != last_end_pos)
		{
			editor_indicator_set_on_range(doc->editor, style_indices[diag->severity],
				start_pos, end_pos);
			last_start_pos = start_pos;
			last_end_pos = end_pos;
		}
	}
}


void lsp_diagnostics_style_init(GeanyDocument *doc)
{
	LspServerConfig *cfg = lsp_server_get_config(doc);
	ScintillaObject *sci;

	if (!doc || !cfg)
		return;

	sci = doc->editor->sci;

	style_indices[LspError] = lsp_utils_set_indicator_style(sci, cfg->diagnostics_error_style);
	style_indices[LspWarning] = lsp_utils_set_indicator_style(sci, cfg->diagnostics_warning_style);
	style_indices[LspInfo] = lsp_utils_set_indicator_style(sci, cfg->diagnostics_info_style);
	style_indices[LspHint] = lsp_utils_set_indicator_style(sci, cfg->diagnostics_hint_style);

	SSM(sci, SCI_SETMOUSEDWELLTIME, 500, 0);
}


static gint sort_diags(gconstpointer a, gconstpointer b)
{
	LspDiag *d1 = *((LspDiag **)a);
	LspDiag *d2 = *((LspDiag **)b);

	if (d2->range.start.line > d1->range.start.line)
		return -1;
	if (d2->range.start.line < d1->range.start.line)
		return 1;

	if (d2->range.start.character > d1->range.start.character)
		return -1;
	if (d2->range.start.character < d1->range.start.character)
		return 1;

	return d1->severity - d2->severity;
}


void lsp_diagnostics_received(GVariant* diags)
{
	GeanyDocument *doc = document_get_current();;
	GVariantIter *iter = NULL;
	const gchar *uri = NULL;
	gchar *real_path;
	GVariant *diag = NULL;
	GPtrArray *arr;

	JSONRPC_MESSAGE_PARSE(diags,
		"uri", JSONRPC_MESSAGE_GET_STRING(&uri),
		"diagnostics", JSONRPC_MESSAGE_GET_ITER(&iter)
		);

	if (!iter)
		return;

	real_path = lsp_utils_get_real_path_from_uri_locale(uri);

	if (!real_path)
	{
		g_variant_iter_free(iter);
		return;
	}

	arr = g_ptr_array_new_full(10, (GDestroyNotify)diag_free);

	while (g_variant_iter_next(iter, "v", &diag))
	{
		GVariant *range = NULL;
		const gchar *code = NULL;
		const gchar *source = NULL;
		const gchar *message = NULL;
		gint64 severity = 0;
		LspDiag *lsp_diag;

		JSONRPC_MESSAGE_PARSE(diag, "code", JSONRPC_MESSAGE_GET_STRING(&code));
		JSONRPC_MESSAGE_PARSE(diag, "source", JSONRPC_MESSAGE_GET_STRING(&source));
		JSONRPC_MESSAGE_PARSE(diag, "message", JSONRPC_MESSAGE_GET_STRING(&message));
		JSONRPC_MESSAGE_PARSE(diag, "severity", JSONRPC_MESSAGE_GET_INT64(&severity));
		JSONRPC_MESSAGE_PARSE(diag, "range", JSONRPC_MESSAGE_GET_VARIANT(&range));

		lsp_diag = g_new0(LspDiag, 1);
		lsp_diag->code = g_strdup(code);
		lsp_diag->source = g_strdup(source);
		lsp_diag->message = g_strdup(message);
		lsp_diag->severity = severity;
		lsp_diag->range = lsp_utils_parse_range(range);
		lsp_diag->diag_raw = diag;

		g_ptr_array_add(arr, lsp_diag);

		if (range)
			g_variant_unref(range);
	}

	g_ptr_array_sort(arr, sort_diags);

	g_hash_table_insert(diag_table, g_strdup(real_path), arr);

	if (doc && doc->real_path && g_strcmp0(doc->real_path, real_path) == 0)
		lsp_diagnostics_redraw(doc);

	g_variant_iter_free(iter);
	g_free(real_path);
}


void lsp_diagnostics_clear(GeanyDocument *doc)
{
	if (doc && doc->real_path)
	{
		g_hash_table_remove(diag_table, doc->real_path);
		lsp_diagnostics_redraw(doc);
	}
}


void lsp_diagnostics_hide_calltip(GeanyDocument *doc)
{
	if (doc->editor->sci == calltip_sci)
	{
		SSM(doc->editor->sci, SCI_CALLTIPCANCEL, 0, 0);
		calltip_sci = NULL;
	}
}
