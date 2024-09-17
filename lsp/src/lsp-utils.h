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

#ifndef LSP_UTILS_H
#define LSP_UTILS_H 1

#include <geanyplugin.h>
#include <jsonrpc-glib.h>

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

struct LspServerConfig;
typedef struct LspServerConfig LspServerConfig;

typedef enum
{
	UnconfiguredConfiguration = -1,
	DisabledConfiguration,
	EnabledConfiguration
} LspProjectConfiguration;


typedef enum
{
	UserConfigurationType,
	ProjectConfigurationType,
} LspProjectConfigurationType;


typedef struct
{
	gint64 line;       // zero-based
	gint64 character;  // pos. on line - number of code units in UTF-16, zero-based
} LspPosition;


typedef struct
{
	LspPosition start;
	LspPosition end;
} LspRange;


typedef struct
{
	gchar *new_text;
	LspRange range;
} LspTextEdit;


typedef struct
{
	gchar *uri;
	LspRange range;
} LspLocation;


typedef gpointer (* LspUtilsCmpFn)(const gchar *s1, const gchar *s2);


void lsp_utils_free_lsp_text_edit(LspTextEdit *e);
void lsp_utils_free_lsp_location(LspLocation *e);

LspPosition lsp_utils_scintilla_pos_to_lsp(ScintillaObject *sci, gint sci_pos);
gint lsp_utils_lsp_pos_to_scintilla(ScintillaObject *sci, LspPosition lsp_pos);

gchar *lsp_utils_get_doc_uri(GeanyDocument *doc);
gchar *lsp_utils_get_lsp_lang_id(GeanyDocument *doc);
gchar *lsp_utils_get_real_path_from_uri_locale(const gchar *uri);
gchar *lsp_utils_get_real_path_from_uri_utf8(const gchar *uri);

gchar *lsp_utils_json_pretty_print(GVariant *variant);

gchar *lsp_utils_get_project_base_path(void);

const gchar *lsp_utils_get_global_config_filename(void);
const gchar *lsp_utils_get_user_config_filename(void);
const gchar *lsp_utils_get_project_config_filename(void);
const gchar *lsp_utils_get_config_filename(void);

gboolean lsp_utils_is_lsp_disabled_for_project(void);

LspPosition lsp_utils_parse_pos(GVariant *variant);
LspRange lsp_utils_parse_range(GVariant *variant);

LspTextEdit *lsp_utils_parse_text_edit(GVariant *variant);
GPtrArray *lsp_utils_parse_text_edits(GVariantIter *iter);

LspLocation *lsp_utils_parse_location(GVariant *variant);
GPtrArray *lsp_utils_parse_locations(GVariantIter *iter);

void lsp_utils_apply_text_edit(ScintillaObject *sci, LspTextEdit *e, gboolean process_snippets);
void lsp_utils_apply_text_edits(ScintillaObject *sci, LspTextEdit *edit, GPtrArray *edits,
	gboolean process_snippets);
gboolean lsp_utils_apply_workspace_edit(GVariant *workspace_edit);

gboolean lsp_utils_wrap_string(gchar *string, gint wrapstart);

gpointer lsp_utils_lowercase_cmp(LspUtilsCmpFn cmp, const gchar *s1, const gchar *s2);

GVariant *lsp_utils_parse_json_file_as_variant(const gchar *utf8_fname, const gchar *fallback_json);
JsonNode *lsp_utils_parse_json_file(const gchar *utf8_fname, const gchar *fallback_json);

ScintillaObject *lsp_utils_new_sci_from_file(const gchar *utf8_fname);

gchar *lsp_utils_get_current_iden(GeanyDocument *doc, gint current_pos, const gchar *wordchars);

gint lsp_utils_set_indicator_style(ScintillaObject *sci, const gchar *style_str);

gchar *lsp_utils_get_relative_path(const gchar *utf8_parent, const gchar *utf8_descendant);

void lsp_utils_save_all_docs(void);

gchar *lsp_utils_find_project_root(GeanyDocument *doc, LspServerConfig *cfg);

gchar *lsp_utils_process_snippet(const gchar *snippet, GSList **positions);

#endif  /* LSP_UTILS_H */
