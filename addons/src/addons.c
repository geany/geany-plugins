/*
 *      addons.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#include "geanyplugin.h"

#include "addons.h"
#include "ao_blanklines.h"
#include "ao_doclist.h"
#include "ao_openuri.h"
#include "ao_systray.h"
#include "ao_bookmarklist.h"
#include "ao_markword.h"
#include "ao_tasks.h"
#include "ao_xmltagging.h"
#include "ao_wrapwords.h"


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(209)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Addons"),
	_("Various small addons for Geany."),
	VERSION,
	"Enrico Tröger, Bert Vermeulen, Eugene Arshinov, Frank Lanitz")


/* Keybinding(s) */
enum
{
	KB_FOCUS_BOOKMARK_LIST,
	KB_FOCUS_TASKS,
	KB_UPDATE_TASKS,
	KB_XMLTAGGING,
	KB_COUNT
};


typedef struct
{
	/* general settings */
	gchar *config_file;
	gboolean enable_doclist;
	gboolean enable_openuri;
	gboolean enable_tasks;
	gboolean enable_systray;
	gboolean enable_bookmarklist;
	gboolean enable_markword;
	gboolean enable_xmltagging;
	gboolean enable_wrapwords;
	gboolean enable_wrapwords_auto;
	gboolean strip_trailing_blank_lines;

	gchar *tasks_token_list;
	gboolean tasks_scan_all_documents;

	DocListSortMode doclist_sort_mode;

	/* instances and variables of components */
	AoDocList *doclist;
	AoOpenUri *openuri;
	AoSystray *systray;
	AoBookmarkList *bookmarklist;
	AoMarkWord *markword;
	AoTasks *tasks;
} AddonsInfo;
static AddonsInfo *ao_info = NULL;



static void ao_update_editor_menu_cb(GObject *obj, const gchar *word, gint pos,
									 GeanyDocument *doc, gpointer data);
static void ao_document_activate_cb(GObject *obj, GeanyDocument *doc, gpointer data);
static void ao_document_open_cb(GObject *obj, GeanyDocument *doc, gpointer data);
static void ao_document_save_cb(GObject *obj, GeanyDocument *doc, gpointer data);
static void ao_document_before_save_cb(GObject *obj, GeanyDocument *doc, gpointer data);
static void ao_document_close_cb(GObject *obj, GeanyDocument *doc, gpointer data);
static void ao_document_reload_cb(GObject *obj, GeanyDocument *doc, gpointer data);
static void ao_startup_complete_cb(GObject *obj, gpointer data);

gboolean ao_editor_notify_cb(GObject *object, GeanyEditor *editor,
	SCNotification *nt, gpointer data);


PluginCallback plugin_callbacks[] =
{
	{ "update-editor-menu", (GCallback) &ao_update_editor_menu_cb, FALSE, NULL },
	{ "editor-notify", (GCallback) &ao_editor_notify_cb, TRUE, NULL },

	{ "document-open", (GCallback) &ao_document_open_cb, TRUE, NULL },
	{ "document-save", (GCallback) &ao_document_save_cb, TRUE, NULL },
	{ "document-close", (GCallback) &ao_document_close_cb, TRUE, NULL },
	{ "document-activate", (GCallback) &ao_document_activate_cb, TRUE, NULL },
	{ "document-before-save", (GCallback) &ao_document_before_save_cb, TRUE, NULL },
	{ "document-reload", (GCallback) &ao_document_reload_cb, TRUE, NULL },

	{ "geany-startup-complete", (GCallback) &ao_startup_complete_cb, TRUE, NULL },

	{ NULL, NULL, FALSE, NULL }
};


static void ao_startup_complete_cb(GObject *obj, gpointer data)
{
	ao_tasks_set_active(ao_info->tasks);
}


static void kb_bmlist_activate(guint key_id)
{
	ao_bookmark_list_activate(ao_info->bookmarklist);
}


static void kb_tasks_activate(guint key_id)
{
	ao_tasks_activate(ao_info->tasks);
}


static void kb_tasks_update(guint key_id)
{
	ao_tasks_update(ao_info->tasks, NULL);
}

static void kb_ao_xmltagging(guint key_id)
{
	if (ao_info->enable_xmltagging == TRUE)
	{
		ao_xmltagging();
	}
}

