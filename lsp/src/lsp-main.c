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

#include <sys/time.h>
#include <string.h>

#include <geanyplugin.h>

#include <jsonrpc-glib.h>


// https://github.com/microsoft/language-server-protocol/blob/main/versions/protocol-1-x.md
// https://github.com/microsoft/language-server-protocol/blob/main/versions/protocol-2-x.md


GeanyPlugin *geany_plugin;
GeanyData *geany_data;

LspProjectConfigurationType project_configuration_type;
gchar *project_configuration_file;

static gint last_click_pos;
static gboolean ignore_selection_change;
static gboolean session_opening;
static GPtrArray *commands;


#ifdef GEANY_LSP_COMBINED_PROJECT
# define PLUGIN_LOCALEDIR GEANY_LOCALEDIR
# define PLUGIN_VERSION "0.1"
#else
# define PLUGIN_LOCALEDIR LOCALEDIR
# define PLUGIN_VERSION VERSION
#endif

PLUGIN_VERSION_CHECK(246)  //TODO
PLUGIN_SET_TRANSLATABLE_INFO(
	PLUGIN_LOCALEDIR,
	GETTEXT_PACKAGE,
	_("LSP Client"),
	_("Language server protocol client for Geany"),
	PLUGIN_VERSION,
	"Jiri Techet <techet@gmail.com>")


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

  KB_FIND_IMPLEMENTATIONS,
  KB_FIND_REFERENCES,

  KB_SHOW_HOVER_POPUP,

  KB_RENAME_IN_FILE,
  KB_RENAME_IN_PROJECT,
  KB_FORMAT_CODE,

  KB_RESTART_SERVERS,

#ifndef HAVE_GEANY_PLUGIN_EXTENSION
  KB_INVOKE_AUTOCOMPLETE,
  KB_SHOW_CALLTIP,
#endif

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

	GtkWidget *goto_ref;
	GtkWidget *goto_impl;

	GtkWidget *rename_in_file;
	GtkWidget *rename_in_project;
	GtkWidget *format_code;

	GtkWidget *hover_popup;
} menu_items;


struct
{
	GtkWidget *command_item;
	GtkWidget *goto_type_def;
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
	GtkWidget *settings_type_combo;
	GtkWidget *config_file_entry;
	GtkWidget *path_box;
	GtkWidget *properties_tab;
} project_dialog;


static gboolean autocomplete_provided(GeanyDocument *doc)
{
	LspServerConfig *cfg = lsp_server_get_config(doc);

	if (!cfg)
		return FALSE;

	return lsp_server_is_usable(doc) && cfg->autocomplete_enable;
}


static void autocomplete_perform(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	lsp_autocomplete_completion(srv, doc);
}


static gboolean calltips_provided(GeanyDocument *doc)
{
	LspServerConfig *cfg = lsp_server_get_config(doc);

	if (!cfg)
		return FALSE;

	return lsp_server_is_usable(doc) && cfg->signature_enable;
}


static void calltips_show(GeanyDocument *doc, gboolean force)
{
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	lsp_signature_send_request(srv, doc, force);
}


static gboolean goto_provided(GeanyDocument *doc)
{
	LspServerConfig *cfg = lsp_server_get_config(doc);

	if (!cfg)
		return FALSE;

	return lsp_server_is_usable(doc) && cfg->goto_enable;
}


static void goto_perform(GeanyDocument *doc, gint pos, gboolean definition)
{
	if (definition)
		lsp_goto_definition(pos);
	else
		lsp_goto_declaration(pos);
}


static gboolean symbol_highlight_provided(GeanyDocument *doc)
{
	LspServerConfig *cfg = lsp_server_get_config(doc);

	if (!cfg)
		return FALSE;

	return lsp_server_is_usable(doc) && cfg->semantic_tokens_enable;
}


#ifdef HAVE_GEANY_PLUGIN_EXTENSION
static gboolean doc_symbols_provided(GeanyDocument *doc)
{
#ifdef HAVE_GEANY_PLUGIN_EXTENSION_DOC_SYMBOLS
	LspServerConfig *cfg = lsp_server_get_config(doc);

	if (!cfg)
		return FALSE;

	return lsp_server_is_usable(doc) && cfg->document_symbols_enable;
#else
	return FALSE;
#endif
}


