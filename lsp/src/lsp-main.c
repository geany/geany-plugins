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

#include "lsp-server.h"
#include "lsp-sync.h"
#include "lsp-utils.h"
#include "lsp-autocomplete.h"
#include "lsp-diagnostics.h"
#include "lsp-hover.h"
#include "lsp-semtokens.h"
#include "lsp-signature.h"
#include "lsp-goto.h"
#include "lsp-symbols.h"
#include "lsp-goto-anywhere.h"
#include "lsp-format.h"
#include "lsp-highlight.h"
#include "lsp-rename.h"
#include "lsp-command.h"
#include "lsp-code-lens.h"
#include "lsp-symbol.h"
#include "lsp-extension.h"
#include "lsp-workspace-folders.h"
#include "lsp-symbol-tree.h"
#include "lsp-selection-range.h"

#include <sys/time.h>
#include <string.h>

#include <geanyplugin.h>

#include <jsonrpc-glib.h>


// https://github.com/microsoft/language-server-protocol/blob/main/versions/protocol-1-x.md
// https://github.com/microsoft/language-server-protocol/blob/main/versions/protocol-2-x.md


GeanyPlugin *geany_plugin;
GeanyData *geany_data;

LspProjectConfiguration project_configuration = UnconfiguredConfiguration;
LspProjectConfigurationType project_configuration_type = UserConfigurationType;
gchar *project_configuration_file;

static gint last_click_pos;
static gboolean session_loaded;

static gboolean geany_quitting = FALSE;

PLUGIN_VERSION_CHECK(250)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("LSP Client"),
	_("Language server protocol client for Geany"),
	VERSION,
	"Jiri Techet <techet@gmail.com>")

#define UPDATE_SOURCE_DOC_DATA "lsp_update_source"
#define CODE_ACTIONS_PERFORMED "lsp_code_actions_performed"

enum {
	KB_GOTO_DEFINITION,
	KB_GOTO_DECLARATION,
	KB_GOTO_TYPE_DEFINITION,

	KB_GOTO_ANYWHERE,
	KB_GOTO_DOC_SYMBOL,
	KB_GOTO_WORKSPACE_SYMBOL,
	KB_GOTO_LINE,

	KB_GOTO_NEXT_DIAG,
	KB_GOTO_PREV_DIAG,
	KB_SHOW_DIAG,
	KB_SHOW_FILE_DIAGS,
	KB_SHOW_ALL_DIAGS,

	KB_FIND_IMPLEMENTATIONS,
	KB_FIND_REFERENCES,
	KB_HIGHLIGHT_OCCUR,
	KB_HIGHLIGHT_CLEAR,

	KB_EXPAND_SELECTION,
	KB_SHRINK_SELECTION,

	KB_SHOW_HOVER_POPUP,
	KB_SHOW_CODE_ACTIONS,

	KB_SWAP_HEADER_SOURCE,

	KB_RENAME_IN_FILE,
	KB_RENAME_IN_PROJECT,
	KB_FORMAT_CODE,

	KB_RESTART_SERVERS,

	KB_COUNT
};


struct
{
	GtkWidget *parent_item;

	GtkWidget *project_config;
	GtkWidget *user_config;

	GtkWidget *goto_def;
	GtkWidget *goto_decl;
	GtkWidget *goto_type_def;

	GtkWidget *goto_next_diag;
	GtkWidget *goto_prev_diag;
	GtkWidget *show_diag;
	GtkWidget *show_file_diags;
	GtkWidget *show_all_diags;

	GtkWidget *goto_ref;
	GtkWidget *goto_impl;
	GtkWidget *highlight_occur;
	GtkWidget *highlight_clear;

	GtkWidget *rename_in_file;
	GtkWidget *rename_in_project;
	GtkWidget *format_code;

	GtkWidget *expand_selection;
	GtkWidget *shrink_selection;

	GtkWidget *hover_popup;
	GtkWidget *code_action_popup;

	GtkWidget *header_source;
} menu_items;


struct
{
	GtkWidget *command_item;
	GtkWidget *goto_def;
	GtkWidget *goto_ref;
	GtkWidget *rename_in_file;
	GtkWidget *rename_in_project;
	GtkWidget *format_code;
	GtkWidget *separator1;
	GtkWidget *separator2;
} context_menu_items;


struct
{
	GtkWidget *enable_check_button;
	GtkWidget *settings_type_combo;
	GtkWidget *config_file_entry;
	GtkWidget *path_box;
	GtkWidget *properties_tab;
} project_dialog;


static void on_save_finish(GeanyDocument *doc);
static gboolean on_code_actions_received(GPtrArray *actions, gpointer user_data);


static gboolean autocomplete_provided(GeanyDocument *doc, gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return FALSE;

	return lsp_server_is_usable(doc) && srv->config.autocomplete_enable;
}


static void autocomplete_perform(GeanyDocument *doc, gboolean force, gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	lsp_autocomplete_completion(srv, doc, force);
}


static gboolean calltips_provided(GeanyDocument *doc, gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return FALSE;

	return lsp_server_is_usable(doc) && srv->config.signature_enable;
}


static void calltips_show(GeanyDocument *doc, gboolean force, gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	lsp_signature_send_request(srv, doc, force);
}


static gboolean goto_provided(GeanyDocument *doc, gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return FALSE;

	return lsp_server_is_usable(doc) && srv->config.goto_enable;
}


static gboolean goto_perform(GeanyDocument *doc, gint pos, gboolean definition, gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);
	gchar *iden = lsp_utils_get_current_iden(doc, pos, srv->config.word_chars);

	if (!iden)
		return FALSE;

	if (definition)
		lsp_goto_definition(pos);
	else
		lsp_goto_declaration(pos);

	g_free(iden);
	return TRUE;
}


static gboolean symbol_highlight_provided(GeanyDocument *doc, gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return FALSE;

	return lsp_server_is_usable(doc) && srv->config.semantic_tokens_enable;
}


static PluginExtension extension = {
	.autocomplete_provided = autocomplete_provided,
	.autocomplete_perform = autocomplete_perform,

	.calltips_provided = calltips_provided,
	.calltips_show = calltips_show,

	.goto_provided = goto_provided,
	.goto_perform = goto_perform,

	.symbol_highlight_provided = symbol_highlight_provided,
};


static void lsp_symbol_request_cb(gpointer user_data)
{
	GeanyDocument *doc = user_data;

	if (doc == document_get_current())
		lsp_symbol_tree_refresh();
}


static void on_document_new(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	// we don't know the filename yet - nothing for the LSP server
}


