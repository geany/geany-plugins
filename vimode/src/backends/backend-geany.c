/*
 * Copyright (C) 2018 Jiri Techet <techet@gmail.com>
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

#include "vi.h"

#include <gtk/gtk.h>
#include <geanyplugin.h>

#define CONF_GROUP "Settings"
#define CONF_ENABLE_VIM "enable_vim"
#define CONF_START_IN_INSERT "start_in_insert"
#define CONF_INSERT_FOR_DUMMIES "insert_for_dummies"

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

PLUGIN_VERSION_CHECK(235)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Vimode"),
	_("Vim mode for Geany"),
	VERSION,
	"Jiří Techet <techet@gmail.com>"
)

enum
{
	KB_ENABLE_VIM,
	KB_INSERT_FOR_DUMMIES,
	KB_COUNT
};

struct
{
	GtkWidget *parent_item;
	GtkWidget *enable_vim_item;
	GtkWidget *insert_for_dummies_item;
	GtkWidget *start_in_insert_item;
} menu_items =
{
	NULL, NULL, NULL, NULL
};

static gboolean start_in_insert;
static ViCallback cb;


static gchar *get_config_filename(void)
{
	return g_build_filename(geany_data->app->configdir, "plugins", PLUGIN, PLUGIN".conf", NULL);
}


static void load_config(void)
{
	gchar *filename = get_config_filename();
	GKeyFile *kf = g_key_file_new();

	if (g_key_file_load_from_file(kf, filename, G_KEY_FILE_NONE, NULL))
	{
		vi_set_enabled(utils_get_setting_boolean(kf, CONF_GROUP, CONF_ENABLE_VIM, TRUE));
		vi_set_insert_for_dummies(utils_get_setting_boolean(kf,
			CONF_GROUP, CONF_INSERT_FOR_DUMMIES, FALSE));
		start_in_insert = utils_get_setting_boolean(kf,
			CONF_GROUP, CONF_START_IN_INSERT, FALSE);
	}
  
	g_key_file_free(kf);
	g_free(filename);
}


static void save_config(void)
{
	GKeyFile *kf = g_key_file_new();
	gchar *filename = get_config_filename();
	gchar *dirname = g_path_get_dirname(filename);
	gchar *data;
	gsize length;

	g_key_file_set_boolean(kf, CONF_GROUP, CONF_ENABLE_VIM, vi_get_enabled());
	g_key_file_set_boolean(kf, CONF_GROUP, CONF_INSERT_FOR_DUMMIES, vi_get_insert_for_dummies());
	g_key_file_set_boolean(kf, CONF_GROUP, CONF_START_IN_INSERT, start_in_insert);

	utils_mkdir(dirname, TRUE);
	data = g_key_file_to_data(kf, &length, NULL);
	g_file_set_contents(filename, data, length, NULL);

	g_free(data);
	g_key_file_free(kf);
	g_free(filename);
	g_free(dirname);
}


static void on_enable_vim_mode(void)
{
	gboolean enabled = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_items.enable_vim_item));
	vi_set_enabled(enabled);
	vi_set_mode(start_in_insert ? VI_MODE_INSERT : VI_MODE_COMMAND);
	if (!enabled)
		ui_set_statusbar(FALSE, "Vim Mode Disabled");
	save_config();
}


static gboolean on_enable_vim_mode_kb(GeanyKeyBinding *kb, guint key_id, gpointer data)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items.enable_vim_item),
			!vi_get_enabled());
	return TRUE;
}


static void on_insert_for_dummies(void)
{
	gboolean enabled = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_items.insert_for_dummies_item));
	vi_set_insert_for_dummies(enabled);
	ui_set_statusbar(FALSE, _("Insert Mode for Dummies: %s"), enabled ? _("ON") : _("OFF"));
	save_config();
}


static gboolean on_insert_for_dummies_kb(GeanyKeyBinding *kb, guint key_id, gpointer data)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items.insert_for_dummies_item),
			!vi_get_insert_for_dummies());
	return TRUE;
}


static void on_start_in_insert(void)
{
	gboolean enabled = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_items.start_in_insert_item));
	start_in_insert = enabled;
	save_config();
}


static void on_doc_open(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);
	vi_set_active_sci(doc->editor->sci);
}


static void on_doc_activate(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);
	vi_set_active_sci(doc->editor->sci);
}


static void on_doc_close(G_GNUC_UNUSED GObject * obj, GeanyDocument * doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);
	/* This makes sure we don't hold an invalid scintilla inside the plugin and
	 * that vi_set_active_sci() doesn't crash in plugin_cleanup(). Fortunately
	 * Geany calls document-close before document-activate in which case this
	 * wouldn't work correctly. */
	vi_set_active_sci(NULL);
}


static gboolean on_editor_notify(GObject *object, GeanyEditor *editor,
		SCNotification *nt, gpointer data)
{
	return vi_notify_sci(nt);
}


