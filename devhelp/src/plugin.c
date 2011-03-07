/*
 * plugin.c - Part of the Geany Devhelp Plugin
 * 
 * Copyright 2010 Matthew Brush <mbrush@leftclick.ca>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
 
#include <sys/stat.h> /* for g_mkdir_with_parents, is it portable? */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h> /* for keybindings */
#include <geanyplugin.h>
#include <devhelp/dh-search.h>

#include "plugin.h"
#include "devhelpplugin.h"

PLUGIN_VERSION_CHECK(200)

PLUGIN_SET_INFO(
	_("Devhelp Plugin"), 
	_("Adds built-in Devhelp support."),
	"1.0", "Matthew Brush <mbrush@leftclick.ca>")

GeanyPlugin	 	*geany_plugin;
GeanyData	   	*geany_data;
GeanyFunctions 	*geany_functions;

DevhelpPlugin *dev_help_plugin = NULL;

static gchar *default_config = NULL;
static gchar *user_config = NULL;
static gboolean move_sidebar_tabs_bottom;
static gboolean show_in_msg_window;

/* keybindings */
enum
{
	KB_DEVHELP_TOGGLE_CONTENTS,
	KB_DEVHELP_TOGGLE_SEARCH,
	KB_DEVHELP_SEARCH_SYMBOL,
	KB_COUNT
};

/* Called when a keybinding is activated */
static void kb_activate(guint key_id)
{
	switch (key_id)
	{
		case KB_DEVHELP_TOGGLE_CONTENTS:
			devhelp_plugin_activate_tabs(dev_help_plugin, TRUE);
			break;
		case KB_DEVHELP_TOGGLE_SEARCH:
			devhelp_plugin_activate_tabs(dev_help_plugin, FALSE);
			break;
		case KB_DEVHELP_SEARCH_SYMBOL:
		{
			gchar *current_tag = devhelp_plugin_get_current_tag();
			if (current_tag == NULL) return;
			dh_search_set_search_string(
				DH_SEARCH(dev_help_plugin->search), current_tag, NULL);
			devhelp_plugin_activate_tabs(dev_help_plugin, FALSE);
			g_free(current_tag);
			break;
		}
	}
}

static void 
move_sidebar_tabs_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	move_sidebar_tabs_bottom = gtk_toggle_button_get_active(
									GTK_TOGGLE_BUTTON(togglebutton));
	devhelp_plugin_sidebar_tabs_bottom(dev_help_plugin, move_sidebar_tabs_bottom);
}

static void 
show_in_msg_window_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	show_in_msg_window = gtk_toggle_button_get_active(
								GTK_TOGGLE_BUTTON(togglebutton));
}

static void 
configure_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
	static gboolean message_shown = FALSE;
	if (response_id == GTK_RESPONSE_OK || response_id == GTK_RESPONSE_APPLY) {
		plugin_store_preferences();
		if (!message_shown) 
		{
			dialogs_show_msgbox(GTK_MESSAGE_INFO, "Settings for whether to use "
				"the message window or not will only take effect upon "
				"reloading the Devhelp plugin or restarting Geany.");
			message_shown = TRUE;
		}
	}
}

gint plugin_load_preferences()
{
	GError *error;
	GKeyFile *kf;
	gint rcode=0;
	
	kf = g_key_file_new();
	
	error = NULL;
	if (!g_key_file_load_from_file(kf, user_config, G_KEY_FILE_NONE, &error))
	{
		g_warning("Unable to load config file '%s': %s", user_config, error->message);
		g_error_free(error);
		g_key_file_free(kf);
		return ++rcode;
	}
	
	/* add new settings here */
	error = NULL;
	move_sidebar_tabs_bottom = g_key_file_get_boolean(kf, "general",
													  "move_sidebar_tabs_bottom",
													  &error);
	if (error)
	{
		g_warning("Unable to load 'move_sidebar_tabs_bottom' setting: %s",
				  error->message);
		g_error_free(error);
		error = NULL;
		rcode++;
	}
	
	error = NULL;
	show_in_msg_window = g_key_file_get_boolean(kf, "general",
												"show_in_message_window",
												&error);
	if (error)
	{
		g_warning("Unable to load 'show_in_message_window' setting: %s",
				  error->message);
		g_error_free(error);
		error = NULL;
		rcode++;
	}
	
	g_key_file_free(kf);
	
	return rcode;	
}