static void update_menu(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	gboolean goto_definition_enable = srv && srv->config.goto_definition_enable;
	gboolean selection_range_enable = srv && srv->config.selection_range_enable;
	gboolean goto_references_enable = srv && srv->config.goto_references_enable;
	gboolean goto_type_definition_enable = srv && srv->config.goto_type_definition_enable;
	gboolean document_formatting_enable = srv && srv->config.document_formatting_enable;
	gboolean range_formatting_enable = srv && srv->config.range_formatting_enable;
	gboolean rename_enable = srv && srv->config.rename_enable;
	gboolean highlighting_enable = srv && srv->config.highlighting_enable;
	gboolean goto_declaration_enable = srv && srv->config.goto_declaration_enable;
	gboolean goto_implementation_enable = srv && srv->config.goto_implementation_enable;
	gboolean diagnostics_enable = srv && srv->config.diagnostics_enable;
	gboolean hover_popup_enable = srv && srv->config.hover_available;
	gboolean code_action_enable = srv && (srv->config.code_action_enable || srv->config.code_lens_enable);
	gboolean swap_header_source_enable = srv && srv->config.swap_header_source_enable;

	if (!menu_items.parent_item)
		return;

	gtk_widget_set_sensitive(menu_items.goto_def, goto_definition_enable);
	gtk_widget_set_sensitive(menu_items.goto_decl, goto_declaration_enable);
	gtk_widget_set_sensitive(menu_items.goto_type_def, goto_type_definition_enable);

	gtk_widget_set_sensitive(menu_items.goto_next_diag, diagnostics_enable);
	gtk_widget_set_sensitive(menu_items.goto_prev_diag, diagnostics_enable);
	gtk_widget_set_sensitive(menu_items.show_diag, diagnostics_enable);

	gtk_widget_set_sensitive(menu_items.goto_ref, goto_references_enable);
	gtk_widget_set_sensitive(menu_items.goto_impl, goto_implementation_enable);

	gtk_widget_set_sensitive(menu_items.rename_in_file, highlighting_enable);
	gtk_widget_set_sensitive(menu_items.rename_in_project, rename_enable);
	gtk_widget_set_sensitive(menu_items.format_code, document_formatting_enable || range_formatting_enable);

	gtk_widget_set_sensitive(menu_items.expand_selection, selection_range_enable);
	gtk_widget_set_sensitive(menu_items.shrink_selection, selection_range_enable);

	gtk_widget_set_sensitive(menu_items.header_source, swap_header_source_enable);

	gtk_widget_set_sensitive(menu_items.hover_popup, hover_popup_enable);
	gtk_widget_set_sensitive(menu_items.code_action_popup, code_action_enable);
}


static gboolean on_update_idle(gpointer data)
{
	GeanyDocument *doc = data;
	LspServer *srv;

	plugin_set_document_data(geany_plugin, doc, UPDATE_SOURCE_DOC_DATA, GUINT_TO_POINTER(0));

	if (!DOC_VALID(doc))
		return G_SOURCE_REMOVE;

	srv = lsp_server_get_if_running(doc);
	if (!srv)
		return G_SOURCE_REMOVE;

	lsp_code_lens_send_request(doc);
	if (symbol_highlight_provided(doc, NULL))
		lsp_semtokens_send_request(doc);
	if (srv->config.document_symbols_enable)
		lsp_symbols_doc_request(doc, lsp_symbol_request_cb, doc);

	return G_SOURCE_REMOVE;
}


static void on_document_visible(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get(doc);

	session_loaded = TRUE;

	update_menu(doc);

	// quick synchronous refresh with the last value without server request
	lsp_symbol_tree_refresh();

	// perform also without server - to revert to default Geany behavior
	lsp_autocomplete_style_init(doc);

	lsp_diagnostics_style_init(doc);
	lsp_diagnostics_redraw(doc);

	if (!srv)
		return;

	lsp_highlight_style_init(doc);
	lsp_semtokens_style_init(doc);
	lsp_code_lens_style_init(doc);

	lsp_selection_clear_selections();

	// just in case we didn't get some callback from the server
	on_save_finish(doc);

	// this might not get called for the first time when server gets started because
	// lsp_server_get() returns NULL. However, we also "open" current and modified
	// documents after successful server handshake inside on_server_initialized()
	lsp_sync_text_document_did_open(srv, doc);

	on_update_idle(doc);
}


static gboolean on_doc_close_idle(gpointer user_data)
{
	if (!document_get_current() && menu_items.parent_item)
		update_menu(NULL);  // the last open document was closed

	return G_SOURCE_REMOVE;
}


static void on_document_close(G_GNUC_UNUSED GObject * obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	LspServer *srv = lsp_server_get_if_running(doc);

	plugin_idle_add(geany_plugin, on_doc_close_idle, NULL);

	if (!srv)
		return;

	lsp_diagnostics_clear(srv, doc);
	lsp_semtokens_clear(doc);
	lsp_sync_text_document_did_close(srv, doc);
}


static void stop_and_init_all_servers(void)
{
	lsp_server_stop_all(FALSE);
	session_loaded = FALSE;

	lsp_server_init_all();
	lsp_symbol_tree_init();
}


static void restart_all_servers(void)
{
	GeanyDocument *doc = document_get_current();

	stop_and_init_all_servers();

	if (doc)
		on_document_visible(doc);
}


static void on_document_save(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	LspServer *srv;

	if (g_strcmp0(doc->real_path, lsp_utils_get_config_filename()) == 0)
	{
		stop_and_init_all_servers();
		return;
	}

	if (lsp_server_uses_init_file(doc->real_path))
	{
		stop_and_init_all_servers();
		return;
	}

	srv = lsp_server_get(doc);
	if (!srv)
		return;

	if (!lsp_sync_is_document_open(srv, doc))
	{
		// "new" documents without filename saved for the first time or
		// "save as" performed
		on_document_visible(doc);
	}

	lsp_sync_text_document_did_save(srv, doc);
}


static gboolean code_action_was_performed(LspCommand *cmd, GeanyDocument *doc)
{
	GPtrArray *code_actions_performed = plugin_get_document_data(geany_plugin, doc, CODE_ACTIONS_PERFORMED);
	gchar *name;
	guint i;

	if (!code_actions_performed)
		return TRUE;

	foreach_ptr_array(name, i, code_actions_performed)
	{
		if (g_strcmp0(cmd->title, name) == 0)
			return TRUE;
	}

	return FALSE;
}


static void on_save_finish(GeanyDocument *doc)
{
	if (!DOC_VALID(doc))
		return;

	if (plugin_get_document_data(geany_plugin, doc, CODE_ACTIONS_PERFORMED))
	{
		// save file at the end because the intermediate updates modified the file
		document_save_file(doc, FALSE);
		plugin_set_document_data(geany_plugin, doc, CODE_ACTIONS_PERFORMED, NULL);
	}
}


static void on_command_performed(GeanyDocument *doc)
{
	if (DOC_VALID(doc))
	{
		// re-request code actions on the now modified document
		lsp_command_send_code_action_request(doc, sci_get_current_position(doc->editor->sci),
			on_code_actions_received, doc);
	}
}


