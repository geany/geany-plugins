/*
 *  geanydoc.c
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "geanyplugin.h"


#include "geanydoc.h"

/* These items are set by Geany before init() is called. */
PluginFields *plugin_fields;
GeanyData *geany_data;
GeanyFunctions *geany_functions;

static GtkWidget *keyb1;
static GtkWidget *keyb2;


/* Check that Geany supports plugin API version 128 or later, and check
 * for binary compatibility. */
PLUGIN_VERSION_CHECK(128)
/* All plugins must set name, description, version and author. */
	PLUGIN_SET_INFO(_("Doc"), _("Call documentation viewer on current symbol."), VERSION,
		_("Yura Siamshka <yurand2@gmail.com>"));

/* Keybinding(s) */
     enum
     {
	     KB_DOCUMENT_WORD,
	     KB_DOCUMENT_WORD_ASK,
	     KB_COUNT
     };

PLUGIN_KEY_GROUP(doc_chars, KB_COUNT);

     GtkWidget *create_Interactive(void);

     static gboolean word_check_left(gchar c)
{
	if (g_ascii_isalnum(c) || c == '_' || c == '.')
		return TRUE;
	return FALSE;
}

static gboolean
word_check_right(gchar c)
{
	if (g_ascii_isalnum(c) || c == '_')
		return TRUE;
	return FALSE;
}

static gchar *
current_word()
{
	gchar *txt;
	GeanyDocument *doc;

	gint pos;
	gint cstart, cend;
	gchar c;
	gint text_len;

	doc = document_get_current();
	g_return_val_if_fail(doc != NULL && doc->file_name != NULL, NULL);

	text_len = sci_get_selected_text_length(doc->editor->sci);
	if (text_len > 1)
	{
		txt = g_malloc(text_len + 1);
		sci_get_selected_text(doc->editor->sci, txt);
		return txt;
	}

	pos = sci_get_current_position(doc->editor->sci);
	if (pos > 0)
		pos--;

	cstart = pos;
	c = sci_get_char_at(doc->editor->sci, cstart);

	if (!word_check_left(c))
		return NULL;

	while (word_check_left(c))
	{
		cstart--;
		if (cstart >= 0)
			c = sci_get_char_at(doc->editor->sci, cstart);
		else
			break;
	}
	cstart++;

	cend = pos;
	c = sci_get_char_at(doc->editor->sci, cend);
	while (word_check_right(c) && cend < sci_get_length(doc->editor->sci))
	{
		cend++;
		c = sci_get_char_at(doc->editor->sci, cend);
	}

	if (cstart == cend)
		return NULL;
	txt = g_malloc0(cend - cstart + 1);

	sci_get_text_range(doc->editor->sci, cstart, cend, txt);
	return txt;
}

/* name should be in UTF-8, and can have a path. */
static void
show_output(const gchar * std_output, const gchar * name, const gchar * force_encoding,
	    gint filetype_new_file)
{
	gint page;
	GtkNotebook *book;
	GeanyDocument *doc, *cur_doc;

	if (std_output)
	{
		cur_doc = document_get_current();
		doc = document_find_by_filename(name);
		if (doc == NULL)
		{
			doc = document_new_file(name,
						   filetypes[filetype_new_file],
						   std_output);
		}
		else
		{
			sci_set_text(doc->editor->sci, std_output);
			book = GTK_NOTEBOOK(geany->main_widgets->notebook);
			page = gtk_notebook_page_num(book, GTK_WIDGET(doc->editor->sci));
			gtk_notebook_set_current_page(book, page);
		}
		document_set_text_changed(doc, FALSE);
		document_set_encoding(doc, (force_encoding ? force_encoding : "UTF-8"));
		navqueue_goto_line(cur_doc, document_get_current(), 1);
	}
	else
	{
		ui_set_statusbar(FALSE, _("Could not parse the output of command"));
	}
}

