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

#include <devhelp/dh-search.h>

#include "dhp-plugin.h"
#include "dhp.h"


PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Devhelp Plugin"),
	_("Adds support for looking up documentation in Devhelp, manual pages, and "
	  "Google Code Search in the integrated viewer."),
	"1.0", "Matthew Brush <mbrush@leftclick.ca>")


GeanyPlugin	 	*geany_plugin;
GeanyData	   	*geany_data;

struct PluginData plugin;

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
			devhelp_plugin_toggle_contents_tab(plugin.devhelp);
			break;
		case KB_DEVHELP_TOGGLE_SEARCH:
			devhelp_plugin_toggle_search_tab(plugin.devhelp);
			break;
		case KB_DEVHELP_TOGGLE_WEBVIEW: /* not working */
			devhelp_plugin_toggle_webview_tab(plugin.devhelp);
			break;
		case KB_DEVHELP_ACTIVATE_DEVHELP:
			devhelp_plugin_activate_all_tabs(plugin.devhelp);
			break;
		case KB_DEVHELP_SEARCH_SYMBOL:
		{
			current_tag = devhelp_plugin_get_current_word(plugin.devhelp);
			if (current_tag == NULL)
				return;
			devhelp_plugin_search_books(plugin.devhelp, current_tag);
			g_free(current_tag);
			break;
		}
		case KB_DEVHELP_SEARCH_MANPAGES:
		{
			current_tag = devhelp_plugin_get_current_word(plugin.devhelp);
			if (current_tag == NULL)
				return;
			devhelp_plugin_search_manpages(plugin.devhelp, current_tag);
			g_free(current_tag);
			break;
		}
	}
}


gboolean plugin_config_init(struct PluginData *pd)
{
	gchar *user_config_dir;

	g_return_val_if_fail(pd != NULL, FALSE);

	plugin.default_config = g_build_path(G_DIR_SEPARATOR_S, DHPLUG_DATA_DIR, "devhelp.conf", NULL);

	user_config_dir = g_build_path(G_DIR_SEPARATOR_S, geany_data->app->configdir, "plugins", "devhelp", NULL);
	plugin.user_config = g_build_path(G_DIR_SEPARATOR_S, user_config_dir, "devhelp.conf", NULL);
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


void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;

	plugin_module_make_resident(geany_plugin);

	if (!g_thread_supported())
		g_thread_init(NULL);

	memset(&plugin, 0, sizeof(struct PluginData));

	plugin.devhelp = devhelp_plugin_new();
	plugin_config_init(&plugin);

	devhelp_plugin_load_settings(plugin.devhelp, plugin.user_config);

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
	if (devhelp_plugin_get_have_man_prog(plugin.devhelp))
	{
		keybindings_set_item(key_group, KB_DEVHELP_SEARCH_MANPAGES, kb_activate,
			0, 0, "devhelp_search_manpages", _("Search for current tag in Manual Pages"), NULL);
	}
}


void plugin_cleanup(void)
{
	devhelp_plugin_store_settings(plugin.devhelp, plugin.user_config);
	g_object_unref(plugin.devhelp);
	g_free(plugin.default_config);
	g_free(plugin.user_config);
}