static gboolean on_code_actions_received(GPtrArray *actions, gpointer user_data)
{
	GeanyDocument *doc = user_data;
	LspCommand *cmd;
	LspServer *srv;
	guint i;

	if (!DOC_VALID(doc))
		return TRUE;

	srv = lsp_server_get_if_running(doc);

	if (!srv)
		return TRUE;

	foreach_ptr_array(cmd, i, actions)
	{
		if (!code_action_was_performed(cmd, doc) &&
			g_regex_match_simple(srv->config.command_on_save_regex, cmd->title, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
		{
			GPtrArray *code_actions_performed = plugin_get_document_data(geany_plugin, doc, CODE_ACTIONS_PERFORMED);

			// save the performed title so it isn't performed in the next iteration
			g_ptr_array_add(code_actions_performed, g_strdup(cmd->title));
			// perform the code action and re-request code actions in its
			// callback
			lsp_command_perform(srv, cmd, (LspCallback)on_command_performed, doc);
			// this isn't the final call - return now
			return TRUE;
		}
	}

	// we get here only when nothing can be performed above - this is the last
	// code action call
	if (srv->config.document_formatting_enable && srv->config.format_on_save)
		lsp_format_perform(doc, TRUE, (LspCallback)on_save_finish, doc);
	else
		on_save_finish(doc);

	return TRUE;
}


static void free_ptrarray(gpointer data)
{
	g_ptr_array_free((GPtrArray *)data, TRUE);
}


static void on_document_before_save(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	GPtrArray *code_actions_performed = plugin_get_document_data(geany_plugin, doc, CODE_ACTIONS_PERFORMED);
	LspServer *srv = lsp_server_get(doc);

	// code_actions_performed non-NULL when performing save and applying code actions
	if (!srv || code_actions_performed)
		return;

	code_actions_performed = g_ptr_array_new_full(1, g_free);
	plugin_set_document_data_full(geany_plugin, doc, CODE_ACTIONS_PERFORMED, code_actions_performed, free_ptrarray);

	if (srv->config.code_action_enable && !EMPTY(srv->config.command_on_save_regex))
		lsp_command_send_code_action_request(doc, sci_get_current_position(doc->editor->sci),
			on_code_actions_received, doc);
	else if (srv->config.document_formatting_enable && srv->config.format_on_save)
		lsp_format_perform(doc, TRUE, (LspCallback)on_save_finish, doc);
}


static void on_document_before_save_as(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	lsp_sync_text_document_did_close(srv, doc);
}


static void on_document_filetype_set(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	GeanyFiletype *filetype_old, G_GNUC_UNUSED gpointer user_data)
{
	LspServer *srv_old;
	LspServer *srv_new;

	// called also when opening documents - without this it would start servers
	// unnecessarily
	if (!session_loaded)
		return;

	srv_old = lsp_server_get_for_ft(filetype_old);
	srv_new = lsp_server_get(doc);

	if (srv_old == srv_new)
		return;

	if (srv_old)
	{
		// only uses URI/path so no problem we are using the "new" doc here
		lsp_diagnostics_clear(srv_old, doc);
		lsp_semtokens_clear(doc);
		lsp_sync_text_document_did_close(srv_old, doc);
	}

	// might not succeed because lsp_server_get() just launched new server but should
	// be opened once the new server starts
	on_document_visible(doc);
}


static void on_document_reload(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	// do nothing - reload behaves like a normal edit where the original text is
	// removed from Scintilla and the new one inserted
}


static void on_document_activate(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	on_document_visible(doc);
}


static gboolean on_editor_notify(G_GNUC_UNUSED GObject *obj, GeanyEditor *editor, SCNotification *nt,
	G_GNUC_UNUSED gpointer user_data)
{
	static gboolean perform_highlight = TRUE;  // static!
	GeanyDocument *doc = editor->document;
	ScintillaObject *sci = editor->sci;

	if (nt->nmhdr.code == SCN_PAINTED)  // e.g. caret blinking
		return FALSE;

	if (nt->nmhdr.code == SCN_AUTOCSELECTION &&
		plugin_extension_autocomplete_provided(doc, &extension))
	{
		LspServer *srv = lsp_server_get_if_running(doc);

		if (!srv || !srv->config.autocomplete_enable)
			return FALSE;

		// ignore cursor position change as a result of autocomplete (for highlighting)
		perform_highlight = FALSE;

		sci_start_undo_action(editor->sci);
		lsp_autocomplete_item_selected(srv, doc, SSM(sci, SCI_AUTOCGETCURRENT, 0, 0));
		sci_end_undo_action(editor->sci);

		sci_send_command(sci, SCI_AUTOCCANCEL);

		lsp_autocomplete_set_displayed_symbols(NULL);
		return FALSE;
	}
	else if (nt->nmhdr.code == SCN_AUTOCCANCELLED)
	{
		lsp_autocomplete_set_displayed_symbols(NULL);
		lsp_autocomplete_discard_pending_requests();
		lsp_autocomplete_clear_statusbar();
		return FALSE;
	}
	else if (nt->nmhdr.code == SCN_AUTOCSELECTIONCHANGE &&
		plugin_extension_autocomplete_provided(doc, &extension))
	{
		lsp_autocomplete_selection_changed(doc, nt->text);
	}
	else if (nt->nmhdr.code == SCN_CALLTIPCLICK &&
		plugin_extension_calltips_provided(doc, &extension))
	{
		LspServer *srv = lsp_server_get_if_running(doc);

		if (!srv)
			return FALSE;

		if (srv->config.signature_enable)
		{
			if (nt->position == 1)  /* up arrow */
				lsp_signature_show_prev();
			if (nt->position == 2)  /* down arrow */
				lsp_signature_show_next();
		}
	}

	if (nt->nmhdr.code == SCN_DWELLSTART)
	{
		LspServer *srv = lsp_server_get_if_running(doc);
		if (!srv)
			return FALSE;

		// also delivered when other window has focus
		if (!gtk_widget_has_focus(GTK_WIDGET(sci)))
			return FALSE;

		// the event is also delivered for the margin with numbers where position
		// is -1. In addition, at the top of Scintilla window, the event is delivered
		// when mouse is at the menubar place, with y = 0
		if (nt->position < 0 || nt->y == 0)
			return FALSE;

		if (lsp_signature_showing_calltip(doc))
			;  /* don't cancel signature calltips by accidental hovers */
		else if (srv->config.diagnostics_enable && lsp_diagnostics_has_diag(nt->position))
			lsp_diagnostics_show_calltip(nt->position);
		else if (srv->config.hover_enable)
			lsp_hover_send_request(srv, doc, nt->position);

		return FALSE;
	}
	else if (nt->nmhdr.code == SCN_DWELLEND)
	{
		LspServer *srv = lsp_server_get_if_running(doc);
		if (!srv)
			return FALSE;

		if (srv->config.diagnostics_enable)
			lsp_diagnostics_hide_calltip(doc);
		if (srv->config.hover_enable)
			lsp_hover_hide_calltip(doc);

		return FALSE;
	}
	else if (nt->nmhdr.code == SCN_MODIFIED)
	{
		LspServer *srv;

		// lots of SCN_MODIFIED notifications, filter-out those we are not interested in
		if (!(nt->modificationType & (SC_MOD_BEFOREINSERT | SC_MOD_INSERTTEXT | SC_MOD_BEFOREDELETE | SC_MOD_DELETETEXT)))
			return FALSE;

		srv = lsp_server_get(doc);

		if (!srv || !doc->real_path)
			return FALSE;

		if (nt->modificationType & (SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE))
		{
			perform_highlight = FALSE;
			lsp_highlight_clear(doc);
		}

		// BEFORE insert, BEFORE delete - send the original document
		if (!lsp_sync_is_document_open(srv, doc) &&
			nt->modificationType & (SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE))
		{
			// might happen when the server just started and no interaction with it was
			// possible before
			lsp_sync_text_document_did_open(srv, doc);
		}

		if (nt->modificationType & SC_MOD_INSERTTEXT)  // after insert
		{
			LspPosition pos_start = lsp_utils_scintilla_pos_to_lsp(sci, nt->position);
			LspPosition pos_end = pos_start;
			gchar *text;

			if (srv->use_incremental_sync)
			{
				text = g_malloc(nt->length + 1);
				memcpy(text, nt->text, nt->length);
				text[nt->length] = '\0';
			}
			else
				text = sci_get_contents(doc->editor->sci, -1);

			lsp_sync_text_document_did_change(srv, doc, pos_start, pos_end, text);

			g_free(text);
		}
		else if (srv->use_incremental_sync &&(nt->modificationType & SC_MOD_BEFOREDELETE))
		{
			// BEFORE! delete for incremental sync
			LspPosition pos_start = lsp_utils_scintilla_pos_to_lsp(sci, nt->position);
			LspPosition pos_end = lsp_utils_scintilla_pos_to_lsp(sci, nt->position + nt->length);
			gchar *text = g_strdup("");

			lsp_sync_text_document_did_change(srv, doc, pos_start, pos_end, text);
			g_free(text);
		}
		else if (!srv->use_incremental_sync &&(nt->modificationType & SC_MOD_DELETETEXT))
		{
			// AFTER! delete for full document sync
			LspPosition dummy_start = lsp_utils_scintilla_pos_to_lsp(sci, 0);
			LspPosition dummy_end = lsp_utils_scintilla_pos_to_lsp(sci, 0);
			gchar *text = sci_get_contents(doc->editor->sci, -1);

			lsp_sync_text_document_did_change(srv, doc, dummy_start, dummy_end, text);
			g_free(text);
		}

		if (nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
		{
			guint update_source = GPOINTER_TO_UINT(plugin_get_document_data(geany_plugin, doc, UPDATE_SOURCE_DOC_DATA));

			if (update_source != 0)
				g_source_remove(update_source);

			// perform expensive queries only after some minimum delay
			update_source = plugin_timeout_add(geany_plugin, 300, on_update_idle, doc);
			plugin_set_document_data(geany_plugin, doc, UPDATE_SOURCE_DOC_DATA, GUINT_TO_POINTER(update_source));
		}
	}
	else if (nt->nmhdr.code == SCN_UPDATEUI)
	{
		LspServer *srv = lsp_server_get_if_running(doc);

		if (!srv)
			return FALSE;

		if (nt->updated & (SC_UPDATE_H_SCROLL | SC_UPDATE_V_SCROLL | SC_UPDATE_SELECTION /* when caret moves */))
		{
			lsp_signature_hide_calltip(doc);
			lsp_hover_hide_calltip(doc);
			lsp_diagnostics_hide_calltip(doc);

			SSM(sci, SCI_AUTOCCANCEL, 0, 0);

			if ((nt->updated & SC_UPDATE_SELECTION) && !sci_has_selection(sci))
				lsp_selection_clear_selections();
		}

		if (srv->config.highlighting_enable && perform_highlight &&
			(nt->updated & SC_UPDATE_SELECTION))
		{
			lsp_highlight_schedule_request(doc);
		}
		perform_highlight = TRUE;
	}
	else if (nt->nmhdr.code == SCN_CHARADDED)
	{
		// don't hightlight while typing
		lsp_highlight_clear(doc);
		lsp_hover_hide_calltip(doc);
		lsp_diagnostics_hide_calltip(doc);
	}

	return FALSE;
}


static void on_project_open(G_GNUC_UNUSED GObject *obj, GKeyFile *kf,
	G_GNUC_UNUSED gpointer user_data)
{
	gboolean have_project_config;

	project_configuration = utils_get_setting_boolean(kf, "lsp", "enabled", UnconfiguredConfiguration);
	project_configuration_type = utils_get_setting_integer(kf, "lsp", "settings_type", UserConfigurationType);
	project_configuration_file = g_key_file_get_string(kf, "lsp", "config_file", NULL);

	have_project_config = lsp_utils_get_project_config_filename() != NULL;
	gtk_widget_set_sensitive(menu_items.project_config, have_project_config);
	gtk_widget_set_sensitive(menu_items.user_config, !have_project_config);

	stop_and_init_all_servers();
}


static void on_project_close(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer user_data)
{
	project_configuration = UnconfiguredConfiguration;
	project_configuration_type = UserConfigurationType;
	g_free(project_configuration_file);
	project_configuration_file = NULL;

	gtk_widget_set_sensitive(menu_items.project_config, FALSE);
	gtk_widget_set_sensitive(menu_items.user_config, TRUE);

	stop_and_init_all_servers();
}


static void on_project_dialog_confirmed(G_GNUC_UNUSED GObject *obj, GtkWidget *notebook,
	G_GNUC_UNUSED gpointer user_data)
{
	const gchar *config_file;
	gboolean have_project_config;

	project_configuration = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(project_dialog.enable_check_button));
	project_configuration_type = gtk_combo_box_get_active(GTK_COMBO_BOX(project_dialog.settings_type_combo));
	config_file = gtk_entry_get_text(GTK_ENTRY(project_dialog.config_file_entry));
	SETPTR(project_configuration_file, g_strdup(config_file));

	have_project_config = lsp_utils_get_project_config_filename() != NULL;
	gtk_widget_set_sensitive(menu_items.project_config, have_project_config);
	gtk_widget_set_sensitive(menu_items.user_config, !have_project_config);

	restart_all_servers();
}


static void update_sensitivity(gboolean checkbox_enabled, LspProjectConfigurationType combo_state)
{
	gtk_widget_set_sensitive(project_dialog.settings_type_combo, checkbox_enabled);
	gtk_widget_set_sensitive(project_dialog.path_box, checkbox_enabled && combo_state == ProjectConfigurationType);
}


static void on_config_changed(void)
{
	LspProjectConfigurationType combo_state = gtk_combo_box_get_active(GTK_COMBO_BOX(project_dialog.settings_type_combo));
	gboolean checkbox_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(project_dialog.enable_check_button));

	update_sensitivity(checkbox_enabled, combo_state);
}


static void add_project_properties_tab(GtkWidget *notebook)
{
	LspServerConfig *all_cfg = lsp_server_get_all_section_config();
	GtkWidget *vbox, *hbox, *ebox, *table_box;
	GtkWidget *label;
	GtkSizeGroup *size_group;
	gint combo_value;
	gboolean project_enabled;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	project_dialog.enable_check_button = gtk_check_button_new_with_label(_("Enable LSP client for project"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), project_dialog.enable_check_button, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 12);

	table_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_set_spacing(GTK_BOX(table_box), 6);

	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	label = gtk_label_new(_("Configuration type:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);

	project_dialog.settings_type_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(project_dialog.settings_type_combo), _("Use user configuration file"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(project_dialog.settings_type_combo), _("Use project configuration file"));

	if (project_configuration == UnconfiguredConfiguration)
	{
		project_enabled = all_cfg->enable_by_default;
		combo_value = UserConfigurationType;
	}
	else
	{
		project_enabled = project_configuration;
		combo_value = project_configuration_type;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(project_dialog.enable_check_button), project_enabled);
	gtk_combo_box_set_active(GTK_COMBO_BOX(project_dialog.settings_type_combo), combo_value);
	g_signal_connect(project_dialog.settings_type_combo, "changed", on_config_changed, NULL);
	g_signal_connect(project_dialog.enable_check_button, "toggled", on_config_changed, NULL);

	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), project_dialog.settings_type_combo, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	label = gtk_label_new(_("Configuration file:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);

	project_dialog.config_file_entry = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(project_dialog.config_file_entry));
	project_dialog.path_box = ui_path_box_new(_("Choose LSP Configuration File"),
		GTK_FILE_CHOOSER_ACTION_OPEN, GTK_ENTRY(project_dialog.config_file_entry));
	gtk_entry_set_text(GTK_ENTRY(project_dialog.config_file_entry),
		project_configuration_file ? project_configuration_file : "");

	update_sensitivity(project_enabled, combo_value);

	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), project_dialog.path_box, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), table_box, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

	label = gtk_label_new(_("LSP Client"));

	project_dialog.properties_tab = vbox;
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
}


static void on_project_dialog_open(G_GNUC_UNUSED GObject * obj, GtkWidget * notebook,
		G_GNUC_UNUSED gpointer user_data)
{
	if (!project_dialog.properties_tab)
		add_project_properties_tab(notebook);
}


static void on_project_dialog_close(G_GNUC_UNUSED GObject * obj, GtkWidget * notebook,
		G_GNUC_UNUSED gpointer user_data)
{
	if (project_dialog.properties_tab)
	{
		gtk_widget_destroy(project_dialog.properties_tab);
		project_dialog.properties_tab = NULL;
	}
}


static void on_project_save(G_GNUC_UNUSED GObject *obj, GKeyFile *kf,
		G_GNUC_UNUSED gpointer user_data)
{
	if (project_configuration != UnconfiguredConfiguration)
	{
		g_key_file_set_boolean(kf, "lsp", "enabled", project_configuration);
		g_key_file_set_integer(kf, "lsp", "settings_type", project_configuration_type);
		g_key_file_set_string(kf, "lsp", "config_file",
			project_configuration_file ? project_configuration_file : "");
	}
}


static void code_action_cb(GtkWidget *widget, gpointer user_data)
{
	LspCommand *cmd = user_data;
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get_if_running(doc);

	lsp_command_perform(srv, cmd, NULL, NULL);
}


static void remove_item(GtkWidget *widget, gpointer data)
{
	gtk_container_remove(GTK_CONTAINER(data), widget);
}


static gboolean update_command_menu_items(GPtrArray *code_action_commands, gpointer user_data)
{
	static GPtrArray *action_commands = NULL;  // static!
	GeanyDocument *doc = user_data;
	GtkWidget *menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(context_menu_items.command_item));
	GPtrArray *code_lens_commands = lsp_code_lens_get_commands();
	gboolean command_added = FALSE;
	LspCommand *cmd;
	guint i;

	gtk_container_foreach(GTK_CONTAINER(menu), remove_item, menu);

	if (action_commands)
		g_ptr_array_free(action_commands, TRUE);
	action_commands = code_action_commands;

	foreach_ptr_array(cmd, i, code_action_commands)
	{
		GtkWidget *item = gtk_menu_item_new_with_label(cmd->title);

		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(code_action_cb), code_action_commands->pdata[i]);
		command_added = TRUE;
	}

	foreach_ptr_array(cmd, i, code_lens_commands)
	{
		guint line = sci_get_line_from_position(doc->editor->sci, last_click_pos);

		if (cmd->line == line)
		{
			GtkWidget *item = gtk_menu_item_new_with_label(cmd->title);

			gtk_container_add(GTK_CONTAINER(menu), item);
			g_signal_connect(item, "activate", G_CALLBACK(code_action_cb), code_lens_commands->pdata[i]);
			command_added = TRUE;
		}
	}

	gtk_widget_show_all(context_menu_items.command_item);
	gtk_widget_set_sensitive(context_menu_items.command_item, command_added);

	// code_action_commands are not freed now but preserved until the next call
	return FALSE;
}


