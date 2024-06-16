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

#include <string.h>
#include <sys/stat.h> /* for g_mkdir_with_parents, is it portable? */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h> /* for keybindings */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <geanyplugin.h>

//#include <devhelp/dh-search.h>

#include "dhp-plugin.h"
#include "dhp.h"


GeanyPlugin	 	*geany_plugin;
GeanyData	   	*geany_data;


struct PluginData
{
	gchar *default_config;
	gchar *user_config;

	DevhelpPlugin *devhelp;
};


struct PluginData plugin_data;

/* keybindings */
enum
{
	KB_DEVHELP_TOGGLE_CONTENTS,
	KB_DEVHELP_TOGGLE_SEARCH,
	KB_DEVHELP_TOGGLE_WEBVIEW,
	KB_DEVHELP_ACTIVATE_DEVHELP,
	KB_DEVHELP_SEARCH_SYMBOL,
	KB_DEVHELP_SEARCH_MANPAGES,
	KB_COUNT
};

/* Called when a keybinding is activated */
static void kb_activate(guint key_id)
{
	gchar *current_tag;

	switch (key_id)
	{
		case KB_DEVHELP_TOGGLE_CONTENTS:
			devhelp_plugin_toggle_contents_tab(plugin_data.devhelp);
			break;
		case KB_DEVHELP_TOGGLE_SEARCH:
			devhelp_plugin_toggle_search_tab(plugin_data.devhelp);
			break;
		case KB_DEVHELP_TOGGLE_WEBVIEW: /* not working */
			devhelp_plugin_toggle_webview_tab(plugin_data.devhelp);
			break;
		case KB_DEVHELP_ACTIVATE_DEVHELP:
			devhelp_plugin_activate_all_tabs(plugin_data.devhelp);
			break;
		case KB_DEVHELP_SEARCH_SYMBOL:
		{
			current_tag = devhelp_plugin_get_current_word(plugin_data.devhelp);
			if (current_tag == NULL)
				return;
			devhelp_plugin_search_books(plugin_data.devhelp, current_tag);
			g_free(current_tag);
			break;
		}
		case KB_DEVHELP_SEARCH_MANPAGES:
		{
			current_tag = devhelp_plugin_get_current_word(plugin_data.devhelp);
			if (current_tag == NULL)
				return;
			devhelp_plugin_search_manpages(plugin_data.devhelp, current_tag);
			g_free(current_tag);
			break;
		}
	}
}


gboolean plugin_config_init(struct PluginData *pd)
{
	gchar *user_config_dir;

	g_return_val_if_fail(pd != NULL, FALSE);

	plugin_data.default_config = g_build_path(G_DIR_SEPARATOR_S, DHPLUG_DATA_DIR, "devhelp.conf", NULL);

	user_config_dir = g_build_path(G_DIR_SEPARATOR_S, geany_data->app->configdir, "plugins", "devhelp", NULL);
	plugin_data.user_config = g_build_path(G_DIR_SEPARATOR_S, user_config_dir, "devhelp.conf", NULL);
	if (g_mkdir_with_parents(user_config_dir, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
	{
		g_warning(_("Unable to create config dir at '%s'"), user_config_dir);
		g_free(user_config_dir);
		return FALSE;
	}
	g_free(user_config_dir);

	/* copy default config into user config if it doesn't exist */
	if (!g_file_test(pd->user_config, G_FILE_TEST_EXISTS))
	{
		gchar *config_text;
		GError *error;

		error = NULL;
		if (!g_file_get_contents(pd->default_config, &config_text, NULL, &error))
		{
			g_warning(_("Unable to get default configuration: %s"), error->message);
			g_error_free(error);
			return FALSE;
		}
		else
		{
			if (!g_file_set_contents(pd->user_config, config_text, -1, &error))
			{
				g_warning(_("Unable to write default configuration: %s"), error->message);
				g_error_free(error);
				return FALSE;
			}
		}
	}

	return TRUE;
}


static gboolean plugin_devhelp_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	GeanyKeyGroup *key_group;

	geany_plugin = plugin;
	geany_data = plugin->geany_data;

	plugin_module_make_resident(geany_plugin);

	if (!g_thread_supported())
		g_thread_init(NULL);

	memset(&plugin_data, 0, sizeof(struct PluginData));

	plugin_data.devhelp = devhelp_plugin_new();
	plugin_config_init(&plugin_data);

	devhelp_plugin_load_settings(plugin_data.devhelp, plugin_data.user_config);

	key_group = plugin_set_key_group(geany_plugin, "devhelp", KB_COUNT, NULL);

	keybindings_set_item(key_group, KB_DEVHELP_TOGGLE_CONTENTS, kb_activate,
		0, 0, "devhelp_toggle_contents", _("Toggle sidebar contents tab"), NULL);
	keybindings_set_item(key_group, KB_DEVHELP_TOGGLE_SEARCH, kb_activate,
		0, 0, "devhelp_toggle_search", _("Toggle sidebar search tab"), NULL);
	keybindings_set_item(key_group, KB_DEVHELP_TOGGLE_WEBVIEW, kb_activate,
		0, 0, "devhelp_toggle_webview", _("Toggle documentation tab"), NULL);
	keybindings_set_item(key_group, KB_DEVHELP_ACTIVATE_DEVHELP, kb_activate,
		0, 0, "devhelp_activate_all", _("Activate all tabs"), NULL);
	keybindings_set_item(key_group, KB_DEVHELP_SEARCH_SYMBOL, kb_activate,
		0, 0, "devhelp_search_symbol", _("Search for current tag in Devhelp"), NULL);
	if (devhelp_plugin_get_have_man_prog(plugin_data.devhelp))
	{
		keybindings_set_item(key_group, KB_DEVHELP_SEARCH_MANPAGES, kb_activate,
			0, 0, "devhelp_search_manpages", _("Search for current tag in Manual Pages"), NULL);
	}

	return TRUE;
}


static void plugin_devhelp_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	devhelp_plugin_store_settings(plugin_data.devhelp, plugin_data.user_config);
	g_object_unref(plugin_data.devhelp);
	g_free(plugin_data.default_config);
	g_free(plugin_data.user_config);
}


static void plugin_devhelp_help (G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	utils_open_browser("https://plugins.geany.org/devhelp.html");
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	/* Set metadata */
	plugin->info->name = _("Devhelp Plugin");
	plugin->info->description = _("Adds support for looking up documentation in Devhelp, manual pages, and "
		"Google Code Search in the integrated viewer.");
	plugin->info->version = "1.0";
	plugin->info->author = "Matthew Brush <mbrush@leftclick.ca>";

	/* Set functions */
	plugin->funcs->init = plugin_devhelp_init;
	plugin->funcs->cleanup = plugin_devhelp_cleanup;
	plugin->funcs->help = plugin_devhelp_help;

	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
