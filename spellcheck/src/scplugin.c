/*
 *      scplugin.c - this file is part of Spellcheck, a Geany plugin
 *
 *      Copyright 2008-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>


#include "scplugin.h"
#include "gui.h"
#include "speller.h"


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(209)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Spell Check"),
	_("Checks the spelling of the current document."),
	VERSION,
	"The Geany developer team")


SpellCheck *sc_info = NULL;



/* Keybinding(s) */
enum
{
	KB_SPELL_CHECK,
	KB_SPELL_TOOGLE_TYPING,
	KB_COUNT
};




PluginCallback plugin_callbacks[] =
{
	{ "update-editor-menu", (GCallback) &sc_gui_update_editor_menu_cb, FALSE, NULL },
	{ "editor-notify", (GCallback) &sc_gui_editor_notify, FALSE, NULL },
	{ "document-open", (GCallback) &sc_gui_document_open_cb, FALSE, NULL },
	{ "document-reload", (GCallback) &sc_gui_document_open_cb, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


static void populate_dict_combo(GtkComboBox *combo)
{
	guint i;
	GtkTreeModel *model = gtk_combo_box_get_model(combo);

	gtk_list_store_clear(GTK_LIST_STORE(model));
	for (i = 0; i < sc_info->dicts->len; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), g_ptr_array_index(sc_info->dicts, i));

		if (utils_str_equal(g_ptr_array_index(sc_info->dicts, i), sc_info->default_language))
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
	}
	/* if the default language couldn't be selected, select the first available language */
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) == -1)
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
}


static void configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(sc_info->config_file);
		GtkComboBox *combo = GTK_COMBO_BOX(g_object_get_data(G_OBJECT(dialog), "combo"));

		setptr(sc_info->default_language, gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo)));
#ifdef HAVE_ENCHANT_1_5
		setptr(sc_info->dictionary_dir, g_strdup(gtk_entry_get_text(GTK_ENTRY(
			g_object_get_data(G_OBJECT(dialog), "dict_dir")))));
#endif
		sc_speller_reinit_enchant_dict();

		sc_info->check_while_typing = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_type"))));

		sc_info->check_on_document_open = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_on_open"))));

		sc_info->use_msgwin = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_msgwin"))));

		sc_info->show_toolbar_item = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_toolbar"))));

		sc_info->show_editor_menu_item = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_editor_menu"))));

		sc_info->show_editor_menu_item_sub_menu = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_editor_menu_sub_menu"))));

		g_key_file_load_from_file(config, sc_info->config_file, G_KEY_FILE_NONE, NULL);
		if (sc_info->default_language != NULL) /* lang may be NULL */
			g_key_file_set_string(config, "spellcheck", "language", sc_info->default_language);
		g_key_file_set_boolean(config, "spellcheck", "check_while_typing",
			sc_info->check_while_typing);
		g_key_file_set_boolean(config, "spellcheck", "check_on_document_open",
			sc_info->check_on_document_open);
		g_key_file_set_boolean(config, "spellcheck", "use_msgwin",
			sc_info->use_msgwin);
		g_key_file_set_boolean(config, "spellcheck", "show_toolbar_item",
			sc_info->show_toolbar_item);
		g_key_file_set_boolean(config, "spellcheck", "show_editor_menu_item",
			sc_info->show_editor_menu_item);
		g_key_file_set_boolean(config, "spellcheck", "show_editor_menu_item_sub_menu",
			sc_info->show_editor_menu_item_sub_menu);
		if (sc_info->dictionary_dir != NULL)
			g_key_file_set_string(config, "spellcheck", "dictionary_dir",
				sc_info->dictionary_dir);

		sc_gui_recreate_editor_menu();
		sc_gui_update_toolbar();
		sc_gui_update_menu();
		populate_dict_combo(combo);

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
	GeanyKeyGroup *key_group;
	GKeyFile *config = g_key_file_new();
	gchar *default_lang;

	default_lang = sc_speller_get_default_lang();
	sc_info = g_new0(SpellCheck, 1);

	sc_info->config_file = g_strconcat(geany->app->configdir,
		G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"spellcheck", G_DIR_SEPARATOR_S, "spellcheck.conf", NULL);

	g_key_file_load_from_file(config, sc_info->config_file, G_KEY_FILE_NONE, NULL);
	sc_info->default_language = utils_get_setting_string(config,
		"spellcheck", "language", default_lang);
	sc_info->check_while_typing = utils_get_setting_boolean(config,
		"spellcheck", "check_while_typing", FALSE);
	sc_info->check_on_document_open = utils_get_setting_boolean(config,
		"spellcheck", "check_on_document_open", FALSE);
	sc_info->show_toolbar_item = utils_get_setting_boolean(config,
		"spellcheck", "show_toolbar_item", TRUE);
	sc_info->show_editor_menu_item = utils_get_setting_boolean(config,
		"spellcheck", "show_editor_menu_item", TRUE);
	sc_info->show_editor_menu_item_sub_menu = utils_get_setting_boolean(config,
		"spellcheck", "show_editor_menu_item_sub_menu", TRUE);
	sc_info->dictionary_dir = utils_get_setting_string(config,
		"spellcheck", "dictionary_dir", NULL);
	sc_info->use_msgwin = utils_get_setting_boolean(config, "spellcheck", "use_msgwin", FALSE);
	g_key_file_free(config);
	g_free(default_lang);

	sc_info->menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_SPELL_CHECK, NULL);
	ui_add_document_sensitive(sc_info->menu_item);

	sc_gui_update_toolbar();

	sc_gui_init();
	sc_speller_init();

	sc_gui_update_menu();
	gtk_widget_show_all(sc_info->menu_item);

	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "spellcheck", KB_COUNT, NULL);
	keybindings_set_item(key_group, KB_SPELL_CHECK, sc_gui_kb_run_activate_cb,
		0, 0, "spell_check", _("Run Spell Check"), sc_info->submenu_item_default);
	keybindings_set_item(key_group, KB_SPELL_TOOGLE_TYPING,
		sc_gui_kb_toggle_typing_activate_cb, 0, 0, "spell_toggle_typing",
		_("Toggle Check While Typing"), NULL);
}


