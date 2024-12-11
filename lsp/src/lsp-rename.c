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

#include "lsp-rename.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"

#include <jsonrpc-glib.h>


static struct
{
	GtkWidget *widget;
	GtkWidget *old_label;
	GtkWidget *combo;
} rename_dialog = {NULL, NULL, NULL};


extern GeanyData *geany_data;

static GtkWidget *progress_dialog;


static gchar *show_dialog_rename(const gchar *old_name)
{
	gint res;
	GtkWidget *entry;
	GtkSizeGroup *size_group;
	const gchar *str = NULL;
	gchar *old_name_str;

	if (!rename_dialog.widget)
	{
		GtkWidget *label, *vbox, *ebox;

		rename_dialog.widget = gtk_dialog_new_with_buttons(
			_("Rename in Project"), GTK_WINDOW(geany->main_widgets->window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_Rename"), GTK_RESPONSE_ACCEPT, NULL);
		gtk_window_set_default_size(GTK_WINDOW(rename_dialog.widget), 600, -1);
		gtk_dialog_set_default_response(GTK_DIALOG(rename_dialog.widget), GTK_RESPONSE_CANCEL);

		vbox = ui_dialog_vbox_new(GTK_DIALOG(rename_dialog.widget));
		gtk_box_set_spacing(GTK_BOX(vbox), 6);

		label = gtk_label_new(_("<b>Warning</b>"));
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, FALSE, 0);

		label = gtk_label_new(_("By pressing the <i>Rename</i> button below, you are going to replace <i>Old name</i> with <i>New name</i> <b>in the whole project</b>. There is no further confirmation or change review after this step."));
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, FALSE, 0);

		label = gtk_label_new(_("Since this operation cannot be undone easily, it is highly recommended to perform this action only after committing all modified files into VCS in case something goes wrong."));
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, FALSE, 0);

		size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

		label = gtk_label_new(_("New name:"));
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_size_group_add_widget(size_group, label);
		rename_dialog.combo = gtk_combo_box_text_new_with_entry();
		entry = gtk_bin_get_child(GTK_BIN(rename_dialog.combo));
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		ui_entry_add_clear_icon(GTK_ENTRY(entry));
		gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

		ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(ebox), rename_dialog.combo, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

		label = gtk_label_new(_("Old name:"));
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_size_group_add_widget(size_group, label);
		rename_dialog.old_label = gtk_label_new("");
		gtk_label_set_use_markup(GTK_LABEL(rename_dialog.old_label), TRUE);
		gtk_label_set_xalign(GTK_LABEL(rename_dialog.old_label), 0.0);

		ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(ebox), rename_dialog.old_label, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

		gtk_widget_show_all(vbox);
	}

	old_name_str = g_markup_printf_escaped("<b>%s</b>", old_name);
	gtk_label_set_markup(GTK_LABEL(rename_dialog.old_label), old_name_str);
	g_free(old_name_str);

	entry = gtk_bin_get_child(GTK_BIN(rename_dialog.combo));
	gtk_entry_set_text(GTK_ENTRY(entry), old_name);
	gtk_widget_grab_focus(entry);

	res = gtk_dialog_run(GTK_DIALOG(rename_dialog.widget));

	if (res == GTK_RESPONSE_ACCEPT)
	{
		str = gtk_entry_get_text(GTK_ENTRY(entry));
		ui_combo_box_add_to_history(GTK_COMBO_BOX_TEXT(rename_dialog.combo), str, 0);
	}

	gtk_widget_hide(rename_dialog.widget);

	return g_strdup(str);
}


static GtkWidget *create_progress_dialog(void)
{
	GtkWidget *dialog, *label, *vbox;

	dialog = gtk_window_new(GTK_WINDOW_POPUP);

	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(geany->main_widgets->window));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_set_name(dialog, "GeanyDialog");

	gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 200, 100);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_container_add(GTK_CONTAINER(dialog), vbox);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Renaming...</b>"));
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, FALSE, 0);

	gtk_widget_show_all(dialog);

	return dialog;
}


static void rename_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	GCallback on_rename_done = user_data;

	gtk_widget_destroy(progress_dialog);
	progress_dialog = NULL;

	if (!error)
	{
		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		if (lsp_utils_apply_workspace_edit(return_value))
			on_rename_done();
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", error->message);
}


void lsp_rename_send_request(gint pos, GCallback on_rename_done)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);
	ScintillaObject *sci;
	LspPosition lsp_pos;
	GVariant *node;
	gchar *selection;
	gchar *iden;

	if (!srv)
		return;

	sci = doc->editor->sci;
	lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);

	iden = lsp_utils_get_current_iden(doc, pos, srv->config.word_chars);
	selection = sci_get_selection_contents(sci);
	if ((!sci_has_selection(sci) && iden) || (sci_has_selection(sci) && g_strcmp0(iden, selection) == 0))
	{
		gchar *new_name = show_dialog_rename(iden);

		if (new_name && new_name[0])
		{
			gchar *doc_uri = lsp_utils_get_doc_uri(doc);

			node = JSONRPC_MESSAGE_NEW (
				"textDocument", "{",
					"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
				"}",
				"position", "{",
					"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
					"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
				"}",
				"newName", JSONRPC_MESSAGE_PUT_STRING(new_name)
			);

			//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

			progress_dialog = create_progress_dialog();

			lsp_rpc_call(srv, "textDocument/rename", node,
				rename_cb, on_rename_done);

			g_free(doc_uri);
			g_variant_unref(node);
		}

		g_free(new_name);
	}

	g_free(iden);
	g_free(selection);
}
