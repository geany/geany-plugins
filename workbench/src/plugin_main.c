/*
 * Copyright 2017 LarsGit223
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

/*
 * Code for plugin setup/registration.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/time.h>
#include <string.h>

#include <wb_globals.h>

#include "sidebar.h"
#include "menu.h"
#include "popup_menu.h"

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

/* Callback function for document open */
static void plugin_workbench_on_doc_open(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GeanyDocument * doc,
										 G_GNUC_UNUSED gpointer user_data)
{
	WB_PROJECT *project;

	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	project = workbench_file_is_included(wb_globals.opened_wb, doc->file_name);
	if (project != NULL)
	{
		wb_project_add_idle_action(WB_PROJECT_IDLE_ACTION_ID_REMOVE_SINGLE_TM_FILE,
			project, g_strdup(doc->file_name));
	}
}


/* Callback function for document close */
static void plugin_workbench_on_doc_close(G_GNUC_UNUSED GObject * obj, GeanyDocument * doc,
										  G_GNUC_UNUSED gpointer user_data)
{
	WB_PROJECT *project;

	g_return_if_fail(doc != NULL);

	if (doc->file_name == NULL)
	{
		return;
	}

	/* tags of open files managed by geany - when the file gets closed,
	 * we should take care of it */
	project = workbench_file_is_included(wb_globals.opened_wb, doc->file_name);
	if (project != NULL)
	{
		wb_project_add_idle_action(WB_PROJECT_IDLE_ACTION_ID_ADD_SINGLE_TM_FILE,
			project, g_strdup(doc->file_name));
	}
}


/* Initialize plugin */
static gboolean plugin_workbench_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	/* Init/Update globals */
	workbench_globals_init();
	wb_globals.geany_plugin = plugin;
	geany_plugin = plugin;
	geany_data = plugin->geany_data;

	menu_init();
	sidebar_init();
	popup_menu_init();

	/* At start there is no workbench open:
	   deactive save and close menu item and sidebar */
	menu_set_context(MENU_CONTEXT_WB_CLOSED);
	sidebar_show_intro_message(_("Create or open a workbench\nusing the workbench menu."), FALSE);

	return TRUE;
}


/* Cleanup plugin */
static void plugin_workbench_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	menu_cleanup();
	sidebar_cleanup();
}


/* Show help */
static void plugin_workbench_help (G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	utils_open_browser("http://plugins.geany.org/workbench.html");
}


static PluginCallback plugin_workbench_callbacks[] = {
	{"document-open", (GCallback) &plugin_workbench_on_doc_open, TRUE, NULL},
	{"document-close", (GCallback) &plugin_workbench_on_doc_close, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	/* Set metadata */
	plugin->info->name = _("Workbench");
	plugin->info->description = _("Manage and customize multiple projects.");
	plugin->info->version = "1.03";
	plugin->info->author = "LarsGit223";

	/* Set functions */
	plugin->funcs->init = plugin_workbench_init;
	plugin->funcs->cleanup = plugin_workbench_cleanup;
	plugin->funcs->help = plugin_workbench_help;
	plugin->funcs->callbacks = plugin_workbench_callbacks;

	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 235);
}
