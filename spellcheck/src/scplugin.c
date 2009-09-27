/*
 *      scplugin.c - this file is part of Spellcheck, a Geany plugin
 *
 *      Copyright 2008-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 *
 * $Id$
 */


#include "geanyplugin.h"


#include "scplugin.h"
#include "gui.h"
#include "speller.h"


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(147);
PLUGIN_SET_INFO(_("Spell Check"), _("Checks the spelling of the current document."), VERSION,
			_("The Geany developer team"))


SpellCheck *sc_info = NULL;



/* Keybinding(s) */
enum
{
	KB_SPELL_CHECK,
	KB_SPELL_TOOGLE_TYPING,
	KB_COUNT
};
PLUGIN_KEY_GROUP(spellcheck, KB_COUNT)




PluginCallback plugin_callbacks[] =
{
	{ "update-editor-menu", (GCallback) &sc_gui_update_editor_menu_cb, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


static void configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(sc_info->config_file);

		setptr(sc_info->default_language, gtk_combo_box_get_active_text(GTK_COMBO_BOX(
			g_object_get_data(G_OBJECT(dialog), "combo"))));
		setptr(sc_info->dictionary_dir, g_strdup(gtk_entry_get_text(GTK_ENTRY(
			g_object_get_data(G_OBJECT(dialog), "dict_dir")))));
		sc_speller_reinit_enchant_dict();

		sc_info->check_while_typing = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_type"))));

		sc_info->use_msgwin = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_msgwin"))));

		sc_info->show_toolbar_item = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_toolbar"))));

		g_key_file_load_from_file(config, sc_info->config_file, G_KEY_FILE_NONE, NULL);
		if (sc_info->default_language != NULL) /* lang may be NULL */
			g_key_file_set_string(config, "spellcheck", "language", sc_info->default_language);
		g_key_file_set_boolean(config, "spellcheck", "check_while_typing", sc_info->check_while_typing);
		g_key_file_set_boolean(config, "spellcheck", "use_msgwin", sc_info->use_msgwin);
		g_key_file_set_boolean(config, "spellcheck", "show_toolbar_item", sc_info->show_toolbar_item);
		g_key_file_set_string(config, "spellcheck", "dictionary_dir", sc_info->dictionary_dir);

		sc_gui_update_toolbar();
		sc_gui_update_menu();

		if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Plugin configuration directory could not be created."));
		}
		else
		{
			/* write config to file */
			data = g_key_file_to_data(config, NULL, NULL);
			utils_write_file(sc_info->config_file, data);
			g_free(data);
		}
		g_free(config_dir);
		g_key_file_free(config);
	}
}


void plugin_init(GeanyData *data)
{
	GKeyFile *config = g_key_file_new();
	gchar *default_lang;

	default_lang = sc_speller_get_default_lang();
	sc_info = g_new0(SpellCheck, 1);

	sc_info->config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"spellcheck", G_DIR_SEPARATOR_S, "spellcheck.conf", NULL);

	g_key_file_load_from_file(config, sc_info->config_file, G_KEY_FILE_NONE, NULL);
	sc_info->default_language = utils_get_setting_string(config,
		"spellcheck", "language", default_lang);
	sc_info->check_while_typing = utils_get_setting_boolean(config,
		"spellcheck", "check_while_typing", FALSE);
	sc_info->show_toolbar_item = utils_get_setting_boolean(config,
		"spellcheck", "show_toolbar_item", TRUE);
	sc_info->dictionary_dir = utils_get_setting_string(config,
		"spellcheck", "dictionary_dir", NULL);
	sc_info->use_msgwin = utils_get_setting_boolean(config, "spellcheck", "use_msgwin", FALSE);
	g_key_file_free(config);
	g_free(default_lang);

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	sc_info->menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_SPELL_CHECK, NULL);
	ui_add_document_sensitive(sc_info->menu_item);

	sc_gui_update_toolbar();

	sc_gui_init();
	sc_speller_init();

	sc_gui_create_edit_menu();
	sc_gui_create_menu(sc_info->menu_item);
	gtk_widget_show_all(sc_info->menu_item);

	sc_info->signal_id = g_signal_connect(geany->main_widgets->window,
			"key-release-event", G_CALLBACK(sc_gui_key_release_cb), NULL);

	/* setup keybindings */
	keybindings_set_item(plugin_key_group, KB_SPELL_CHECK, sc_gui_kb_run_activate_cb,
		0, 0, "spell_check", _("Run Spell Check"), sc_info->submenu_item_default);
	keybindings_set_item(plugin_key_group, KB_SPELL_TOOGLE_TYPING,
		sc_gui_kb_toggle_typing_activate_cb, 0, 0, "spell_toggle_typing",
		_("Toggle Check While Typing"), NULL);
}