static GPtrArray *doc_symbols_get(GeanyDocument *doc)
{
#ifdef HAVE_GEANY_PLUGIN_EXTENSION_DOC_SYMBOLS
	static GPtrArray *ret = NULL;  // static!

	GPtrArray *symbols = lsp_symbols_doc_get_cached(doc);
	LspSymbol *sym;
	guint i;

	if (ret)
		g_ptr_array_free(ret, TRUE);
	ret = g_ptr_array_new_full(0, (GDestroyNotify)tm_tag_unref);

	if (symbols)
	{
		foreach_ptr_array(sym, i, symbols)
		{
			TMTag *tag = tm_tag_new();
			tag->plugin_extension = TRUE;
			tag->name = g_strdup(sym->name);
			tag->file_name = g_strdup(sym->file_name);
			tag->scope = g_strdup(sym->scope);
			tag->tooltip = g_strdup(sym->tooltip);
			tag->line = sym->line;
			tag->icon = sym->icon;

			g_ptr_array_add(ret, tag);
		}
	}

	return ret;
#else
	return NULL;
#endif
}


static PluginExtension extension = {
	.autocomplete_provided = autocomplete_provided,
	.autocomplete_perform = autocomplete_perform,

	.calltips_provided = calltips_provided,
	.calltips_show = calltips_show,

	.goto_provided = goto_provided,
	.goto_perform = goto_perform,

	.doc_symbols_provided = doc_symbols_provided,
	.doc_symbols_get = doc_symbols_get,

	.symbol_highlight_provided = symbol_highlight_provided,
};


static void lsp_symbol_request_cb(gpointer user_data)
{
#ifdef HAVE_GEANY_PLUGIN_EXTENSION_DOC_SYMBOLS
	GeanyDocument *doc = user_data;

	if (doc == document_get_current())
		symbols_reload_tag_list();
#endif
}

#else  // without HAVE_GEANY_PLUGIN_EXTENSION

static gboolean on_button_press_event(GtkWidget *widget, GdkEventButton *event,
											 gpointer data)
{
	GeanyDocument *doc = data;

	if (!goto_provided(doc))
		return FALSE;

	if (event->button == 1)
	{
		guint state = keybindings_get_modifiers(event->state);

		if (event->type == GDK_BUTTON_PRESS && state == GEANY_PRIMARY_MOD_MASK)
		{
			gchar *current_word;
			gint click_pos;

			/* it's very unlikely we got a 'real' click even on 0, 0, so assume it is a
			 * fake event to show the editor menu triggered by a key event where we want to use the
			 * text cursor position. */
			if (event->x > 0.0 && event->y > 0.0)
				click_pos = SSM(doc->editor->sci, SCI_POSITIONFROMPOINT, (uptr_t) event->x, event->y);
			else
				click_pos = sci_get_current_position(doc->editor->sci);

			sci_set_current_position(doc->editor->sci, click_pos, FALSE);
			current_word = lsp_utils_get_current_iden(doc, click_pos);

			if (current_word)
			{
				goto_perform(doc, click_pos, TRUE);

				g_free(current_word);
				return TRUE;
			}
		}
	}

	return FALSE;
}
#endif  // HAVE_GEANY_PLUGIN_EXTENSION


static void on_document_new(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	// we don't know the filename yet - nothing for the LSP server
}


static void update_menu(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	gboolean goto_definition_enable = srv && srv->config.goto_definition_enable;
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

	gtk_widget_set_sensitive(menu_items.goto_def, goto_definition_enable);
	gtk_widget_set_sensitive(menu_items.goto_decl, goto_declaration_enable);
	gtk_widget_set_sensitive(menu_items.goto_type_def, goto_type_definition_enable);

	gtk_widget_set_sensitive(menu_items.goto_next_diag, diagnostics_enable);
	gtk_widget_set_sensitive(menu_items.goto_prev_diag, diagnostics_enable);

	gtk_widget_set_sensitive(menu_items.goto_ref, goto_references_enable);
	gtk_widget_set_sensitive(menu_items.goto_impl, goto_implementation_enable);

	gtk_widget_set_sensitive(menu_items.rename_in_file, highlighting_enable);
	gtk_widget_set_sensitive(menu_items.rename_in_project, rename_enable);
	gtk_widget_set_sensitive(menu_items.format_code, document_formatting_enable || range_formatting_enable);

	gtk_widget_set_sensitive(menu_items.hover_popup, hover_popup_enable);
}