gboolean ao_editor_notify_cb(GObject *object, GeanyEditor *editor,
							 SCNotification *nt, gpointer data)
{
	ao_bookmark_list_update_marker(ao_info->bookmarklist, editor, nt);
	ao_mark_word_check(ao_info->markword, editor, nt);

	return FALSE;
}


static void ao_update_editor_menu_cb(GObject *obj, const gchar *word, gint pos,
									 GeanyDocument *doc, gpointer data)
{
	g_return_if_fail(doc != NULL && doc->is_valid);

	ao_open_uri_update_menu(ao_info->openuri, doc, pos);
}


static void ao_document_activate_cb(GObject *obj, GeanyDocument *doc, gpointer data)
{
	g_return_if_fail(doc != NULL && doc->is_valid);

	ao_bookmark_list_update(ao_info->bookmarklist, doc);
	ao_tasks_update_single(ao_info->tasks, doc);
}


static void ao_document_open_cb(GObject *obj, GeanyDocument *doc, gpointer data)
{
	g_return_if_fail(doc != NULL && doc->is_valid);

	ao_tasks_update(ao_info->tasks, doc);
}


static void ao_document_close_cb(GObject *obj, GeanyDocument *doc, gpointer data)
{
	g_return_if_fail(doc != NULL && doc->is_valid);

	ao_tasks_remove(ao_info->tasks, doc);
}


static void ao_document_save_cb(GObject *obj, GeanyDocument *doc, gpointer data)
{
	g_return_if_fail(doc != NULL && doc->is_valid);

	ao_tasks_update(ao_info->tasks, doc);
}


static void ao_document_before_save_cb(GObject *obj, GeanyDocument *doc, gpointer data)
{
	g_return_if_fail(doc != NULL && doc->is_valid);

	ao_blanklines_on_document_before_save(obj, doc, data);
}


static void ao_document_reload_cb(GObject *obj, GeanyDocument *doc, gpointer data)
{
	g_return_if_fail(doc != NULL && doc->is_valid);

	ao_tasks_update(ao_info->tasks, doc);
}


GtkWidget *ao_image_menu_item_new(const gchar *stock_id, const gchar *label)
{
	GtkWidget *item = gtk_image_menu_item_new_with_label(label);
	GtkWidget *image = gtk_image_new_from_icon_name(stock_id, GTK_ICON_SIZE_MENU);

	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(image);
	return item;
}