// stolen from Geany
static void show_menu_at_caret(GtkMenu* menu, ScintillaObject *sci)
{
	GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(sci));
	gint pos = sci_get_current_position(sci);
	gint line = sci_get_line_from_position(sci, pos);
	gint line_height = SSM(sci, SCI_TEXTHEIGHT, line, 0);
	gint x = SSM(sci, SCI_POINTXFROMPOSITION, 0, pos);
	gint y = SSM(sci, SCI_POINTYFROMPOSITION, 0, pos);
	gint pos_next = SSM(sci, SCI_POSITIONAFTER, pos, 0);
	gint char_width = 0;
	/* if next pos is on the same Y (same line and not after wrapping), diff the X */
	if (pos_next > pos && SSM(sci, SCI_POINTYFROMPOSITION, 0, pos_next) == y)
		char_width = SSM(sci, SCI_POINTXFROMPOSITION, 0, pos_next) - x;
	GdkRectangle rect = {x, y, char_width, line_height};
	gtk_menu_popup_at_rect(GTK_MENU(menu), window, &rect, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}


static gboolean show_code_action_popup(GPtrArray *code_action_commands, gpointer user_data)
{
	GPtrArray *code_lens_commands = lsp_code_lens_get_commands();

	if (code_action_commands->len > 0 || code_lens_commands->len > 0)
	{
		GeanyDocument *doc = user_data;
		GtkWidget *menu;

		update_command_menu_items(code_action_commands, user_data);
		menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(context_menu_items.command_item));
		show_menu_at_caret(GTK_MENU(menu), doc->editor->sci);
		gtk_menu_shell_select_first(GTK_MENU_SHELL(menu), FALSE);
	}
	return FALSE;
}