#ifdef HAVE_ENCHANT_1_5
static void dictionary_dir_button_clicked_cb(GtkButton *button, gpointer item)
{
	GtkWidget *dialog;
	gchar *text;

	/* initialise the dialog */
	dialog = gtk_file_chooser_dialog_new(_("Select Directory"), NULL,
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	text = utils_get_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(item)));
	if (!EMPTY(text))
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


static void configure_frame_editor_menu_toggled_cb(GtkToggleButton *togglebutton, gpointer data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data),
		"check_editor_menu_sub_menu"), sens);
}


GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *label_language, *label_dir, *vbox;
	GtkWidget *combo, *check_type, *check_on_open, *check_msgwin, *check_toolbar;
	GtkWidget *frame_editor_menu, *check_editor_menu;
	GtkWidget *check_editor_menu_sub_menu, *align_editor_menu_sub_menu;
	GtkWidget *vbox_interface, *frame_interface, *label_interface;
	GtkWidget *vbox_behavior, *frame_behavior, *label_behavior;
#ifdef HAVE_ENCHANT_1_5
	GtkWidget *entry_dir, *hbox, *button, *image;
#endif

	vbox = gtk_vbox_new(FALSE, 6);

	check_toolbar = gtk_check_button_new_with_label(
		_("Show toolbar item to toggle spell checking"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_toolbar), sc_info->show_toolbar_item);

	check_editor_menu = gtk_check_button_new_with_label(
		_("Show editor menu item to show spelling suggestions"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_editor_menu),
		sc_info->show_editor_menu_item);

	check_editor_menu_sub_menu = gtk_check_button_new_with_label(
		_("Show suggestions in a sub menu of the editor menu"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_editor_menu_sub_menu),
		sc_info->show_editor_menu_item_sub_menu);
	align_editor_menu_sub_menu = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align_editor_menu_sub_menu), 0, 0, 9, 0);
	gtk_container_add(GTK_CONTAINER(align_editor_menu_sub_menu), check_editor_menu_sub_menu);

	frame_editor_menu = gtk_frame_new(NULL);
	gtk_frame_set_label_widget(GTK_FRAME(frame_editor_menu), check_editor_menu);
	gtk_container_set_border_width(GTK_CONTAINER(frame_editor_menu), 3);
	gtk_container_add(GTK_CONTAINER(frame_editor_menu), align_editor_menu_sub_menu);
	g_signal_connect(check_editor_menu, "toggled",
		G_CALLBACK(configure_frame_editor_menu_toggled_cb), dialog);

	check_msgwin = gtk_check_button_new_with_label(
		_("Print misspelled words and suggestions in the messages window"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_msgwin), sc_info->use_msgwin);

	vbox_interface = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_interface), check_toolbar, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_interface), frame_editor_menu, TRUE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_interface), check_msgwin, TRUE, TRUE, 3);

	label_interface = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(label_interface), TRUE);
	gtk_label_set_markup(GTK_LABEL(label_interface), _("<b>Interface</b>"));

	frame_interface = gtk_frame_new(NULL);
	gtk_frame_set_label_widget(GTK_FRAME(frame_interface), label_interface);
	gtk_container_add(GTK_CONTAINER(frame_interface), vbox_interface);
	gtk_box_pack_start(GTK_BOX(vbox), frame_interface, FALSE, FALSE, 3);


	check_type = gtk_check_button_new_with_label(_("Check spelling while typing"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_type), sc_info->check_while_typing);

	check_on_open = gtk_check_button_new_with_label(_("Check spelling when opening a document"));
	gtk_widget_set_tooltip_text(check_on_open,
		_("Enabling this option will check every document after it is opened in Geany. "
		  "Reloading a document will also trigger a re-check."));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_on_open), sc_info->check_on_document_open);

	label_language = gtk_label_new(_("Language to use for the spell check:"));
	gtk_misc_set_alignment(GTK_MISC(label_language), 0, 0.5);

	combo = gtk_combo_box_text_new();
	populate_dict_combo(GTK_COMBO_BOX(combo));

	if (sc_info->dicts->len > 20)
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 3);
	else if (sc_info->dicts->len > 10)
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 2);