void
show_doc(const gchar * word, gint cmd_num)
{
	GeanyDocument *doc;
	const gchar *ftype;
	gchar *command;
	gchar *tmp;
	gboolean intern;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	ftype = doc->file_type->name;
	command = config_get_command(ftype, cmd_num, &intern);
	if (!NZV(command))
	{
		g_free(command);
		return;
	}

	tmp = strstr(command, "%w");
	if (tmp != NULL)
	{
		tmp[1] = 's';
		setptr(command, g_strdup_printf(command, word));
	}

	if (intern)
	{
		g_spawn_command_line_sync(command, &tmp, NULL, NULL, NULL);
		if (NZV(tmp))
		{
			show_output(tmp, "*DOC*", NULL, doc->file_type->id);
		}
		else
		{
			show_doc(word, cmd_num + 1);
		}
		g_free(tmp);
	}
	else
	{
		g_spawn_command_line_async(command, NULL);
	}
	g_free(command);
}

static void
kb_doc(G_GNUC_UNUSED guint key_id)
{
	gchar *word = current_word();
	if (word)
	{
		show_doc(word, 0);
		g_free(word);
	}
}

static void
kb_doc_ask(G_GNUC_UNUSED guint key_id)
{
	gchar *word = NULL;
	GtkWidget *dialog, *entry;

	/* example configuration dialog */
	dialog = create_Interactive();

	/* run the dialog and check for the response code */
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		entry = ui_lookup_widget(dialog, "entry_word");
		word = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	gtk_widget_destroy(dialog);

	if (word)
	{
		show_doc(word, 0);
		g_free(word);
	}
}

static void
on_comboboxType_changed(GtkComboBox * combobox, G_GNUC_UNUSED gpointer user_data)
{
	gchar *from, *to;
	GtkWidget *cmd0 = ui_lookup_widget(GTK_WIDGET(combobox), "entryCommand0");
	GtkWidget *cmd1 = ui_lookup_widget(GTK_WIDGET(combobox), "entryCommand1");
	GtkWidget *intern = ui_lookup_widget(GTK_WIDGET(combobox), "cbIntern");

	gchar *cmd0_txt = (gchar *) gtk_entry_get_text(GTK_ENTRY(cmd0));
	gchar *cmd1_txt = (gchar *) gtk_entry_get_text(GTK_ENTRY(cmd1));
	gboolean intern_b = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(intern));
	GKeyFile *config = (GKeyFile *) g_object_get_data(G_OBJECT(combobox), "config");

	from = g_object_get_data(G_OBJECT(combobox), "current");
	to = gtk_combo_box_get_active_text(combobox);

	if (from != NULL)
	{
		if (NZV(cmd0_txt))
			g_key_file_set_string(config, from, "command0", cmd0_txt);
		else
			g_key_file_remove_key(config, from, "command0", NULL);
		if (NZV(cmd1_txt))
			g_key_file_set_string(config, from, "command1", cmd1_txt);
		else
			g_key_file_remove_key(config, from, "command1", NULL);
		g_key_file_set_boolean(config, from, "internal", intern_b);
		g_free(from);
	}
	g_object_set_data(G_OBJECT(combobox), "current", g_strdup(to));

	cmd0_txt = utils_get_setting_string(config, to, "command0", "");
	cmd1_txt = utils_get_setting_string(config, to, "command1", "");
	intern_b = utils_get_setting_boolean(config, to, "internal", FALSE);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(intern), intern_b);
	gtk_entry_set_text(GTK_ENTRY(cmd0), cmd0_txt);
	gtk_entry_set_text(GTK_ENTRY(cmd1), cmd1_txt);
}

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