static void on_document_visible(GeanyDocument *doc)
{
	LspServer *srv;

	if (session_opening)
		return;

	update_menu(doc);

	if (!doc)
		return;

	srv = lsp_server_get(doc);

	lsp_diagnostics_style_init(doc);
	lsp_diagnostics_redraw(doc);
	lsp_highlight_style_init(doc);
	lsp_semtokens_style_init(doc);
	lsp_code_lens_style_init(doc);

#ifdef HAVE_GEANY_PLUGIN_EXTENSION
	if (doc_symbols_provided(doc))
		lsp_symbols_doc_request(doc, lsp_symbol_request_cb, doc);
#else
	if (lsp_utils_doc_ft_has_tags(doc))
	{
		gchar *ft_lower = g_utf8_strdown(doc->file_type->name, -1);

		dialogs_show_msgbox(GTK_MESSAGE_WARNING, _("Because of conflicting implementations, the LSP plugin requires that symbol generation is disabled for the filetypes for which LSP is enabled.\n\nTo disable it for the current filetype, go to:\n\nTools->Configuration Files->...->filetypes.%s\n\nand under the [settings] section add tag_parser= (with no value after =) which disables the symbol parser. Plugin reload or Geany restart may be required afterwards."), ft_lower);
		g_free(ft_lower);
	}
#endif

	if (symbol_highlight_provided(doc))
		lsp_semtokens_send_request(doc);

	if (!srv)
		return;

	// this might not get called for the first time when server gets started because
	// lsp_server_get() returns NULL. However, we also "open" current and modified
	// documents after successful server handshake inside on_server_initialized()
	if (!lsp_sync_is_document_open(doc))
		lsp_sync_text_document_did_open(srv, doc);
}


static void on_document_open(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	if (!session_opening)
		on_document_visible(doc);

#ifndef HAVE_GEANY_PLUGIN_EXTENSION
	g_signal_connect(doc->editor->sci, "button-press-event", G_CALLBACK(on_button_press_event), doc);
#endif
}


static gboolean on_doc_close_idle(gpointer user_data)
{
	if (!document_get_current() && menu_items.parent_item)
		update_menu(NULL);  // the last open document was closed

	return FALSE;
}


static void on_document_close(G_GNUC_UNUSED GObject * obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	LspServer *srv = lsp_server_get_if_running(doc);

	plugin_idle_add(geany_plugin, on_doc_close_idle, NULL);

	if (!srv)
		return;

	lsp_diagnostics_clear(doc);
	lsp_semtokens_clear(doc);
	lsp_sync_text_document_did_close(srv, doc);
}


static void destroy_all(void)
{
	lsp_diagnostics_destroy();
	lsp_semtokens_destroy();
	lsp_symbols_destroy();
	lsp_command_send_code_action_destroy();
}


static void stop_and_init_all_servers(void)
{
	lsp_server_stop_all(FALSE);
	lsp_server_init_all();

	destroy_all();

	lsp_sync_init();
	lsp_diagnostics_init();
	lsp_command_send_code_action_init();
}


static void restart_all_servers(void)
{
	GeanyDocument *doc = document_get_current();

	stop_and_init_all_servers();

	if (doc)
		lsp_server_get(doc);
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

	if (!lsp_sync_is_document_open(doc))
	{
		// "new" documents without filename saved for the first time or
		// "save as" performed
		lsp_sync_text_document_did_open(srv, doc);
#ifndef HAVE_GEANY_PLUGIN_EXTENSION
		g_signal_connect(doc->editor->sci, "button-press-event", G_CALLBACK(on_button_press_event), doc);
#endif
	}

	lsp_sync_text_document_did_save(srv, doc);
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
	if (!lsp_sync_is_document_open(doc))
		return;

	srv_old = lsp_server_get_for_ft(filetype_old);
	srv_new = lsp_server_get(doc);

	if (srv_old == srv_new)
		return;

	if (srv_old)
	{
		// only uses URI/path so no problem we are using the "new" doc here
		lsp_diagnostics_clear(doc);
		lsp_semtokens_clear(doc);
		lsp_sync_text_document_did_close(srv_old, doc);
	}

	// might be NULL because lsp_server_get() just launched new server but should
	// be opened once the new server starts
	if (srv_new)
	{
		lsp_sync_text_document_did_open(srv_new, doc);
		on_document_visible(doc);
	}
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
	if (!session_opening)
		on_document_visible(doc);
}