void plugin_init(GeanyData *data)
{
	GKeyFile *config = g_key_file_new();
	GeanyKeyGroup *key_group;

	ao_info = g_new0(AddonsInfo, 1);

	ao_info->config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S,
		"plugins", G_DIR_SEPARATOR_S, "addons", G_DIR_SEPARATOR_S, "addons.conf", NULL);

	g_key_file_load_from_file(config, ao_info->config_file, G_KEY_FILE_NONE, NULL);
	ao_info->enable_doclist = utils_get_setting_boolean(config,
		"addons", "show_toolbar_doclist_item", TRUE);
	ao_info->doclist_sort_mode = utils_get_setting_integer(config,
		"addons", "doclist_sort_mode", DOCLIST_SORT_BY_TAB_ORDER);
	ao_info->enable_openuri = utils_get_setting_boolean(config,
		"addons", "enable_openuri", FALSE);
	ao_info->enable_tasks = utils_get_setting_boolean(config,
		"addons", "enable_tasks", TRUE);
	ao_info->tasks_scan_all_documents = utils_get_setting_boolean(config,
		"addons", "tasks_scan_all_documents", FALSE);
	ao_info->tasks_token_list = utils_get_setting_string(config,
		"addons", "tasks_token_list", "TODO;FIXME");
	ao_info->enable_systray = utils_get_setting_boolean(config,
		"addons", "enable_systray", FALSE);
	ao_info->enable_bookmarklist = utils_get_setting_boolean(config,
		"addons", "enable_bookmarklist", FALSE);
	ao_info->enable_markword = utils_get_setting_boolean(config,
		"addons", "enable_markword", FALSE);
	ao_info->strip_trailing_blank_lines = utils_get_setting_boolean(config,
		"addons", "strip_trailing_blank_lines", FALSE);
	ao_info->enable_xmltagging = utils_get_setting_boolean(config, "addons",
		"enable_xmltagging", FALSE);
	ao_info->enable_wrapwords = utils_get_setting_boolean(config, "addons",
		"enable_wrapwords", FALSE);
	ao_info->enable_wrapwords_auto = utils_get_setting_boolean(config, "addons",
		"enable_wrapwords_auto", FALSE);

	plugin_module_make_resident(geany_plugin);

	ao_info->doclist = ao_doc_list_new(ao_info->enable_doclist, ao_info->doclist_sort_mode);
	ao_info->openuri = ao_open_uri_new(ao_info->enable_openuri);
	ao_info->systray = ao_systray_new(ao_info->enable_systray);
	ao_info->bookmarklist = ao_bookmark_list_new(ao_info->enable_bookmarklist);
	ao_info->markword = ao_mark_word_new(ao_info->enable_markword);
	ao_info->tasks = ao_tasks_new(ao_info->enable_tasks,
						ao_info->tasks_token_list, ao_info->tasks_scan_all_documents);

	ao_blanklines_set_enable(ao_info->strip_trailing_blank_lines);

	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "addons", KB_COUNT+8, NULL);
	keybindings_set_item(key_group, KB_FOCUS_BOOKMARK_LIST, kb_bmlist_activate,
		0, 0, "focus_bookmark_list", _("Focus Bookmark List"), NULL);
	keybindings_set_item(key_group, KB_FOCUS_TASKS, kb_tasks_activate,
		0, 0, "focus_tasks", _("Focus Tasks List"), NULL);
	keybindings_set_item(key_group, KB_UPDATE_TASKS, kb_tasks_update,
		0, 0, "update_tasks", _("Update Tasks List"), NULL);
	keybindings_set_item(key_group, KB_XMLTAGGING, kb_ao_xmltagging,
		0, 0, "xml_tagging", _("Run XML tagging"), NULL);

	ao_wrapwords_init(ao_info->config_file, key_group);
	ao_wrapwords_set_enabled (ao_info->enable_wrapwords, ao_info->enable_wrapwords_auto);

	g_key_file_free(config);
}


static void ao_configure_tasks_toggled_cb(GtkToggleButton *togglebutton, gpointer data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data), "check_tasks_scan_mode"), sens);
	gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data), "entry_tasks_tokens"), sens);
}


static void ao_configure_doclist_toggled_cb(GtkToggleButton *togglebutton, gpointer data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data),
		"radio_doclist_name"), sens);
	gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data),
		"radio_doclist_tab_order"), sens);
	gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data),
		"radio_doclist_tab_order_reversed"), sens);
}