GtkWidget *
create_Interactive(void)
{
	GtkWidget *dialog_vbox1;
	GtkWidget *entry_word;
	GtkWidget *dialog = gtk_dialog_new_with_buttons("Document interactive",
							GTK_WINDOW(geany->main_widgets->window),
							GTK_DIALOG_MODAL |
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							NULL);
	dialog_vbox1 = GTK_DIALOG(dialog)->vbox;

	entry_word = gtk_entry_new();
	gtk_widget_show(entry_word);
	gtk_box_pack_start(GTK_BOX(dialog_vbox1), entry_word, TRUE, TRUE, 0);


	GLADE_HOOKUP_OBJECT(dialog, entry_word, "entry_word");
	return dialog;
}

GtkWidget *
create_Configure(void)
{
	GtkWidget *Configure;
	GtkWidget *dialog_vbox1;
	GtkWidget *vbox1;
	GtkWidget *cbIntern;
	GtkWidget *comboboxType;
	GtkWidget *table1;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *entryCommand0;
	GtkWidget *entryCommand1;
	GtkWidget *label2;
	GtkWidget *dialog_action_area1;
	GtkWidget *cancelbutton1;
	GtkWidget *okbutton1;

	Configure = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(Configure), _("Doc"));
	gtk_window_set_type_hint(GTK_WINDOW(Configure), GDK_WINDOW_TYPE_HINT_DIALOG);

	dialog_vbox1 = GTK_DIALOG(Configure)->vbox;
	gtk_widget_show(dialog_vbox1);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(dialog_vbox1), vbox1, TRUE, TRUE, 0);

	cbIntern = gtk_check_button_new_with_mnemonic(_("Put output in buffer"));
	gtk_widget_show(cbIntern);
	gtk_box_pack_start(GTK_BOX(vbox1), cbIntern, FALSE, FALSE, 0);

	comboboxType = gtk_combo_box_new_text();
	gtk_widget_show(comboboxType);
	gtk_box_pack_start(GTK_BOX(vbox1), comboboxType, FALSE, FALSE, 0);

	table1 = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table1);
	gtk_box_pack_start(GTK_BOX(vbox1), table1, TRUE, TRUE, 0);

	label3 = gtk_label_new(_("Command 0:"));
	gtk_widget_show(label3);
	gtk_table_attach(GTK_TABLE(table1), label3, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	label4 = gtk_label_new(_("Command 1:"));
	gtk_widget_show(label4);
	gtk_table_attach(GTK_TABLE(table1), label4, 0, 1, 1, 2,
			 (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label4), 0, 0.5);

	entryCommand0 = gtk_entry_new();
	gtk_widget_show(entryCommand0);
	gtk_table_attach(GTK_TABLE(table1), entryCommand0, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_invisible_char(GTK_ENTRY(entryCommand0), 8226);

	entryCommand1 = gtk_entry_new();
	gtk_widget_show(entryCommand1);
	gtk_table_attach(GTK_TABLE(table1), entryCommand1, 1, 2, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_invisible_char(GTK_ENTRY(entryCommand1), 8226);

	label2 = gtk_label_new(_("%w will be replaced with current word\n"));
	gtk_widget_show(label2);
	gtk_box_pack_start(GTK_BOX(dialog_vbox1), label2, FALSE, FALSE, 0);

	dialog_action_area1 = GTK_DIALOG(Configure)->action_area;
	gtk_widget_show(dialog_action_area1);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(dialog_action_area1), GTK_BUTTONBOX_END);

	cancelbutton1 = gtk_button_new_from_stock("gtk-cancel");
	gtk_widget_show(cancelbutton1);
	gtk_dialog_add_action_widget(GTK_DIALOG(Configure), cancelbutton1, GTK_RESPONSE_CANCEL);
	GTK_WIDGET_SET_FLAGS(cancelbutton1, GTK_CAN_DEFAULT);

	okbutton1 = gtk_button_new_from_stock("gtk-ok");
	gtk_widget_show(okbutton1);
	gtk_dialog_add_action_widget(GTK_DIALOG(Configure), okbutton1, GTK_RESPONSE_OK);
	GTK_WIDGET_SET_FLAGS(okbutton1, GTK_CAN_DEFAULT);

	g_signal_connect_after((gpointer) comboboxType, "changed",
			       G_CALLBACK(on_comboboxType_changed), NULL);

	/* Store pointers to all widgets, for use by lookup_widget(). */
	GLADE_HOOKUP_OBJECT_NO_REF(Configure, Configure, "Configure");
	GLADE_HOOKUP_OBJECT_NO_REF(Configure, dialog_vbox1, "dialog_vbox1");
	GLADE_HOOKUP_OBJECT(Configure, vbox1, "vbox1");
	GLADE_HOOKUP_OBJECT(Configure, cbIntern, "cbIntern");
	GLADE_HOOKUP_OBJECT(Configure, comboboxType, "comboboxType");
	GLADE_HOOKUP_OBJECT(Configure, table1, "table1");
	GLADE_HOOKUP_OBJECT(Configure, label3, "label3");
	GLADE_HOOKUP_OBJECT(Configure, label4, "label4");
	GLADE_HOOKUP_OBJECT(Configure, entryCommand0, "entryCommand0");
	GLADE_HOOKUP_OBJECT(Configure, entryCommand1, "entryCommand1");
	GLADE_HOOKUP_OBJECT(Configure, label2, "label2");
	GLADE_HOOKUP_OBJECT_NO_REF(Configure, dialog_action_area1, "dialog_action_area1");
	GLADE_HOOKUP_OBJECT(Configure, cancelbutton1, "cancelbutton1");
	GLADE_HOOKUP_OBJECT(Configure, okbutton1, "okbutton1");

	return Configure;
}

void
plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	gchar *kb_label1;
	gchar *kb_label2;

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	kb_label1 = _("Document current word");
	kb_label2 = _("Document interactive");

	config_init();

	keyb1 = gtk_menu_item_new();
	keyb2 = gtk_menu_item_new();

	keybindings_set_item(plugin_key_group, KB_DOCUMENT_WORD, kb_doc,
				0, 0, kb_label1, kb_label1, keyb1);
	keybindings_set_item(plugin_key_group, KB_DOCUMENT_WORD_ASK, kb_doc_ask,
				0, 0, kb_label2, kb_label2, keyb2);
}

