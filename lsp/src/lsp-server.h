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

#ifndef LSP_SERVER_H
#define LSP_SERVER_H 1

#include <geanyplugin.h>

#include <gio/gio.h>

typedef void (*LspCallback) (gpointer user_data);

struct LspRpc;
typedef struct LspRpc LspRpc;


typedef struct LspServerConfig
{
	gchar *cmd;
	gchar **env;
	gchar *ref_lang;
	gchar **lang_id_mappings;

	gboolean show_server_stderr;
	gchar *rpc_log;
	gboolean rpc_log_full;
	gboolean send_did_change_configuration;
	gchar *initialization_options_file;
	gchar *word_chars;
	gchar *initialization_options;
	gchar **project_root_marker_patterns;
	gboolean enable_by_default;
	gboolean use_outside_project_dir;
	gboolean use_without_project;

	gboolean autocomplete_enable;
	gchar **autocomplete_trigger_sequences;
	gboolean autocomplete_use_label;
	gboolean autocomplete_apply_additional_edits;
	gint autocomplete_window_max_entries;
	gint autocomplete_window_max_displayed;
	gint autocomplete_window_max_width;
	gboolean autocomplete_use_snippets;
	gchar *autocomplete_hide_after_words;
	gboolean autocomplete_in_strings;
	gboolean autocomplete_show_documentation;

	gboolean diagnostics_enable;
	gint diagnostics_statusbar_severity;
	gchar *diagnostics_disable_for;
	gchar *diagnostics_error_style;
	gchar *diagnostics_warning_style;
	gchar *diagnostics_info_style;
	gchar *diagnostics_hint_style;

	gchar *formatting_options_file;
	gchar *formatting_options;

	gboolean hover_enable;
	gboolean hover_available;
	gint hover_popup_max_lines;
	gint hover_popup_max_paragraphs;

	gboolean signature_enable;

	gboolean goto_enable;

	gboolean document_symbols_enable;
	gchar *document_symbols_tab_label;
	gboolean document_symbols_available;

	gboolean semantic_tokens_enable;
	gboolean semantic_tokens_force_full;
	gchar **semantic_tokens_types;
	gboolean semantic_tokens_supports_delta;
	gboolean semantic_tokens_range_only;
	gint semantic_tokens_lexer_kw_index;
	gchar *semantic_tokens_type_style;

	gboolean highlighting_enable;
	gchar *highlighting_style;

	gboolean code_lens_enable;
	gchar *code_lens_style;

	gboolean goto_declaration_enable;
	gboolean goto_definition_enable;
	gboolean goto_implementation_enable;
	gboolean goto_references_enable;
	gboolean goto_type_definition_enable;

	gboolean document_formatting_enable;
	gboolean range_formatting_enable;
	gboolean format_on_save;

	gboolean progress_bar_enable;

	gboolean execute_command_enable;
	gboolean code_action_enable;
	gboolean selection_range_enable;
	gboolean swap_header_source_enable;
	gchar *command_on_save_regex;
	gint command_keybinding_num;
	GPtrArray *command_regexes;

	gchar *trace_value;
	gboolean enable_telemetry;

	gboolean rename_enable;
} LspServerConfig;


typedef struct
{
	gint type;  // 0: use stream, 1: stdout, 2: stderr
	gboolean full;
	GFileOutputStream *stream;
} LspLogInfo;


typedef struct LspServer
{
	LspRpc *rpc;
	//GSubprocess *process;
	GPid pid;
	GIOStream *stream;
	LspLogInfo log;

	struct LspServer *referenced;
	gboolean not_used;
	gboolean startup_shutdown;
	guint restarts;
	gint filetype;

	LspServerConfig config;

	GHashTable *open_docs;
	GSList *mru_docs;
	GHashTable *diag_table;
	GHashTable *wks_folder_table;
	GSList *progress_ops;

	gchar *autocomplete_trigger_chars;
	gchar *signature_trigger_chars;
	gchar *initialize_response;
	gboolean use_incremental_sync;
	gboolean send_did_save;
	gboolean include_text_on_save;
	gboolean use_workspace_folders;
	gboolean supports_workspace_symbols;
	gboolean supports_completion_resolve;

	guint64 semantic_token_mask;
} LspServer;

typedef void (*LspServerInitializedCallback) (LspServer *srv);

LspServer *lsp_server_get(GeanyDocument *doc);
LspServer *lsp_server_get_for_ft(GeanyFiletype *ft);
LspServer *lsp_server_get_if_running(GeanyDocument *doc);
LspServerConfig *lsp_server_get_all_section_config(void);
gboolean lsp_server_is_usable(GeanyDocument *doc);
GeanyFiletype *lsp_server_get_ft(GeanyDocument *doc, gchar **lsp_lang_id);

void lsp_server_stop_all(gboolean wait);
void lsp_server_init_all(void);

void lsp_server_set_initialized_cb(LspServerInitializedCallback cb);

gboolean lsp_server_uses_init_file(gchar *path);

gchar *lsp_server_get_initialize_responses(void);

#endif  /* LSP_SERVER_H */