#ifdef HAVE_ENCHANT_1_5
	label_dir = gtk_label_new_with_mnemonic(_("_Directory to look for dictionary files:"));
	gtk_misc_set_alignment(GTK_MISC(label_dir), 0, 0.5);

	entry_dir = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(entry_dir));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_dir), entry_dir);
	gtk_widget_set_tooltip_text(entry_dir,
		_("Read additional dictionary files from this directory. "
		  "For now, this only works with myspell dictionaries."));
	if (! EMPTY(sc_info->dictionary_dir))
		gtk_entry_set_text(GTK_ENTRY(entry_dir), sc_info->dictionary_dir);

	button = gtk_button_new();
	g_signal_connect(button, "clicked",
		G_CALLBACK(dictionary_dir_button_clicked_cb), entry_dir);

	image = gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), entry_dir, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	g_object_set_data(G_OBJECT(dialog), "dict_dir", entry_dir);
#endif

	vbox_behavior = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_behavior), check_type, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_behavior), check_on_open, TRUE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_behavior), label_language, TRUE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_behavior), combo, TRUE, TRUE, 3);
#ifdef HAVE_ENCHANT_1_5
	gtk_box_pack_start(GTK_BOX(vbox_behavior), label_dir, TRUE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_behavior), hbox, TRUE, TRUE, 3);
#endif

	label_behavior = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(label_behavior), TRUE);
	gtk_label_set_markup(GTK_LABEL(label_behavior), _("<b>Behavior</b>"));

	frame_behavior = gtk_frame_new(NULL);
	gtk_frame_set_label_widget(GTK_FRAME(frame_behavior), label_behavior);
	gtk_container_add(GTK_CONTAINER(frame_behavior), vbox_behavior);
	gtk_box_pack_start(GTK_BOX(vbox), frame_behavior, FALSE, FALSE, 3);

	g_object_set_data(G_OBJECT(dialog), "combo", combo);
	g_object_set_data(G_OBJECT(dialog), "check_type", check_type);
	g_object_set_data(G_OBJECT(dialog), "check_on_open", check_on_open);
	g_object_set_data(G_OBJECT(dialog), "check_msgwin", check_msgwin);
	g_object_set_data(G_OBJECT(dialog), "check_toolbar", check_toolbar);
	g_object_set_data(G_OBJECT(dialog), "check_editor_menu", check_editor_menu);
	g_object_set_data(G_OBJECT(dialog), "check_editor_menu_sub_menu", check_editor_menu_sub_menu);
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);

	configure_frame_editor_menu_toggled_cb(GTK_TOGGLE_BUTTON(check_editor_menu), dialog);
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
	utils_open_browser("http://plugins.geany.org/spellcheck.html");
}


void plugin_cleanup(void)
{
	sc_gui_free();
	sc_speller_free();

	g_free(sc_info->dictionary_dir);
	g_free(sc_info->default_language);
	g_free(sc_info->config_file);
	gtk_widget_destroy(sc_info->menu_item);
	g_free(sc_info);
}