static void
init_Configure(GtkWidget * dialog)
{
	guint i;
	GtkWidget *cbTypes;

	cbTypes = ui_lookup_widget(dialog, "comboboxType");
	g_object_set(cbTypes, "wrap-width", 3, NULL);

	for (i = 0; i < geany->filetypes_array->len; i++)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(cbTypes),
					  ((struct GeanyFiletype *) (filetypes[i]))->
					  name);
	}
	g_object_set_data(G_OBJECT(cbTypes), "config", config_clone());
	g_object_set_data(G_OBJECT(cbTypes), "current", NULL);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cbTypes), 0);
}

void
plugin_configure_single(G_GNUC_UNUSED GtkWidget * parent)
{
	int ret;
	GtkWidget *dialog;
	GtkWidget *cbTypes;
	GKeyFile *config;
	gchar *current;

	/* example configuration dialog */
	dialog = create_Configure();
	init_Configure(dialog);

	cbTypes = ui_lookup_widget(dialog, "comboboxType");

	/* run the dialog and check for the response code */

	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	config = (GKeyFile *) g_object_get_data(G_OBJECT(cbTypes), "config");
	current = g_object_get_data(G_OBJECT(cbTypes), "current");
	if (ret == GTK_RESPONSE_OK)
	{
		on_comboboxType_changed(GTK_COMBO_BOX(cbTypes), NULL);
		config_set(config);
	}
	else
	{
		g_key_file_free(config);
	}
	g_free(current);
	gtk_widget_destroy(dialog);
}

void
plugin_cleanup(void)
{
	config_uninit();
	keyb1 = NULL;
	keyb2 = NULL;
}