#ifdef HAVE_ENCHANT_1_5
static void dictionary_dir_button_clicked_cb(GtkButton *button, gpointer item)
{
	GtkWidget *dialog;
	gchar *text;

	/* initialize the dialog */
	dialog = gtk_file_chooser_dialog_new(_("Select Directory"), NULL,
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	text = utils_get_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(item)));
	if (NZV(text))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), text);

	/* run it */
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *utf8_filename, *tmp;

		tmp = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8_filename = utils_get_utf8_from_locale(tmp);

		gtk_entry_set_text(GTK_ENTRY(item), utf8_filename);

		g_free(utf8_filename);
		g_free(tmp);
	}

	gtk_widget_destroy(dialog);
}
#endif


GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *label, *vbox, *combo, *check_type, *check_msgwin, *check_toolbar;
#ifdef HAVE_ENCHANT_1_5
	GtkWidget *entry_dir, *hbox, *button, *image;
#endif
	guint i;

	vbox = gtk_vbox_new(FALSE, 6);

	check_type = gtk_check_button_new_with_label(_("Check spelling while typing"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_type), sc_info->check_while_typing);
	gtk_box_pack_start(GTK_BOX(vbox), check_type, FALSE, FALSE, 6);

	check_toolbar = gtk_check_button_new_with_label(_("Show toolbar item to toggle spell checking"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_toolbar), sc_info->show_toolbar_item);
	gtk_box_pack_start(GTK_BOX(vbox), check_toolbar, FALSE, FALSE, 3);

	check_msgwin = gtk_check_button_new_with_label(
		_("Print misspelled words and suggestions in the messages window"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_msgwin), sc_info->use_msgwin);
	gtk_box_pack_start(GTK_BOX(vbox), check_msgwin, FALSE, FALSE, 3);

	label = gtk_label_new(_("Language to use for the spell check:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 3);

	combo = gtk_combo_box_new_text();

	for (i = 0; i < sc_info->dicts->len; i++)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), g_ptr_array_index(sc_info->dicts, i));

		if (utils_str_equal(g_ptr_array_index(sc_info->dicts, i), sc_info->default_language))
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
	}
	/* if the default language couldn't be selected, select the first available language */
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) == -1)
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

	if (sc_info->dicts->len > 20)
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 3);
	else if (sc_info->dicts->len > 10)
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 2);
	gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, 6);

#ifdef HAVE_ENCHANT_1_5
	label = gtk_label_new_with_mnemonic(_("_Directory to look for dictionary files:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	entry_dir = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_dir);
	ui_widget_set_tooltip_text(entry_dir,
		_("Read additional dictionary files from this directory. "
		  "For now, this only works with myspell dictionaries."));
	if (NZV(sc_info->dictionary_dir))
		gtk_entry_set_text(GTK_ENTRY(entry_dir), sc_info->dictionary_dir);

	button = gtk_button_new();
	g_signal_connect(button, "clicked",
		G_CALLBACK(dictionary_dir_button_clicked_cb), entry_dir);

	image = gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start_defaults(GTK_BOX(hbox), entry_dir);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	g_object_set_data(G_OBJECT(dialog), "dict_dir", entry_dir);
#endif
	g_object_set_data(G_OBJECT(dialog), "combo", combo);
	g_object_set_data(G_OBJECT(dialog), "check_type", check_type);
	g_object_set_data(G_OBJECT(dialog), "check_msgwin", check_msgwin);
	g_object_set_data(G_OBJECT(dialog), "check_toolbar", check_toolbar);
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);

	gtk_widget_show_all(vbox);

	return vbox;
}


void plugin_help(void)
{
/*
	gchar *readme = g_build_filename(DOCDIR, "README", NULL);

	if (g_file_test(readme, G_FILE_TEST_EXISTS))
	{
		document_open_file(readme, FALSE, filetypes_index(GEANY_FILETYPES_REST), NULL);
	}
	else
	{
		utils_open_browser("http://plugins.geany.org/spellcheck/");
	}
	g_free(readme);
*/
	utils_open_browser("http://plugins.geany.org/spellcheck/");
}


void plugin_cleanup(void)
{
	guint i;

	for (i = 0; i < sc_info->dicts->len; i++)
	{
		g_free(g_ptr_array_index(sc_info->dicts, i));
	}
	g_ptr_array_free(sc_info->dicts, TRUE);

	g_signal_handler_disconnect(geany->main_widgets->window, sc_info->signal_id);

	gtk_widget_destroy(sc_info->edit_menu);
	gtk_widget_destroy(sc_info->edit_menu_sep);
	if (sc_info->toolbar_button != NULL)
		gtk_widget_destroy(GTK_WIDGET(sc_info->toolbar_button));

	sc_gui_free();
	sc_speller_free();

	g_free(sc_info->dictionary_dir);
	g_free(sc_info->default_language);
	g_free(sc_info->config_file);
	gtk_widget_destroy(sc_info->menu_item);
	g_free(sc_info);
}
