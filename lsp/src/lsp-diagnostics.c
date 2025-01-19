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

extern GeanyData *geany_data;


typedef struct {
	LspRange range;
	gchar *code;
	gchar *source;
	gchar *message;
	gint severity;
	GVariant *diag_raw;
} LspDiag;


typedef struct {
	const gchar *fname;
	const LspDiag *diag;
} LspFileDiag;


typedef enum {
	LSP_DIAG_SEVERITY_MIN = 1,
	LspError = 1,
	LspWarning,
	LspInfo,
	LspHint,
	LSP_DIAG_SEVERITY_MAX
} LspDiagSeverity;


static gint style_indices[LSP_DIAG_SEVERITY_MAX];

static ScintillaObject *calltip_sci;
static GtkWidget *issue_label;
static GtkWidget *issue_label_container;


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


void lsp_diagnostics_common_destroy(void)
{
	if (issue_label)
		gtk_widget_destroy(issue_label);
	if (issue_label_container)
		gtk_widget_destroy(issue_label_container);
	calltip_sci = NULL;
	issue_label = NULL;
	issue_label_container = NULL;
}


void lsp_diagnostics_init(LspServer *srv)
{
	if (!srv->diag_table)
		srv->diag_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)array_free);
	g_hash_table_remove_all(srv->diag_table);
}


void lsp_diagnostics_free(LspServer *srv)
{
	if (srv->diag_table)
		g_hash_table_destroy(srv->diag_table);
	srv->diag_table = NULL;
}


static LspDiag *get_diag(gint pos, gint where)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);
	LspDiag *previous_diag = NULL;
	GPtrArray *diags;
	gint i;

	if (!srv || !doc->real_path)
		return NULL;

	diags = g_hash_table_lookup(srv->diag_table, doc->real_path);
	if (!diags)
		return NULL;

	for (i = 0; i < diags->len; i++)
	{
		ScintillaObject *sci = doc->editor->sci;
		LspDiag *diag = diags->pdata[i];
		gint start_pos = lsp_utils_lsp_pos_to_scintilla(sci, diag->range.start);
		gint end_pos = lsp_utils_lsp_pos_to_scintilla(sci, diag->range.end);
		gint index = style_indices[diag->severity];

		if (index == 0)
			continue;

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

	if (!cfg || !cfg->diagnostics_enable)
		return TRUE;

	if (EMPTY(cfg->diagnostics_disable_for))
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
	LspServer *srv = lsp_server_get_if_running(doc);
	LspDiag *diag = get_diag(pos, 0);
	gchar *first = NULL;
	gchar *second;

	if (!srv || !diag || is_diagnostics_disabled_for(doc, &srv->config))
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
		gint index = style_indices[severity];
		if (index > 0)
			sci_indicator_set(sci, index);
		sci_indicator_clear(sci, 0, sci_get_length(sci));
	}
}


static gboolean on_issue_label_clicked(GtkWidget *widget, GdkEventButton *event,
	G_GNUC_UNUSED gpointer user_data)
{
	if (event->button == 1)
	{
		lsp_diagnostics_show_all(TRUE);
	}
	return FALSE;
}


static void create_label(void)
{
	GtkWidget *geany_statusbar;

	issue_label = gtk_label_new("");
	issue_label_container = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(issue_label_container), issue_label);

	geany_statusbar = ui_lookup_widget(geany_data->main_widgets->window, "statusbar");
	gtk_box_pack_start(GTK_BOX(geany_statusbar), issue_label_container, FALSE, FALSE, 4);
	gtk_widget_show_all(issue_label_container);

	g_signal_connect(issue_label_container, "button-press-event",
		G_CALLBACK(on_issue_label_clicked), NULL);
}


static void set_statusbar_issue_num(gint num)
{
	gchar *issue_str;

	if (!issue_label)
		create_label();

	issue_str = num >= 0 ? g_strdup_printf(_("issues: %d"), num) : g_strdup("");
	gtk_label_set_text(GTK_LABEL(issue_label), issue_str);
	g_free(issue_str);
}


static void refresh_issue_statusbar(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	gint num = 0;

	if (srv && doc->real_path && !is_diagnostics_disabled_for(doc, &srv->config))
	{
		GPtrArray *diags = g_hash_table_lookup(srv->diag_table, doc->real_path);
		gint i;

		for (i = 0; diags && i < diags->len; i++)
		{
			LspDiag *diag = diags->pdata[i];

			if (diag->severity <= srv->config.diagnostics_statusbar_severity)
				num++;
		}
	}

	set_statusbar_issue_num(num);
}