static gboolean on_editor_notify(G_GNUC_UNUSED GObject *obj, GeanyEditor *editor, SCNotification *nt,
	G_GNUC_UNUSED gpointer user_data)
{
	GeanyDocument *doc = editor->document;
	ScintillaObject *sci = editor->sci;

#ifndef HAVE_GEANY_PLUGIN_EXTENSION
	if (!lsp_utils_doc_ft_has_tags(doc))
	{
#endif
		if (nt->nmhdr.code == SCN_AUTOCSELECTION)
		{
			LspServer *srv = lsp_server_get_if_running(doc);

			if (!srv || !srv->config.autocomplete_enable)
				return FALSE;

			// ignore cursor position change as a result of autocomplete (for highlighting)
			ignore_selection_change = TRUE;

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
			return FALSE;
		}
		else if (nt->nmhdr.code == SCN_CALLTIPCLICK)
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
#ifndef HAVE_GEANY_PLUGIN_EXTENSION
	}
#endif

	if (nt->nmhdr.code == SCN_DWELLSTART)
	{
		LspServer *srv = lsp_server_get(doc);

		// also delivered when other window has focus
		if (!gtk_widget_has_focus(GTK_WIDGET(sci)))
			return FALSE;

		// the event is also delivered for the margin with numbers where position
		// is -1. In addition, at the top of Scintilla window, the event is delivered
		// when mouse is at the menubar place, with y = 0
		if (nt->position < 0 || nt->y == 0)
			return FALSE;

		if (!srv)
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
		LspServer *srv = lsp_server_get(doc);
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
		if (!(nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_BEFOREDELETE | SC_MOD_BEFOREINSERT)))
			return FALSE;

		srv = lsp_server_get(doc);

		if (!srv || !doc->real_path)
			return FALSE;

		// BEFORE insert, BEFORE delete - send the original document
		if (!lsp_sync_is_document_open(doc) &&
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
			gchar *text = malloc(nt->length + 1);

			memcpy(text, nt->text, nt->length);
			text[nt->length] = '\0';
			lsp_sync_text_document_did_change(srv, doc, pos_start, pos_end, text);
			lsp_code_lens_send_request(doc);

			g_free(text);
		}
		else if (nt->modificationType & SC_MOD_BEFOREDELETE)  // BEFORE! delete
		{
			LspPosition pos_start = lsp_utils_scintilla_pos_to_lsp(sci, nt->position);
			LspPosition pos_end = lsp_utils_scintilla_pos_to_lsp(sci, nt->position + nt->length);
			gchar *text = g_strdup("");

			lsp_sync_text_document_did_change(srv, doc, pos_start, pos_end, text);
			lsp_code_lens_send_request(doc);

			g_free(text);
		}

		if (nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_BEFOREDELETE))
		{
			if (symbol_highlight_provided(doc))
				lsp_semtokens_send_request(doc);
#ifdef HAVE_GEANY_PLUGIN_EXTENSION
			if (doc_symbols_provided(doc))
				lsp_symbols_doc_request(doc, lsp_symbol_request_cb, doc);
#endif
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
		}

		if (srv->config.highlighting_enable && !ignore_selection_change &&
			(nt->updated & SC_UPDATE_SELECTION))
		{
			lsp_highlight_send_request(srv, doc);
		}
		ignore_selection_change = FALSE;
	}
	else if (nt->nmhdr.code == SCN_CHARADDED)
	{
		// don't hightlight while typing
		lsp_highlight_clear(doc);

#ifndef HAVE_GEANY_PLUGIN_EXTENSION
		if (autocomplete_provided(doc))
			autocomplete_perform(doc);
		if (calltips_provided(doc))
			calltips_show(doc, FALSE);
#endif
	}

	return FALSE;
}


static void on_project_open(G_GNUC_UNUSED GObject *obj, GKeyFile *kf,
	G_GNUC_UNUSED gpointer user_data)
{
	gboolean have_project_config;

	project_configuration_type = g_key_file_get_integer(kf, "lsp", "settings_type", NULL);
	project_configuration_file = g_key_file_get_string(kf, "lsp", "config_file", NULL);

	have_project_config = lsp_utils_get_project_config_filename() != NULL;
	gtk_widget_set_sensitive(menu_items.project_config, have_project_config);
	gtk_widget_set_sensitive(menu_items.user_config, !have_project_config);

	stop_and_init_all_servers();
}