static void ao_configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(ao_info->config_file);

		ao_info->enable_doclist = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_doclist"))));
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(
				G_OBJECT(dialog), "radio_doclist_name"))))
			ao_info->doclist_sort_mode = DOCLIST_SORT_BY_NAME;
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(
				G_OBJECT(dialog), "radio_doclist_tab_order_reversed"))))
			ao_info->doclist_sort_mode = DOCLIST_SORT_BY_TAB_ORDER_REVERSE;
		else
			ao_info->doclist_sort_mode = DOCLIST_SORT_BY_TAB_ORDER;

		ao_info->enable_openuri = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_openuri"))));
		ao_info->enable_tasks = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_tasks"))));
		ao_info->tasks_scan_all_documents = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_tasks_scan_mode"))));
		g_free(ao_info->tasks_token_list);
		ao_info->tasks_token_list = g_strdup(gtk_entry_get_text(GTK_ENTRY(
			g_object_get_data(G_OBJECT(dialog), "entry_tasks_tokens"))));
		ao_info->enable_systray = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_systray"))));
		ao_info->enable_bookmarklist = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_bookmarklist"))));
		ao_info->enable_markword = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_markword"))));
		ao_info->strip_trailing_blank_lines = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_blanklines"))));
		ao_info->enable_xmltagging = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_xmltagging"))));
		ao_info->enable_wrapwords = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_wrapwords"))));
		ao_info->enable_wrapwords_auto = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_wrapwords_auto"))));

		ao_wrapwords_set_enabled (ao_info->enable_wrapwords, ao_info->enable_wrapwords_auto);

		g_key_file_load_from_file(config, ao_info->config_file, G_KEY_FILE_NONE, NULL);
		g_key_file_set_boolean(config, "addons",
			"show_toolbar_doclist_item", ao_info->enable_doclist);
		g_key_file_set_integer(config, "addons", "doclist_sort_mode", ao_info->doclist_sort_mode);
		g_key_file_set_boolean(config, "addons", "enable_openuri", ao_info->enable_openuri);
		g_key_file_set_boolean(config, "addons", "enable_tasks", ao_info->enable_tasks);
		g_key_file_set_string(config, "addons", "tasks_token_list", ao_info->tasks_token_list);
		g_key_file_set_boolean(config, "addons", "tasks_scan_all_documents",
			ao_info->tasks_scan_all_documents);
		g_key_file_set_boolean(config, "addons", "enable_systray", ao_info->enable_systray);
		g_key_file_set_boolean(config, "addons", "enable_bookmarklist",
			ao_info->enable_bookmarklist);
		g_key_file_set_boolean(config, "addons", "enable_markword", ao_info->enable_markword);
		g_key_file_set_boolean(config, "addons", "strip_trailing_blank_lines",
		  ao_info->strip_trailing_blank_lines);
		g_key_file_set_boolean(config, "addons", "enable_xmltagging",
			ao_info->enable_xmltagging);
		g_key_file_set_boolean(config, "addons", "enable_wrapwords",
			ao_info->enable_wrapwords);
		g_key_file_set_boolean(config, "addons", "enable_wrapwords_auto",
			ao_info->enable_wrapwords_auto);

		g_object_set(ao_info->doclist, "enable-doclist", ao_info->enable_doclist, NULL);
		g_object_set(ao_info->doclist, "sort-mode", ao_info->doclist_sort_mode, NULL);
		g_object_set(ao_info->openuri, "enable-openuri", ao_info->enable_openuri, NULL);
		g_object_set(ao_info->systray, "enable-systray", ao_info->enable_systray, NULL);
		g_object_set(ao_info->bookmarklist, "enable-bookmarklist",
			ao_info->enable_bookmarklist, NULL);
		g_object_set(ao_info->markword, "enable-markword", ao_info->enable_markword, NULL);
		g_object_set(ao_info->tasks,
			"enable-tasks", ao_info->enable_tasks,
			"scan-all-documents", ao_info->tasks_scan_all_documents,
			"tokens", ao_info->tasks_token_list,
			NULL);
		ao_blanklines_set_enable(ao_info->strip_trailing_blank_lines);

		if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Plugin configuration directory could not be created."));
		}
		else
		{
			/* write config to file */
			data = g_key_file_to_data(config, NULL, NULL);
			utils_write_file(ao_info->config_file, data);
			g_free(data);
		}
		g_free(config_dir);
		g_key_file_free(config);
	}
}


GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox, *check_openuri, *check_tasks, *check_systray;
	GtkWidget *check_doclist, *vbox_doclist, *frame_doclist;
	GtkWidget *radio_doclist_name, *radio_doclist_tab_order, *radio_doclist_tab_order_reversed;
	GtkWidget *check_bookmarklist, *check_markword, *frame_tasks, *vbox_tasks;
	GtkWidget *check_tasks_scan_mode, *entry_tasks_tokens, *label_tasks_tokens, *tokens_hbox;
	GtkWidget *check_blanklines, *check_xmltagging;
	GtkWidget *check_wrapwords, *check_wrapwords_auto, *wrapwords_config_button, *wrapwords_hbox;

	vbox = gtk_vbox_new(FALSE, 6);

	check_doclist = gtk_check_button_new_with_label(
		_("Show toolbar item to show a list of currently open documents"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_doclist), ao_info->enable_doclist);
	g_signal_connect(check_doclist, "toggled", G_CALLBACK(ao_configure_doclist_toggled_cb), dialog);

	radio_doclist_name = gtk_radio_button_new_with_mnemonic(NULL, _("Sort documents by _name"));
	ui_widget_set_tooltip_text(radio_doclist_name,
		_("Sort the documents in the list by their filename"));

	radio_doclist_tab_order = gtk_radio_button_new_with_mnemonic_from_widget(
		GTK_RADIO_BUTTON(radio_doclist_name), _("Sort documents by _occurrence"));
	ui_widget_set_tooltip_text(radio_doclist_tab_order,
		_("Sort the documents in the order of the document tabs"));

	radio_doclist_tab_order_reversed = gtk_radio_button_new_with_mnemonic_from_widget(
		GTK_RADIO_BUTTON(radio_doclist_name), _("Sort documents by _occurrence (reversed)"));
	ui_widget_set_tooltip_text(radio_doclist_tab_order_reversed,
		_("Sort the documents in the order of the document tabs (reversed)"));

	switch (ao_info->doclist_sort_mode)
	{
		case DOCLIST_SORT_BY_NAME:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_doclist_name), TRUE);
			break;
		case DOCLIST_SORT_BY_TAB_ORDER_REVERSE:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_doclist_tab_order_reversed), TRUE);
			break;
		case DOCLIST_SORT_BY_TAB_ORDER:
		default:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_doclist_tab_order), TRUE);
			break;
	}

	vbox_doclist = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_doclist), radio_doclist_name, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_doclist), radio_doclist_tab_order, TRUE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_doclist), radio_doclist_tab_order_reversed, TRUE, TRUE, 3);

	frame_doclist = gtk_frame_new(NULL);
	gtk_frame_set_label_widget(GTK_FRAME(frame_doclist), check_doclist);
	gtk_container_add(GTK_CONTAINER(frame_doclist), vbox_doclist);
	gtk_box_pack_start(GTK_BOX(vbox), frame_doclist, FALSE, FALSE, 3);

	check_openuri = gtk_check_button_new_with_label(
		/* TODO fix the string */
		_("Show a 'Open URI' menu item in the editor menu"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_openuri),
		ao_info->enable_openuri);
	gtk_box_pack_start(GTK_BOX(vbox), check_openuri, FALSE, FALSE, 3);

	check_tasks = gtk_check_button_new_with_label(
		_("Show available Tasks in the Messages Window"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_tasks),
		ao_info->enable_tasks);
	g_signal_connect(check_tasks, "toggled", G_CALLBACK(ao_configure_tasks_toggled_cb), dialog);

	check_tasks_scan_mode = gtk_check_button_new_with_label(
		_("Show tasks of all documents"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_tasks_scan_mode),
		ao_info->tasks_scan_all_documents);
	ui_widget_set_tooltip_text(check_tasks_scan_mode,
		_("Whether to show the tasks of all open documents in the list or only those of the current document."));

	entry_tasks_tokens = gtk_entry_new();
	if (NZV(ao_info->tasks_token_list))
		gtk_entry_set_text(GTK_ENTRY(entry_tasks_tokens), ao_info->tasks_token_list);
	ui_entry_add_clear_icon(GTK_ENTRY(entry_tasks_tokens));
	ui_widget_set_tooltip_text(entry_tasks_tokens,
		_("Specify a semicolon separated list of search tokens."));

	label_tasks_tokens = gtk_label_new_with_mnemonic(_("Search tokens:"));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_tasks_tokens), entry_tasks_tokens);

	tokens_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(tokens_hbox), label_tasks_tokens, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(tokens_hbox), entry_tasks_tokens, TRUE, TRUE, 3);

	vbox_tasks = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_tasks), check_tasks_scan_mode, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox_tasks), tokens_hbox, TRUE, TRUE, 3);

	frame_tasks = gtk_frame_new(NULL);
	gtk_frame_set_label_widget(GTK_FRAME(frame_tasks), check_tasks);
	gtk_container_add(GTK_CONTAINER(frame_tasks), vbox_tasks);
	gtk_box_pack_start(GTK_BOX(vbox), frame_tasks, FALSE, FALSE, 3);

	check_systray = gtk_check_button_new_with_label(
		_("Show status icon in the Notification Area"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_systray),
		ao_info->enable_systray);
	gtk_box_pack_start(GTK_BOX(vbox), check_systray, FALSE, FALSE, 3);

	check_bookmarklist = gtk_check_button_new_with_label(
		_("Show defined bookmarks (marked lines) in the sidebar"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_bookmarklist),
		ao_info->enable_bookmarklist);
	gtk_box_pack_start(GTK_BOX(vbox), check_bookmarklist, FALSE, FALSE, 3);

	check_markword = gtk_check_button_new_with_label(
		_("Mark all occurrences of a word when double-clicking it"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_markword),
		ao_info->enable_markword);
	gtk_box_pack_start(GTK_BOX(vbox), check_markword, FALSE, FALSE, 3);

	check_blanklines = gtk_check_button_new_with_label(
		_("Strip trailing blank lines"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_blanklines),
		ao_info->strip_trailing_blank_lines);
	gtk_box_pack_start(GTK_BOX(vbox), check_blanklines, FALSE, FALSE, 3);

	check_xmltagging = gtk_check_button_new_with_label(
		_("XML tagging for selection"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_xmltagging),
		ao_info->enable_xmltagging);
	gtk_box_pack_start(GTK_BOX(vbox), check_xmltagging, FALSE, FALSE, 3);

	check_wrapwords = gtk_check_button_new_with_label(
		_("Wrap selection on configurable keybindings"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_wrapwords),
		ao_info->enable_wrapwords);

	wrapwords_config_button = gtk_button_new_with_label ("Configure wrap pairs");
	g_signal_connect(wrapwords_config_button, "clicked", G_CALLBACK(ao_wrapwords_config), dialog);
	wrapwords_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(wrapwords_hbox), check_wrapwords, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(wrapwords_hbox), wrapwords_config_button, TRUE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), wrapwords_hbox, FALSE, FALSE, 3);

	check_wrapwords_auto = gtk_check_button_new_with_label(
		_("Wrap selection automatically (without having to press a keybinding)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_wrapwords_auto),
		ao_info->enable_wrapwords_auto);
	gtk_box_pack_start(GTK_BOX(vbox), check_wrapwords_auto, FALSE, FALSE, 3);


	g_object_set_data(G_OBJECT(dialog), "check_doclist", check_doclist);
	g_object_set_data(G_OBJECT(dialog), "radio_doclist_name", radio_doclist_name);
	g_object_set_data(G_OBJECT(dialog), "radio_doclist_tab_order", radio_doclist_tab_order);
	g_object_set_data(G_OBJECT(dialog), "radio_doclist_tab_order_reversed",
		radio_doclist_tab_order_reversed);
	g_object_set_data(G_OBJECT(dialog), "check_openuri", check_openuri);
	g_object_set_data(G_OBJECT(dialog), "check_tasks", check_tasks);
	g_object_set_data(G_OBJECT(dialog), "entry_tasks_tokens", entry_tasks_tokens);
	g_object_set_data(G_OBJECT(dialog), "check_tasks_scan_mode", check_tasks_scan_mode);
	g_object_set_data(G_OBJECT(dialog), "check_systray", check_systray);
	g_object_set_data(G_OBJECT(dialog), "check_bookmarklist", check_bookmarklist);
	g_object_set_data(G_OBJECT(dialog), "check_markword", check_markword);
	g_object_set_data(G_OBJECT(dialog), "check_blanklines", check_blanklines);
	g_object_set_data(G_OBJECT(dialog), "check_xmltagging", check_xmltagging);
	g_object_set_data(G_OBJECT(dialog), "check_wrapwords", check_wrapwords);
	g_object_set_data(G_OBJECT(dialog), "check_wrapwords_auto", check_wrapwords_auto);
	g_object_set_data(G_OBJECT(dialog), "wrapwords_config_button", wrapwords_config_button);
	g_signal_connect(dialog, "response", G_CALLBACK(ao_configure_response_cb), NULL);

	ao_configure_tasks_toggled_cb(GTK_TOGGLE_BUTTON(check_tasks), dialog);
	ao_configure_doclist_toggled_cb(GTK_TOGGLE_BUTTON(check_doclist), dialog);

	gtk_widget_show_all(vbox);

#if ! GTK_CHECK_VERSION(2, 10, 0)
	gtk_widget_hide(check_systray);
#endif

	return vbox;
}


void plugin_cleanup(void)
{
	g_object_unref(ao_info->doclist);
	g_object_unref(ao_info->openuri);
	g_object_unref(ao_info->systray);
	g_object_unref(ao_info->bookmarklist);
	g_object_unref(ao_info->markword);
	g_object_unref(ao_info->tasks);
	g_free(ao_info->tasks_token_list);

	ao_blanklines_set_enable(FALSE);

	g_free(ao_info->config_file);
	g_free(ao_info);
}