static gboolean on_update_editor_menu(G_GNUC_UNUSED GObject *obj,
	const gchar *word, gint pos, GeanyDocument *doc, gpointer user_data)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	gboolean goto_definition_enable = srv && srv->config.goto_definition_enable;
	gboolean goto_references_enable = srv && srv->config.goto_references_enable;
	gboolean code_action_enable = srv && srv->config.code_action_enable;
	gboolean document_formatting_enable = srv && srv->config.document_formatting_enable;
	gboolean range_formatting_enable = srv && srv->config.range_formatting_enable;
	gboolean rename_enable = srv && srv->config.rename_enable;
	gboolean highlighting_enable = srv && srv->config.highlighting_enable;

	gtk_widget_set_sensitive(context_menu_items.goto_ref, goto_references_enable);
	gtk_widget_set_sensitive(context_menu_items.goto_def, goto_definition_enable);

	gtk_widget_set_sensitive(context_menu_items.rename_in_file, highlighting_enable);
	gtk_widget_set_sensitive(context_menu_items.rename_in_project, rename_enable);
	gtk_widget_set_sensitive(context_menu_items.format_code, document_formatting_enable || range_formatting_enable);

	gtk_widget_set_sensitive(context_menu_items.command_item, code_action_enable);

	last_click_pos = pos;

	if (code_action_enable)
		lsp_command_send_code_action_request(doc, pos, update_command_menu_items, doc);

	return FALSE;
}


static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	ScintillaObject *sci = doc ? doc->editor->sci : NULL;

	if (!sci || gtk_window_get_focus(GTK_WINDOW(geany->main_widgets->window)) != GTK_WIDGET(sci))
		return FALSE;

	switch (event->keyval)
	{
		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
		case GDK_KEY_uparrow:
		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
		case GDK_KEY_downarrow:
		case GDK_KEY_Page_Up:
		case GDK_KEY_KP_Page_Up:
		case GDK_KEY_Page_Down:
		case GDK_KEY_KP_Page_Down:
			if (SSM(sci, SCI_GETSELECTIONS, 0, 0) > 1 && !SSM(sci, SCI_AUTOCACTIVE, 0, 0))
				SSM(sci, SCI_CANCEL, 0, 0);  // drop multiple carets
			break;
		default:
			break;
	}

	return FALSE;
}


static void terminate_all(void)
{
	plugin_extension_unregister(&extension);
	lsp_server_set_initialized_cb(NULL);

	lsp_server_stop_all(TRUE);
}


static void on_geany_before_quit(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer user_data)
{
	geany_quitting = TRUE;

	terminate_all();  // blocks until all servers are stopped
}