static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	ScintillaObject *sci = doc != NULL ? doc->editor->sci : NULL;

	if (!sci || gtk_window_get_focus(GTK_WINDOW(geany->main_widgets->window)) != GTK_WIDGET(sci))
		return FALSE;

	return vi_notify_key_press(event);
}


PluginCallback plugin_callbacks[] = {
	{"document-open", (GCallback) &on_doc_open, TRUE, NULL},
	{"document-activate", (GCallback) &on_doc_activate, TRUE, NULL},
	{"document-close", (GCallback) &on_doc_close, TRUE, NULL},
	{"editor-notify", (GCallback) &on_editor_notify, TRUE, NULL},
	{"key-press", (GCallback) &on_key_press, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


static const gchar *get_mode_name(ViMode vi_mode)
{
	switch (vi_mode)
	{
		case VI_MODE_COMMAND:
			return "NORMAL";
			break;
		case VI_MODE_COMMAND_SINGLE:
			return "(insert)";
			break;
		case VI_MODE_INSERT:
			return "INSERT";
			break;
		case VI_MODE_REPLACE:
			return "REPLACE";
			break;
		case VI_MODE_VISUAL:
			return "VISUAL";
			break;
		case VI_MODE_VISUAL_LINE:
			return "VISUAL LINE";
			break;
		case VI_MODE_VISUAL_BLOCK:
			return "VISUAL BLOCK";
			break;
	}
	return "";
}


static void on_mode_change(ViMode mode)
{
	ui_set_statusbar(FALSE, "Vim Mode: -- %s --", get_mode_name(mode));
}


static gboolean on_save(gboolean force)
{
	GeanyDocument *doc = document_get_current();
	if (doc != NULL)
		return document_save_file(doc, force);
	return TRUE;
}


static gboolean on_save_all(gboolean force)
{
	gint i;
	gboolean success = TRUE;
	foreach_document(i)
		success = success && document_save_file(documents[i], force);
	return success;
}


static void on_quit(gboolean force)
{
	//TODO: we need to extend Geany API for this
}


void plugin_init(GeanyData *data)
{
	GeanyDocument *doc = document_get_current();
	GeanyKeyGroup *group;
	GtkWidget *menu;

	load_config();

	/* menu items and keybindings */
	group = plugin_set_key_group(geany_plugin, "vimode", KB_COUNT, NULL);

	menu_items.parent_item = gtk_menu_item_new_with_mnemonic(_("_Vim Mode"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_items.parent_item);

	menu = gtk_menu_new ();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items.parent_item), menu);

	menu_items.enable_vim_item = gtk_check_menu_item_new_with_mnemonic(_("Enable _Vim Mode"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.enable_vim_item);
	g_signal_connect((gpointer) menu_items.enable_vim_item, "activate", G_CALLBACK(on_enable_vim_mode), NULL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items.enable_vim_item), vi_get_enabled());
	keybindings_set_item_full(group, KB_ENABLE_VIM, 0, 0, "enable_vim",
			_("Enable Vim Mode"), NULL, on_enable_vim_mode_kb, NULL, NULL);

	menu_items.insert_for_dummies_item = gtk_check_menu_item_new_with_mnemonic(_("Insert Mode for _Dummies"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.insert_for_dummies_item);
	g_signal_connect((gpointer) menu_items.insert_for_dummies_item, "activate",
		G_CALLBACK(on_insert_for_dummies), NULL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items.insert_for_dummies_item), vi_get_insert_for_dummies());
	keybindings_set_item_full(group, KB_INSERT_FOR_DUMMIES, 0, 0, "insert_for_dummies",
			_("Insert Mode for Dummies"), NULL, on_insert_for_dummies_kb, NULL, NULL);

	menu_items.start_in_insert_item = gtk_check_menu_item_new_with_mnemonic(_("Start in _Insert Mode"));
	gtk_container_add(GTK_CONTAINER(menu), menu_items.start_in_insert_item);
	g_signal_connect((gpointer) menu_items.start_in_insert_item, "activate",
		G_CALLBACK(on_start_in_insert), NULL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items.start_in_insert_item), start_in_insert);

	gtk_widget_show_all(menu_items.parent_item);

	cb.on_mode_change = on_mode_change;
	cb.on_save = on_save;
	cb.on_save_all = on_save_all;
	cb.on_quit = on_quit;
	vi_init(geany_data->main_widgets->window, &cb);
	vi_set_mode(start_in_insert ? VI_MODE_INSERT : VI_MODE_COMMAND);

	if (doc)
		vi_set_active_sci(doc->editor->sci);
}


void plugin_cleanup(void)
{
	vi_cleanup();
	gtk_widget_destroy(menu_items.parent_item);
}


void plugin_help(void)
{
	utils_open_browser("http://plugins.geany.org/vimode.html");
}