static void on_project_close(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer user_data)
{
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

	project_configuration_type = gtk_combo_box_get_active(GTK_COMBO_BOX(project_dialog.settings_type_combo));
	config_file = gtk_entry_get_text(GTK_ENTRY(project_dialog.config_file_entry));
	SETPTR(project_configuration_file, g_strdup(config_file));

	have_project_config = lsp_utils_get_project_config_filename() != NULL;
	gtk_widget_set_sensitive(menu_items.project_config, have_project_config);
	gtk_widget_set_sensitive(menu_items.user_config, !have_project_config);

	restart_all_servers();
}


static void update_sensitivity(LspProjectConfigurationType config)
{
	if (config == ProjectConfigurationType)
		gtk_widget_set_sensitive(project_dialog.path_box, TRUE);
	else
		gtk_widget_set_sensitive(project_dialog.path_box, FALSE);
}


static void on_combo_changed(void)
{
	update_sensitivity(gtk_combo_box_get_active(GTK_COMBO_BOX(project_dialog.settings_type_combo)));
}


static void add_project_properties_tab(GtkWidget *notebook)
{
	GtkWidget *vbox, *hbox, *ebox, *table_box;
	GtkWidget *label;
	GtkSizeGroup *size_group;

	table_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_set_spacing(GTK_BOX(table_box), 6);

	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	label = gtk_label_new(_("Configuration:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);

	project_dialog.settings_type_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(project_dialog.settings_type_combo), _("Use user configuration"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(project_dialog.settings_type_combo), _("Use project configuration"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(project_dialog.settings_type_combo), _("Disable LSP Client for project"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(project_dialog.settings_type_combo), project_configuration_type);
	g_signal_connect(project_dialog.settings_type_combo, "changed", on_combo_changed, NULL);

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

	update_sensitivity(project_configuration_type);

	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), project_dialog.path_box, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), table_box, TRUE, TRUE, 12);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
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
	g_key_file_set_integer(kf, "lsp", "settings_type", project_configuration_type);
	g_key_file_set_string(kf, "lsp", "config_file",
		project_configuration_file ? project_configuration_file : "");
}


static void code_action_cb(GtkWidget *widget, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get_if_running(doc);
	guint index = GPOINTER_TO_UINT(user_data);
	LspCommand *cmd;

	if (!srv || index >= commands->len)
		return;

	cmd = commands->pdata[index];
	lsp_command_send_request(srv, cmd->command, cmd->arguments);
}


static void remove_item(GtkWidget *widget, gpointer data)
{
	gtk_container_remove(GTK_CONTAINER(data), widget);
}


static void update_command_menu_items(void)
{
	GeanyDocument *doc = document_get_current();
	GtkWidget *menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(context_menu_items.command_item));
	GPtrArray *arr = lsp_command_get_resolved_code_actions();
	LspCommand *cmd;
	guint i;

	if (commands)
		g_ptr_array_free(commands, TRUE);
	commands = g_ptr_array_new();

	gtk_container_foreach(GTK_CONTAINER(menu), remove_item, menu);

	foreach_ptr_array(cmd, i, arr)
	{
		g_ptr_array_add(commands, cmd);
	}

	if (doc)
	{
		guint line = sci_get_line_from_position(doc->editor->sci, last_click_pos);

		lsp_code_lens_append_commands(commands, line);
	}

	foreach_ptr_array(cmd, i, commands)
	{
		GtkWidget *item = gtk_menu_item_new_with_label(cmd->title);

		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(code_action_cb), GUINT_TO_POINTER(i));
	}

	gtk_widget_show_all(context_menu_items.command_item);
	gtk_widget_set_sensitive(context_menu_items.command_item, commands->len > 0);
}


