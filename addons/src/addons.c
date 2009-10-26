/*
 *      addons.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#include "ao_doclist.h"
#include "ao_openuri.h"
#include "ao_systray.h"
#include "ao_bookmarklist.h"
#include "tasks.h"



GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(164)
PLUGIN_SET_INFO(_("Addons"), _("Various small addons for Geany."), VERSION,
	"Enrico Tröger, Bert Vermeulen")


typedef struct
{
	/* general settings */
	gchar *config_file;
	gboolean show_toolbar_doclist_item;
	gboolean enable_openuri;
	gboolean enable_tasks;
	gboolean enable_systray;
	gboolean enable_bookmarklist;

	/* instances and variables of components */
	AoDocList *doclist;
	AoOpenUri *openuri;
	AoSystray *systray;
	AoBookmarkList *bookmarklist;
} AddonsInfo;
static AddonsInfo *ao_info = NULL;



static void ao_update_editor_menu_cb(GObject *obj, const gchar *word, gint pos,
									 GeanyDocument *doc, gpointer data);
static void ao_document_activate_cb(GObject *obj, GeanyDocument *doc, gpointer data);
gboolean ao_editor_notify_cb(GObject *object, GeanyEditor *editor,
	SCNotification *nt, gpointer data);


PluginCallback plugin_callbacks[] =
{
    { "update-editor-menu", (GCallback) &ao_update_editor_menu_cb, FALSE, NULL },

	{ "editor-notify", (GCallback) &ao_editor_notify_cb, TRUE, NULL },
	{ "document-open", (GCallback) &tasks_on_document_open, TRUE, NULL },
	{ "document-close", (GCallback) &tasks_on_document_close, TRUE, NULL },
	{ "document-activate", (GCallback) &ao_document_activate_cb, TRUE, NULL },

	{ NULL, NULL, FALSE, NULL }
};


gboolean ao_editor_notify_cb(GObject *object, GeanyEditor *editor,
							 SCNotification *nt, gpointer data)
{
	gboolean ret = FALSE;

	ao_bookmark_list_update_marker(ao_info->bookmarklist, editor, nt);

	ret = tasks_on_editor_notify(object, editor, nt, data);

	return ret;
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

	tasks_on_document_activate(obj, doc, data);
	ao_bookmark_list_update(ao_info->bookmarklist, doc);
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

	ao_info = g_new0(AddonsInfo, 1);

	ao_info->config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S,
		"plugins", G_DIR_SEPARATOR_S, "addons", G_DIR_SEPARATOR_S, "addons.conf", NULL);

	g_key_file_load_from_file(config, ao_info->config_file, G_KEY_FILE_NONE, NULL);
	ao_info->show_toolbar_doclist_item = utils_get_setting_boolean(config,
		"addons", "show_toolbar_doclist_item", TRUE);
	ao_info->enable_openuri = utils_get_setting_boolean(config,
		"addons", "enable_openuri", FALSE);
	ao_info->enable_tasks = utils_get_setting_boolean(config,
		"addons", "enable_tasks", TRUE);
	ao_info->enable_systray = utils_get_setting_boolean(config,
		"addons", "enable_systray", FALSE);
	ao_info->enable_bookmarklist = utils_get_setting_boolean(config,
		"addons", "enable_bookmarklist", FALSE);

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	plugin_module_make_resident(geany_plugin);

	ao_info->doclist = ao_doc_list_new(ao_info->show_toolbar_doclist_item);
	ao_info->openuri = ao_open_uri_new(ao_info->enable_openuri);
	ao_info->systray = ao_systray_new(ao_info->enable_systray);
	ao_info->bookmarklist = ao_bookmark_list_new(ao_info->enable_bookmarklist);

	tasks_set_enable(ao_info->enable_tasks);
}


static void ao_configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(ao_info->config_file);

		ao_info->show_toolbar_doclist_item = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_doclist"))));
		ao_info->enable_openuri = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_openuri"))));
		ao_info->enable_tasks = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_tasks"))));
		ao_info->enable_systray = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_systray"))));
		ao_info->enable_bookmarklist = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			g_object_get_data(G_OBJECT(dialog), "check_bookmarklist"))));

		g_key_file_load_from_file(config, ao_info->config_file, G_KEY_FILE_NONE, NULL);
		g_key_file_set_boolean(config, "addons",
			"show_toolbar_doclist_item", ao_info->show_toolbar_doclist_item);
		g_key_file_set_boolean(config, "addons", "enable_openuri", ao_info->enable_openuri);
		g_key_file_set_boolean(config, "addons", "enable_tasks", ao_info->enable_tasks);
		g_key_file_set_boolean(config, "addons", "enable_systray", ao_info->enable_systray);
		g_key_file_set_boolean(config, "addons", "enable_bookmarklist",
			ao_info->enable_bookmarklist);

		g_object_set(ao_info->doclist, "enable-doclist", ao_info->show_toolbar_doclist_item, NULL);
		g_object_set(ao_info->openuri, "enable-openuri", ao_info->enable_openuri, NULL);
		g_object_set(ao_info->systray, "enable-systray", ao_info->enable_systray, NULL);
		g_object_set(ao_info->bookmarklist, "enable-bookmarklist",
			ao_info->enable_bookmarklist, NULL);
		tasks_set_enable(ao_info->enable_tasks);

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
	GtkWidget *vbox, *check_doclist, *check_openuri, *check_tasks, *check_systray;
	GtkWidget *check_bookmarklist;

	vbox = gtk_vbox_new(FALSE, 6);

	check_doclist = gtk_check_button_new_with_label(
		_("Show toolbar item to show a list of currently open documents"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_doclist),
		ao_info->show_toolbar_doclist_item);
	gtk_box_pack_start(GTK_BOX(vbox), check_doclist, FALSE, FALSE, 3);

	check_openuri = gtk_check_button_new_with_label(
		/* TODO fix the string */
		_("Show a 'Open URI' menu item in the editor menu"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_openuri),
		ao_info->enable_openuri);
	gtk_box_pack_start(GTK_BOX(vbox), check_openuri, FALSE, FALSE, 3);

	check_tasks = gtk_check_button_new_with_label(
		_("Show available tasks in the Message Window"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_tasks),
		ao_info->enable_tasks);
	gtk_box_pack_start(GTK_BOX(vbox), check_tasks, FALSE, FALSE, 3);

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

	g_object_set_data(G_OBJECT(dialog), "check_doclist", check_doclist);
	g_object_set_data(G_OBJECT(dialog), "check_openuri", check_openuri);
	g_object_set_data(G_OBJECT(dialog), "check_tasks", check_tasks);
	g_object_set_data(G_OBJECT(dialog), "check_systray", check_systray);
	g_object_set_data(G_OBJECT(dialog), "check_bookmarklist", check_bookmarklist);
	g_signal_connect(dialog, "response", G_CALLBACK(ao_configure_response_cb), NULL);

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

	tasks_set_enable(FALSE);

	g_free(ao_info->config_file);
	g_free(ao_info);
}