PluginCallback plugin_callbacks[] = {
	{"document-new", (GCallback) &on_document_new, FALSE, NULL},
	{"document-close", (GCallback) &on_document_close, FALSE, NULL},
	{"document-reload", (GCallback) &on_document_reload, FALSE, NULL},
	{"document-activate", (GCallback) &on_document_activate, FALSE, NULL},
	{"document-save", (GCallback) &on_document_save, FALSE, NULL},
	{"document-before-save", (GCallback) &on_document_before_save, FALSE, NULL},
	{"document-before-save-as", (GCallback) &on_document_before_save_as, TRUE, NULL},
	{"document-filetype-set", (GCallback) &on_document_filetype_set, FALSE, NULL},
	{"editor-notify", (GCallback) &on_editor_notify, FALSE, NULL},
	{"update-editor-menu", (GCallback) &on_update_editor_menu, FALSE, NULL},
	{"project-open", (GCallback) &on_project_open, FALSE, NULL},
	{"project-close", (GCallback) &on_project_close, FALSE, NULL},
	{"project-save", (GCallback) &on_project_save, FALSE, NULL},
	{"project-dialog-open", (GCallback) &on_project_dialog_open, FALSE, NULL},
	{"project-dialog-confirmed", (GCallback) &on_project_dialog_confirmed, FALSE, NULL},
	{"project-dialog-close", (GCallback) &on_project_dialog_close, FALSE, NULL},
	{"key-press", (GCallback) &on_key_press, FALSE, NULL},
	{"geany-before-quit", (GCallback) &on_geany_before_quit, FALSE, NULL},
	{NULL, NULL, FALSE, NULL}
};


static void on_open_project_config(void)
{
	gchar *utf8_filename = utils_get_utf8_from_locale(lsp_utils_get_project_config_filename());
	if (utf8_filename)
		document_open_file(utf8_filename, FALSE, NULL, NULL);
	g_free(utf8_filename);
}


static void on_open_user_config(void)
{
	gchar *utf8_filename = utils_get_utf8_from_locale(lsp_utils_get_user_config_filename());
	document_open_file(utf8_filename, FALSE, NULL, NULL);
	g_free(utf8_filename);
}


static void on_open_global_config(void)
{
	gchar *utf8_filename = utils_get_utf8_from_locale(lsp_utils_get_global_config_filename());
	document_open_file(utf8_filename, TRUE, NULL, NULL);
	g_free(utf8_filename);
}


static void on_show_initialize_responses(void)
{
	gchar *resps = lsp_server_get_initialize_responses();
	document_new_file(NULL, filetypes_lookup_by_name("JSON"), resps);
	g_free(resps);
}


static void show_hover_popup(void)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (srv)
		lsp_hover_send_request(srv, doc, sci_get_current_position(doc->editor->sci));
}


static void on_rename_done(void)
{
	GeanyDocument *doc = document_get_current();

	if (!doc)
		return;

	if (doc->file_type->id == GEANY_FILETYPES_C || doc->file_type->id == GEANY_FILETYPES_CPP)
	{
		// workaround strange behavior of clangd: it doesn't seem to reflect changes
		// in non-open files unless all files are saved and the server is restarted
		lsp_utils_save_all_docs();
		restart_all_servers();
	}
}