static gboolean on_update_editor_menu(G_GNUC_UNUSED GObject *obj,
	const gchar *word, gint pos, GeanyDocument *doc, gpointer user_data)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	gboolean goto_definition_enable = srv && srv->config.goto_definition_enable;
	gboolean goto_references_enable = srv && srv->config.goto_references_enable;
	gboolean goto_type_definition_enable = srv && srv->config.goto_type_definition_enable;
	gboolean execute_command_enable = srv && srv->config.execute_command_enable;
	gboolean document_formatting_enable = srv && srv->config.document_formatting_enable;
	gboolean range_formatting_enable = srv && srv->config.range_formatting_enable;
	gboolean rename_enable = srv && srv->config.rename_enable;
	gboolean highlighting_enable = srv && srv->config.highlighting_enable;

	gtk_widget_set_sensitive(context_menu_items.goto_ref, goto_references_enable);
	gtk_widget_set_sensitive(context_menu_items.goto_def, goto_definition_enable);
	gtk_widget_set_sensitive(context_menu_items.goto_type_def, goto_type_definition_enable);

	gtk_widget_set_sensitive(context_menu_items.rename_in_file, highlighting_enable);
	gtk_widget_set_sensitive(context_menu_items.rename_in_project, rename_enable);
	gtk_widget_set_sensitive(context_menu_items.format_code, document_formatting_enable || range_formatting_enable);

	gtk_widget_set_sensitive(context_menu_items.command_item, execute_command_enable);
	if (execute_command_enable)
	{
		last_click_pos = pos;
		lsp_command_send_code_action_request(pos, update_command_menu_items);
	}

	return FALSE;
}


static void on_session_opening(G_GNUC_UNUSED GObject *obj, gboolean opening,
		G_GNUC_UNUSED gpointer user_data)
{
	session_opening = opening;
	if (!opening)
		on_document_visible(document_get_current());
}


PluginCallback plugin_callbacks[] = {
	{"document-new", (GCallback) &on_document_new, FALSE, NULL},
	{"document-open", (GCallback) &on_document_open, FALSE, NULL},
	{"document-close", (GCallback) &on_document_close, FALSE, NULL},
	{"document-reload", (GCallback) &on_document_reload, FALSE, NULL},
	{"document-activate", (GCallback) &on_document_activate, FALSE, NULL},
	{"document-save", (GCallback) &on_document_save, FALSE, NULL},
	{"document-before-save-as", (GCallback) &on_document_before_save_as, TRUE, NULL},
	{"document-filetype-set", (GCallback) &on_document_filetype_set, FALSE, NULL},
	{"editor-notify", (GCallback) &on_editor_notify, FALSE, NULL},
	{"update-editor-menu", (GCallback) &on_update_editor_menu, FALSE, NULL},
	{"session-opening", (GCallback) &on_session_opening, FALSE, NULL},
	{"project-open", (GCallback) &on_project_open, FALSE, NULL},
	{"project-close", (GCallback) &on_project_close, FALSE, NULL},
	{"project-save", (GCallback) &on_project_save, FALSE, NULL},
	{"project-dialog-open", (GCallback) &on_project_dialog_open, FALSE, NULL},
	{"project-dialog-confirmed", (GCallback) &on_project_dialog_confirmed, FALSE, NULL},
	{"project-dialog-close", (GCallback) &on_project_dialog_close, FALSE, NULL},
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
	GeanyDocument *doc = document_get_current();;

	// TODO: workaround strange behavior of clangd: it doesn't seem to reflect changes
	// in non-open files unless all files are saved and the server is restarted
	lsp_utils_save_all_docs();
	restart_all_servers();
	if (doc)
	{
		if (symbol_highlight_provided(doc))
			lsp_semtokens_send_request(doc);
	}
}