void lsp_diagnostics_redraw(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	ScintillaObject *sci;
	GPtrArray *diags;
	gint last_start_pos = 0, last_end_pos = 0;
	gint i;

	if (!srv || !doc || !doc->real_path || is_diagnostics_disabled_for(doc, &srv->config))
	{
		set_statusbar_issue_num(-1);
		if (doc)
			clear_indicators(doc->editor->sci);
		return;
	}

	sci = doc->editor->sci;

	clear_indicators(sci);

	diags = g_hash_table_lookup(srv->diag_table, doc->real_path);
	if (!diags)
	{
		set_statusbar_issue_num(0);
		return;
	}

	for (i = 0; i < diags->len; i++)
	{
		LspDiag *diag = diags->pdata[i];
		gint start_pos = lsp_utils_lsp_pos_to_scintilla(sci, diag->range.start);
		gint end_pos = lsp_utils_lsp_pos_to_scintilla(sci, diag->range.end);
		gint next_pos = SSM(sci, SCI_POSITIONAFTER, start_pos, 0);

		if (start_pos == end_pos)
		{
			start_pos = SSM(sci, SCI_POSITIONBEFORE, start_pos, 0);
			end_pos = SSM(sci, SCI_POSITIONAFTER, end_pos, 0);
		}

		// if the error range spans from the last character on line to the
		// first character on the next line (e.g. missing ':' in Python after else),
		// it won't get drawn by Scintilla
		if (end_pos == next_pos &&
			sci_get_line_from_position(sci, start_pos) + 1 == sci_get_line_from_position(sci, end_pos))
		{
			start_pos = SSM(sci, SCI_POSITIONBEFORE, start_pos, 0);
		}

		if (start_pos != last_start_pos || end_pos != last_end_pos)
		{
			gint index = style_indices[diag->severity];
			if (index > 0)
				editor_indicator_set_on_range(doc->editor, index, start_pos, end_pos);
			last_start_pos = start_pos;
			last_end_pos = end_pos;
		}
	}

	refresh_issue_statusbar(doc);
}


void lsp_diagnostics_style_init(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	ScintillaObject *sci;

	if (!srv)
		return;

	sci = doc->editor->sci;

	style_indices[LspError] = lsp_utils_set_indicator_style(sci, srv->config.diagnostics_error_style);
	style_indices[LspWarning] = lsp_utils_set_indicator_style(sci, srv->config.diagnostics_warning_style);
	style_indices[LspInfo] = lsp_utils_set_indicator_style(sci, srv->config.diagnostics_info_style);
	style_indices[LspHint] = lsp_utils_set_indicator_style(sci, srv->config.diagnostics_hint_style);

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


void lsp_diagnostics_received(LspServer *srv, GVariant* diags)
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

	g_hash_table_insert(srv->diag_table, g_strdup(real_path), arr);

	if (doc && doc->real_path && g_strcmp0(doc->real_path, real_path) == 0)
		lsp_diagnostics_redraw(doc);

	g_variant_iter_free(iter);
	g_free(real_path);
}


void lsp_diagnostics_clear(LspServer *srv, GeanyDocument *doc)
{
	if (srv && doc && doc->real_path)
	{
		g_hash_table_remove(srv->diag_table, doc->real_path);
		lsp_diagnostics_redraw(doc);
	}

	refresh_issue_statusbar(doc);
}


void lsp_diagnostics_hide_calltip(GeanyDocument *doc)
{
	if (doc->editor->sci == calltip_sci)
	{
		SSM(doc->editor->sci, SCI_CALLTIPCANCEL, 0, 0);
		calltip_sci = NULL;
	}
}


static gint compare_diags(gconstpointer a, gconstpointer b)
{
	const LspFileDiag *item_a = *((LspFileDiag **) a);
	const LspFileDiag *item_b = *((LspFileDiag **) b);
	gint res = g_strcmp0(item_a->fname, item_b->fname);

	if (res != 0)
		return res;

	if (item_a->diag->range.start.line < item_b->diag->range.start.line)
		return -1;
	if (item_a->diag->range.start.line > item_b->diag->range.start.line)
		return 1;

	if (item_a->diag->severity < item_b->diag->severity)
		return -1;
	if (item_a->diag->severity > item_b->diag->severity)
		return 1;

	return 0;
}


static void replace_char(gchar *str, gchar find, gchar replace)
{
	gchar *current_pos = strchr(str, find);
	while (current_pos)
	{
		*current_pos = replace;
		current_pos = strchr(current_pos, find);
	}
}


static void show_in_msgwin(LspFileDiag *diag)
{
	gint lineno = diag->diag->range.start.line;
	gchar *fname = utils_get_utf8_from_locale(diag->fname);
	gchar *new_msg = g_strdup(diag->diag->message);
	gchar *base_path;

	base_path = lsp_utils_get_project_base_path();

	if (base_path)
	{
		gchar *rel_path = lsp_utils_get_relative_path(base_path, fname);
		gchar *locale_base_path = utils_get_locale_from_utf8(base_path);

		if (rel_path && !g_str_has_prefix(rel_path, ".."))
			SETPTR(fname, g_strdup(rel_path));

		msgwin_set_messages_dir(locale_base_path);

		g_free(locale_base_path);
		g_free(rel_path);
	}

	replace_char(new_msg, '\n', ' ');
	replace_char(new_msg, '\r', ' ');
	msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s:%d:  %s", fname, lineno + 1, new_msg);

	g_free(fname);
	g_free(new_msg);
	g_free(base_path);
}


void lsp_diagnostics_show_all(gboolean current_doc_only)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);
	GPtrArray *arr, *diags;
	LspFileDiag *item;
	GHashTableIter iter;
	const gchar *key;
	guint i;

	if (!srv)
		return;

	arr = g_ptr_array_new_full(100, g_free);

	g_hash_table_iter_init(&iter, srv->diag_table);
	while (g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer *)&diags))
	{
		LspDiag *diag;

		foreach_ptr_array(diag, i, diags)
		{
			if (current_doc_only && !utils_str_equal(doc->real_path, key))
				continue;

			item = g_new0(LspFileDiag, 1);
			item->fname = key;
			item->diag = diag;
			g_ptr_array_add(arr, item);
		}
	}

	g_ptr_array_sort(arr, compare_diags);

	msgwin_clear_tab(MSG_MESSAGE);
	msgwin_switch_tab(MSG_MESSAGE, TRUE);
	foreach_ptr_array(item, i, arr)
	{
		show_in_msgwin(item);
	}

	g_ptr_array_free(arr, TRUE);
}