static gboolean on_code_actions_received_kb(GPtrArray *code_action_commands, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get_if_running(doc);

	if (srv)
	{
		GPtrArray *code_lens_commands = lsp_code_lens_get_commands();
		gint cmd_id = GPOINTER_TO_INT(user_data);
		gchar *cmd_str = srv->config.command_regexes->pdata[cmd_id];
		gint line = sci_get_current_line(doc->editor->sci);
		LspCommand *cmd;
		guint i;

		foreach_ptr_array(cmd, i, code_action_commands)
		{
			if (g_regex_match_simple(cmd_str, cmd->title, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
			{
				lsp_command_perform(srv, cmd, NULL, NULL);
				// perform only the first matching command
				return TRUE;
			}
		}

		foreach_ptr_array(cmd, i, code_lens_commands)
		{
			if (cmd->line == line &&
				g_regex_match_simple(cmd_str, cmd->title, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
			{
				lsp_command_perform(srv, cmd, NULL, NULL);
				// perform only the first matching command
				return TRUE;
			}
		}
	}

	return TRUE;
}


static void invoke_command_kb(guint key_id, gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	if (key_id >= KB_COUNT + srv->config.command_keybinding_num)
		return;

	lsp_command_send_code_action_request(doc, pos, on_code_actions_received_kb, GINT_TO_POINTER(key_id - KB_COUNT));
}


static void invoke_kb(guint key_id, gint pos)
{
	GeanyDocument *doc = document_get_current();

	if (pos < 0)
		pos = doc ? sci_get_current_position(doc->editor->sci) : 0;

	if (key_id >= KB_COUNT)
	{
		invoke_command_kb(key_id , pos);
		return;
	}

	switch (key_id)
	{
		case KB_GOTO_DEFINITION:
			lsp_goto_definition(pos);
			break;
		case KB_GOTO_DECLARATION:
			lsp_goto_declaration(pos);
			break;
		case KB_GOTO_TYPE_DEFINITION:
			lsp_goto_type_definition(pos);
			break;

		case KB_GOTO_ANYWHERE:
			lsp_goto_anywhere_for_file();
			break;
		case KB_GOTO_DOC_SYMBOL:
			lsp_goto_anywhere_for_doc();
			break;
		case KB_GOTO_WORKSPACE_SYMBOL:
			lsp_goto_anywhere_for_workspace();
			break;
		case KB_GOTO_LINE:
			lsp_goto_anywhere_for_line();
			break;

		case KB_GOTO_NEXT_DIAG:
			lsp_diagnostics_goto_next_diag(pos);
			break;
		case KB_GOTO_PREV_DIAG:
			lsp_diagnostics_goto_prev_diag(pos);
			break;
		case KB_SHOW_DIAG:
			lsp_diagnostics_show_calltip(pos);
			break;
		case KB_SHOW_FILE_DIAGS:
			lsp_diagnostics_show_all(TRUE);
			break;
		case KB_SHOW_ALL_DIAGS:
			lsp_diagnostics_show_all(FALSE);
			break;

		case KB_FIND_REFERENCES:
			lsp_goto_references(pos);
			break;
		case KB_FIND_IMPLEMENTATIONS:
			lsp_goto_implementations(pos);
			break;
		case KB_HIGHLIGHT_OCCUR:
			lsp_highlight_schedule_request(doc);
			break;
		case KB_HIGHLIGHT_CLEAR:
			lsp_highlight_clear(doc);
			break;

		case KB_EXPAND_SELECTION:
			lsp_selection_range_expand();
			break;
		case KB_SHRINK_SELECTION:
			lsp_selection_range_shrink();
			break;

		case KB_SHOW_HOVER_POPUP:
			show_hover_popup();
			break;

		case KB_SWAP_HEADER_SOURCE:
			lsp_extension_clangd_switch_source_header();
			break;

		case KB_RENAME_IN_FILE:
			lsp_highlight_rename(pos);
			break;

		case KB_RENAME_IN_PROJECT:
			lsp_rename_send_request(pos, on_rename_done);
			break;

		case KB_FORMAT_CODE:
			lsp_format_perform(doc, FALSE, NULL, NULL);
			break;

		case KB_RESTART_SERVERS:
			restart_all_servers();
			break;

		case KB_SHOW_CODE_ACTIONS:
			lsp_command_send_code_action_request(doc, pos, show_code_action_popup, doc);
			break;

		default:
			break;
	}
}


static gboolean on_kb_invoked(guint key_id)
{
	invoke_kb(key_id, -1);
	return TRUE;
}


static void on_menu_invoked(GtkWidget *widget, gpointer user_data)
{
	guint key_id = GPOINTER_TO_UINT(user_data);
	invoke_kb(key_id, -1);
}


static void on_context_menu_invoked(GtkWidget *widget, gpointer user_data)
{
	guint key_id = GPOINTER_TO_UINT(user_data);
	invoke_kb(key_id, last_click_pos);
}


static void create_menu_items()
{
	LspServerConfig *all_cfg = lsp_server_get_all_section_config();
	GtkWidget *menu, *item, *command_submenu;
	GeanyKeyGroup *group;
	gint i;

	group = plugin_set_key_group(geany_plugin, "lsp", KB_COUNT + all_cfg->command_keybinding_num, on_kb_invoked);

	menu_items.parent_item = gtk_menu_item_new_with_mnemonic(_("_LSP Client"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_items.parent_item);

	menu = gtk_menu_new ();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items.parent_item), menu);

	menu_items.goto_def = gtk_menu_item_new_with_mnemonic(_("Go to _Definition"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.goto_def);
	g_signal_connect(menu_items.goto_def, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_DEFINITION));
	keybindings_set_item(group, KB_GOTO_DEFINITION, NULL, 0, 0, "goto_definition",
		_("Go to definition"), menu_items.goto_def);

	menu_items.goto_decl = gtk_menu_item_new_with_mnemonic(_("Go to D_eclaration"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.goto_decl);
	g_signal_connect(menu_items.goto_decl, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_DECLARATION));
	keybindings_set_item(group, KB_GOTO_DECLARATION, NULL, 0, 0, "goto_declaration",
		_("Go to declaration"), menu_items.goto_decl);

	menu_items.goto_type_def = gtk_menu_item_new_with_mnemonic(_("Go to _Type Definition"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.goto_type_def);
	g_signal_connect(menu_items.goto_type_def, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_TYPE_DEFINITION));
	keybindings_set_item(group, KB_GOTO_TYPE_DEFINITION, NULL, 0, 0, "goto_type_definition",
		_("Go to type definition"), menu_items.goto_type_def);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	item = gtk_menu_item_new_with_mnemonic(_("Go to _Anywhere..."));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_ANYWHERE));
	keybindings_set_item(group, KB_GOTO_ANYWHERE, NULL, 0, 0, "goto_anywhere",
		_("Go to anywhere"), item);

	item = gtk_menu_item_new_with_mnemonic(_("Go to _Document Symbol..."));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_DOC_SYMBOL));
	keybindings_set_item(group, KB_GOTO_DOC_SYMBOL, NULL, 0, 0, "goto_doc_symbol",
		_("Go to document symbol"), item);

	item = gtk_menu_item_new_with_mnemonic(_("Go to _Workspace Symbol..."));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_WORKSPACE_SYMBOL));
	keybindings_set_item(group, KB_GOTO_WORKSPACE_SYMBOL, NULL, 0, 0, "goto_workspace_symbol",
		_("Go to workspace symbol"), item);

	item = gtk_menu_item_new_with_mnemonic(_("Go to _Line..."));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_LINE));
	keybindings_set_item(group, KB_GOTO_LINE, NULL, 0, 0, "goto_line",
		_("Go to line"), item);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	menu_items.goto_next_diag = gtk_menu_item_new_with_mnemonic(_("Go to _Next Diagnostic"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.goto_next_diag);
	g_signal_connect(menu_items.goto_next_diag, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_NEXT_DIAG));
	keybindings_set_item(group, KB_GOTO_NEXT_DIAG, NULL, 0, 0, "goto_next_diag",
		_("Go to next diagnostic"), menu_items.goto_next_diag);

	menu_items.goto_prev_diag = gtk_menu_item_new_with_mnemonic(_("Go to _Previous Diagnostic"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.goto_prev_diag);
	g_signal_connect(menu_items.goto_prev_diag, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_PREV_DIAG));
	keybindings_set_item(group, KB_GOTO_PREV_DIAG, NULL, 0, 0, "goto_prev_diag",
		_("Go to previous diagnostic"), menu_items.goto_prev_diag);

	menu_items.show_diag = gtk_menu_item_new_with_mnemonic(_("_Show Current Diagnostic"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.show_diag);
	g_signal_connect(menu_items.show_diag, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_SHOW_DIAG));
	keybindings_set_item(group, KB_SHOW_DIAG, NULL, 0, 0, "show_diag",
		_("Show current diagnostic"), menu_items.show_diag);

	menu_items.show_file_diags = gtk_menu_item_new_with_mnemonic(_("Show _Current File Diagnostics"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.show_file_diags);
	g_signal_connect(menu_items.show_file_diags, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_SHOW_FILE_DIAGS));
	keybindings_set_item(group, KB_SHOW_FILE_DIAGS, NULL, 0, 0, "show_file_diags",
		_("Show current file diagnostics"), menu_items.show_file_diags);

	menu_items.show_all_diags = gtk_menu_item_new_with_mnemonic(_("Show _All Diagnostics"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.show_all_diags);
	g_signal_connect(menu_items.show_all_diags, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_SHOW_ALL_DIAGS));
	keybindings_set_item(group, KB_SHOW_ALL_DIAGS, NULL, 0, 0, "show_all_diags",
		_("Show all diagnostics"), menu_items.show_all_diags);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	menu_items.goto_ref = gtk_menu_item_new_with_mnemonic(_("Find _References"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.goto_ref);
	g_signal_connect(menu_items.goto_ref, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_FIND_REFERENCES));
	keybindings_set_item(group, KB_FIND_REFERENCES, NULL, 0, 0, "find_references",
		_("Find references"), menu_items.goto_ref);

	menu_items.goto_impl = gtk_menu_item_new_with_mnemonic(_("Find _Implementations"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.goto_impl);
	g_signal_connect(menu_items.goto_impl, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_FIND_IMPLEMENTATIONS));
	keybindings_set_item(group, KB_FIND_IMPLEMENTATIONS, NULL, 0, 0, "find_implementations",
		_("Find implementations"), menu_items.goto_impl);

	menu_items.highlight_occur = gtk_menu_item_new_with_mnemonic(_("_Highlight Symbol Occurrences"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.highlight_occur);
	g_signal_connect(menu_items.highlight_occur, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_HIGHLIGHT_OCCUR));
	keybindings_set_item(group, KB_HIGHLIGHT_OCCUR, NULL, 0, 0, "highlight_occurrences",
		_("Highlight symbol occurrences"), menu_items.highlight_occur);

	menu_items.highlight_clear = gtk_menu_item_new_with_mnemonic(_("_Clear Highlighted"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.highlight_clear);
	g_signal_connect(menu_items.highlight_clear, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_HIGHLIGHT_CLEAR));
	keybindings_set_item(group, KB_HIGHLIGHT_CLEAR, NULL, 0, 0, "highlight_clear",
		_("Clear highlighted"), menu_items.highlight_clear);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	menu_items.rename_in_file = gtk_menu_item_new_with_mnemonic(_("_Rename Highlighted"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.rename_in_file);
	g_signal_connect(menu_items.rename_in_file, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_RENAME_IN_FILE));
	keybindings_set_item(group, KB_RENAME_IN_FILE, NULL, 0, 0, "rename_in_file",
		_("Rename highlighted"), menu_items.rename_in_file);

	menu_items.rename_in_project = gtk_menu_item_new_with_mnemonic(_("Rename in _Project..."));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.rename_in_project);
	g_signal_connect(menu_items.rename_in_project, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_RENAME_IN_PROJECT));
	keybindings_set_item(group, KB_RENAME_IN_PROJECT, NULL, 0, 0, "rename_in_project",
		_("Rename in project"), menu_items.rename_in_project);

	menu_items.format_code = gtk_menu_item_new_with_mnemonic(_("_Format Code"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.format_code);
	g_signal_connect(menu_items.format_code, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_FORMAT_CODE));
	keybindings_set_item(group, KB_FORMAT_CODE, NULL, 0, 0, "format_code",
		_("Format code"), menu_items.format_code);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	menu_items.hover_popup = gtk_menu_item_new_with_mnemonic(_("Show _Hover Popup"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.hover_popup);
	g_signal_connect(menu_items.hover_popup, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_SHOW_HOVER_POPUP));
	keybindings_set_item(group, KB_SHOW_HOVER_POPUP, NULL, 0, 0, "show_hover_popup",
		_("Show hover popup"), menu_items.hover_popup);

	menu_items.code_action_popup = gtk_menu_item_new_with_mnemonic(_("Show Code Action Popup"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.code_action_popup);
	g_signal_connect(menu_items.code_action_popup, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_SHOW_CODE_ACTIONS));
	keybindings_set_item(group, KB_SHOW_CODE_ACTIONS, NULL, 0, 0, "show_code_action_popup",
		_("Show code action popup"), menu_items.code_action_popup);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	menu_items.expand_selection = gtk_menu_item_new_with_mnemonic(_("Expand Selection"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.expand_selection);
	g_signal_connect(menu_items.expand_selection, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_EXPAND_SELECTION));
	keybindings_set_item(group, KB_EXPAND_SELECTION, NULL, 0, 0, "expand_selection",
		_("Expand Selection"), menu_items.expand_selection);

	menu_items.shrink_selection = gtk_menu_item_new_with_mnemonic(_("Shrink Selection"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.shrink_selection);
	g_signal_connect(menu_items.shrink_selection, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_SHRINK_SELECTION));
	keybindings_set_item(group, KB_SHRINK_SELECTION, NULL, 0, 0, "shrink_selection",
		_("Shrink Selection"), menu_items.shrink_selection);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	menu_items.header_source = gtk_menu_item_new_with_mnemonic(_("Swap Header/Source"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.header_source);
	g_signal_connect(menu_items.header_source, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_SWAP_HEADER_SOURCE));
	keybindings_set_item(group, KB_SWAP_HEADER_SOURCE, NULL, 0, 0, "swap_header_source",
		_("Swap header/source"), menu_items.header_source);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	menu_items.project_config = gtk_menu_item_new_with_mnemonic(_("_Project Configuration"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.project_config);
	g_signal_connect(menu_items.project_config, "activate", G_CALLBACK(on_open_project_config), NULL);

	menu_items.user_config = gtk_menu_item_new_with_mnemonic(_("_User Configuration"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.user_config);
	g_signal_connect(menu_items.user_config, "activate", G_CALLBACK(on_open_user_config), NULL);

	item = gtk_menu_item_new_with_mnemonic(_("_Global Configuration"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_open_global_config), NULL);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	item = gtk_menu_item_new_with_mnemonic(_("_Server Initialize Responses"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_show_initialize_responses), NULL);

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	item = gtk_menu_item_new_with_mnemonic(_("_Restart All Servers"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_RESTART_SERVERS));
	keybindings_set_item(group, KB_RESTART_SERVERS, NULL, 0, 0, "restart_all_servers",
		_("Restart all servers"), item);

	gtk_widget_show_all(menu_items.parent_item);

	for (i = 0; i < all_cfg->command_keybinding_num; i++)
	{
		gchar *kb_name = g_strdup_printf("lsp_command_%d", i + 1);
		gchar *kb_display_name = g_strdup_printf(_("Command %d"), i + 1);

		keybindings_set_item(group, KB_COUNT + i, NULL, 0, 0, kb_name, kb_display_name, NULL);

		g_free(kb_display_name);
		g_free(kb_name);
	}

	/* context menu */
	context_menu_items.separator1 = gtk_separator_menu_item_new();
	gtk_widget_show(context_menu_items.separator1);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.separator1);

	context_menu_items.command_item = gtk_menu_item_new_with_mnemonic(_("_Commands (LSP)"));
	command_submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(context_menu_items.command_item), command_submenu);
	gtk_widget_show_all(context_menu_items.command_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.command_item);

	context_menu_items.format_code = gtk_menu_item_new_with_mnemonic(_("_Format Code (LSP)"));
	gtk_widget_show(context_menu_items.format_code);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.format_code);
	g_signal_connect(context_menu_items.format_code, "activate", G_CALLBACK(on_context_menu_invoked),
		GUINT_TO_POINTER(KB_FORMAT_CODE));

	context_menu_items.rename_in_project = gtk_menu_item_new_with_mnemonic(_("Rename in _Project (LSP)..."));
	gtk_widget_show(context_menu_items.rename_in_project);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.rename_in_project);
	g_signal_connect(context_menu_items.rename_in_project, "activate", G_CALLBACK(on_context_menu_invoked),
		GUINT_TO_POINTER(KB_RENAME_IN_PROJECT));

	context_menu_items.rename_in_file = gtk_menu_item_new_with_mnemonic(_("_Rename Highlighted (LSP)"));
	gtk_widget_show(context_menu_items.rename_in_file);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.rename_in_file);
	g_signal_connect(context_menu_items.rename_in_file, "activate", G_CALLBACK(on_context_menu_invoked),
		GUINT_TO_POINTER(KB_RENAME_IN_FILE));

	context_menu_items.separator2 = gtk_separator_menu_item_new();
	gtk_widget_show(context_menu_items.separator2);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.separator2);

	context_menu_items.goto_def = gtk_menu_item_new_with_mnemonic(_("Go to _Definition (LSP)"));
	gtk_widget_show(context_menu_items.goto_def);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.goto_def);
	g_signal_connect(context_menu_items.goto_def, "activate", G_CALLBACK(on_context_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_DEFINITION));

	context_menu_items.goto_ref = gtk_menu_item_new_with_mnemonic(_("Find _References (LSP)"));
	gtk_widget_show(context_menu_items.goto_ref);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.goto_ref);
	g_signal_connect(context_menu_items.goto_ref, "activate", G_CALLBACK(on_context_menu_invoked),
		GUINT_TO_POINTER(KB_FIND_REFERENCES));

	update_menu(NULL);
}