static void invoke_kb(guint key_id, gint pos)
{
	GeanyDocument *doc = document_get_current();

	if (pos < 0)
		pos = doc ? sci_get_current_position(doc->editor->sci) : 0;

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

		case KB_FIND_REFERENCES:
			lsp_goto_references(pos);
			break;
		case KB_FIND_IMPLEMENTATIONS:
			lsp_goto_implementations(pos);
			break;

		case KB_SHOW_HOVER_POPUP:
			show_hover_popup();
			break;

		case KB_RENAME_IN_FILE:
			lsp_highlight_rename(pos);
			break;

		case KB_RENAME_IN_PROJECT:
			lsp_rename_send_request(pos, on_rename_done);
			break;

		case KB_FORMAT_CODE:
			lsp_format_perform();
			break;

		case KB_RESTART_SERVERS:
			restart_all_servers();
			break;

#ifndef HAVE_GEANY_PLUGIN_EXTENSION
		case KB_INVOKE_AUTOCOMPLETE:
			if (doc && autocomplete_provided(doc))
				autocomplete_perform(doc);
			break;

		case KB_SHOW_CALLTIP:
			if (doc && calltips_provided(doc))
				calltips_show(doc, TRUE);
			break;
#endif

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
	GtkWidget *menu, *item, *command_submenu;
	GeanyKeyGroup *group;

	group = plugin_set_key_group(geany_plugin, "lsp", KB_COUNT, on_kb_invoked);

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

	gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());

	menu_items.rename_in_file = gtk_menu_item_new_with_mnemonic(_("_Rename in File"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.rename_in_file);
	g_signal_connect(menu_items.rename_in_file, "activate", G_CALLBACK(on_menu_invoked),
		GUINT_TO_POINTER(KB_RENAME_IN_FILE));
	keybindings_set_item(group, KB_RENAME_IN_FILE, NULL, 0, 0, "rename_in_file",
		_("Rename in file"), menu_items.rename_in_file);

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

#ifndef HAVE_GEANY_PLUGIN_EXTENSION
	keybindings_set_item(group, KB_INVOKE_AUTOCOMPLETE, NULL, 0, 0, "invoke_autocompletion",
		_("Invoke autocompletion"), NULL);

	keybindings_set_item(group, KB_SHOW_CALLTIP, NULL, 0, 0, "show_calltip",
		_("Show calltip"), NULL);
#endif

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

	context_menu_items.rename_in_file = gtk_menu_item_new_with_mnemonic(_("_Rename in File (LSP)"));
	gtk_widget_show(context_menu_items.rename_in_file);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.rename_in_file);
	g_signal_connect(context_menu_items.rename_in_file, "activate", G_CALLBACK(on_context_menu_invoked),
		GUINT_TO_POINTER(KB_RENAME_IN_FILE));

	context_menu_items.separator2 = gtk_separator_menu_item_new();
	gtk_widget_show(context_menu_items.separator2);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.separator2);

	context_menu_items.goto_type_def = gtk_menu_item_new_with_mnemonic(_("Go to _Type Definition (LSP)"));
	gtk_widget_show(context_menu_items.goto_type_def);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), context_menu_items.goto_type_def);
	g_signal_connect(context_menu_items.goto_type_def, "activate", G_CALLBACK(on_context_menu_invoked),
		GUINT_TO_POINTER(KB_GOTO_TYPE_DEFINITION));

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
}


static void on_server_initialized(LspServer *srv)
{
	GeanyDocument *current_doc = document_get_current();
	guint i;

	update_menu(current_doc);

	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];

		// see on_document_visible() for detailed comment
		if (doc->file_type->id == srv->filetype && (doc->changed || doc == current_doc))
		{
			// returns NULL if e.g. configured not to use LSP outside project dir
			LspServer *s2 = lsp_server_get_if_running(doc);

			if (s2)
				lsp_sync_text_document_did_open(srv, doc);
		}
	}
}


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	plugin_module_make_resident(geany_plugin);

	lsp_server_set_initialized_cb(on_server_initialized);

	stop_and_init_all_servers();

#ifdef HAVE_GEANY_PLUGIN_EXTENSION
	plugin_extension_register(&extension);
#endif
	create_menu_items();
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(menu_items.parent_item);
	menu_items.parent_item = NULL;

	gtk_widget_destroy(context_menu_items.goto_type_def);
	gtk_widget_destroy(context_menu_items.goto_def);
	gtk_widget_destroy(context_menu_items.format_code);
	gtk_widget_destroy(context_menu_items.rename_in_file);
	gtk_widget_destroy(context_menu_items.rename_in_project);
	gtk_widget_destroy(context_menu_items.goto_ref);
	gtk_widget_destroy(context_menu_items.command_item);
	gtk_widget_destroy(context_menu_items.separator1);
	gtk_widget_destroy(context_menu_items.separator2);

#ifdef HAVE_GEANY_PLUGIN_EXTENSION
	plugin_extension_unregister(&extension);
#endif
	lsp_server_set_initialized_cb(NULL);
	lsp_server_stop_all(TRUE);
	destroy_all();
}


void plugin_help(void)
{
	utils_open_browser("https://plugins.geany.org/lsp.html");
}