gint plugin_store_preferences()
{
	gchar *config_text;
	GError *error;
	GKeyFile *kf;
	gint rcode=0;
	
	kf = g_key_file_new();

	/* add new settings here */
	g_key_file_set_boolean(kf, "general", "move_sidebar_tabs_bottom", 
						   move_sidebar_tabs_bottom);
	g_key_file_set_boolean(kf, "general", "show_in_message_window",
						   show_in_msg_window);
	
	config_text = g_key_file_to_data(kf, NULL, NULL);
	g_key_file_free(kf);
	
	error = NULL;
	if (!g_file_set_contents(user_config, config_text, -1, &error))
	{
		g_warning("Unable to save config file '%s': %s",
				  user_config, error->message);
		g_error_free(error);
		rcode++;
	}
	
	g_free(config_text);
	
	return rcode;	
}

gboolean plugin_config_init()
{
	gchar *user_config_dir;
	gboolean rcode = TRUE;
	
	default_config = g_build_path(G_DIR_SEPARATOR_S,
								  DHPLUG_DATA_DIR,
								  "devhelp.conf", 
								  NULL);
	
	user_config_dir = g_build_path(G_DIR_SEPARATOR_S,
							   geany_data->app->configdir,
							   "plugins",
							   "devhelp",
							   NULL);
							   
	user_config = g_build_path(G_DIR_SEPARATOR_S,
							   user_config_dir,
							   "devhelp.conf",
							   NULL);
	
	/* TODO: is this portable? */
	if (g_mkdir_with_parents(user_config_dir, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
		g_warning(_("Unable to create config dir at '%s'"), user_config_dir);
		g_free(user_config_dir);
		return FALSE;
	}
	g_free(user_config_dir);
	
	/* copy default config into user config if it doesn't exist */
	if (!g_file_test(user_config, G_FILE_TEST_EXISTS))
	{
		gchar *config_text;
		GError *error;
		
		error = NULL;
		if (!g_file_get_contents(default_config, &config_text, NULL, &error))
		{
			g_warning(_("Unable to get default configuration: %s"), 
					  error->message);
			g_error_free(error);
			error = NULL;
			config_text = " ";
			rcode = FALSE;
		}
		if (!g_file_set_contents(user_config, config_text, -1, &error))
		{
			g_warning(_("Unable to write default configuration: %s"),
					  error->message);
			g_error_free(error);
			error = NULL;
			rcode = FALSE;
		}
	}
	
	return rcode;
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
	
	GtkWidget *check_button = gtk_check_button_new_with_label(
								_("Move sidebar notebook tabs to the bottom."));
	gtk_box_pack_start(GTK_BOX(vbox), check_button, FALSE, TRUE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), move_sidebar_tabs_bottom);
	g_signal_connect(check_button, "toggled", G_CALLBACK(move_sidebar_tabs_toggled), NULL);
	
	check_button = gtk_check_button_new_with_label(
						_("Show documentation in message window."));
	gtk_box_pack_start(GTK_BOX(vbox), check_button, FALSE, TRUE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), show_in_msg_window);
	g_signal_connect(check_button, "toggled", G_CALLBACK(show_in_msg_window_toggled), NULL);
	
	g_signal_connect(dialog, "response", G_CALLBACK(configure_dialog_response), NULL);
	
	return vbox;
}

void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;

	plugin_module_make_resident(geany_plugin);

	plugin_config_init();				   
	plugin_load_preferences();
	
	dev_help_plugin = devhelp_plugin_new(move_sidebar_tabs_bottom,
										 show_in_msg_window);

	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "devhelp", KB_COUNT, NULL);
	
	keybindings_set_item(key_group, KB_DEVHELP_TOGGLE_CONTENTS, kb_activate,
		0, 0, "devhelp_toggle_contents", _("Toggle Devhelp (Contents Tab)"), NULL);
	keybindings_set_item(key_group, KB_DEVHELP_TOGGLE_SEARCH, kb_activate,
		0, 0, "devhelp_toggle_search", _("Toggle Devhelp (Search Tab)"), NULL);
	keybindings_set_item(key_group, KB_DEVHELP_SEARCH_SYMBOL, kb_activate,
		0, 0, "devhelp_search_symbol", _("Search for Current Symbol/Tag"), NULL);
	
}

void plugin_cleanup(void)
{	
	plugin_store_preferences();
	
	g_object_unref(dev_help_plugin);
	
	g_free(default_config);
	g_free(user_config);
}