static void on_server_initialized(LspServer *srv)
{
	GeanyDocument *current_doc = document_get_current();
	guint i;

	update_menu(current_doc);

	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];
		LspServer *s2 = lsp_server_get_if_running(doc);

		// see on_document_visible() for detailed comment
		if (s2 == srv && (doc->changed || doc == current_doc))
		{
			if (doc == current_doc)
				on_document_visible(doc);
			else  // unsaved open file
				lsp_sync_text_document_did_open(srv, doc);
		}
	}
}


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	GeanyDocument *doc = document_get_current();

	plugin_module_make_resident(geany_plugin);

	lsp_server_set_initialized_cb(on_server_initialized);

	stop_and_init_all_servers();

	plugin_extension_register(&extension, "LSP", 100, NULL);

	create_menu_items();

	if (doc)
		on_document_visible(doc);
}


void plugin_cleanup(void)
{
	if (!geany_quitting)
		terminate_all();  // done in "geany-before-quit" handler when quitting

	gtk_widget_destroy(menu_items.parent_item);
	menu_items.parent_item = NULL;

	gtk_widget_destroy(context_menu_items.goto_def);
	gtk_widget_destroy(context_menu_items.format_code);
	gtk_widget_destroy(context_menu_items.rename_in_file);
	gtk_widget_destroy(context_menu_items.rename_in_project);
	gtk_widget_destroy(context_menu_items.goto_ref);
	gtk_widget_destroy(context_menu_items.command_item);
	gtk_widget_destroy(context_menu_items.separator1);
	gtk_widget_destroy(context_menu_items.separator2);

	lsp_symbol_tree_destroy();
	lsp_diagnostics_common_destroy();
}


void plugin_help(void)
{
	utils_open_browser("https://plugins.geany.org/lsp.html");
}
