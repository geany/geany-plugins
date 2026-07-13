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
#include <git2.h>

#include <wb_globals.h>

#include "sidebar.h"
#include "menu.h"
#include "popup_menu.h"
#include "idle_queue.h"
#include "tm_control.h"


#if ! defined (LIBGIT2_VER_MINOR) || ( (LIBGIT2_VER_MAJOR == 0) && (LIBGIT2_VER_MINOR < 22))
# define git_libgit2_init     git_threads_init
# define git_libgit2_shutdown git_threads_shutdown
#endif


GeanyPlugin *geany_plugin;
GeanyData *geany_data;

/* Callback function for document open */
static void plugin_workbench_on_doc_open(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GeanyDocument * doc,
										 G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	wb_idle_queue_add_action(WB_IDLE_ACTION_ID_TM_SOURCE_FILE_REMOVE,
		g_strdup(doc->file_name));
}


/* Callback function for document close */
static void plugin_workbench_on_doc_close(G_GNUC_UNUSED GObject * obj, GeanyDocument * doc,
										  G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	/* tags of open files managed by geany - when the file gets closed,
	 * we should take care of it */
	wb_idle_queue_add_action(WB_IDLE_ACTION_ID_TM_SOURCE_FILE_ADD,
		g_strdup(doc->file_name));
}


#if GEANY_API_VERSION >= 240

/* Callback function for document activate */
static void plugin_workbench_on_doc_activate(G_GNUC_UNUSED GObject * obj, GeanyDocument * doc,
											 G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL && doc->file_name != NULL);
	if (wb_globals.opened_wb != NULL &&
		workbench_get_enable_auto_open_by_doc(wb_globals.opened_wb) == TRUE &&
		workbench_get_selected_project(wb_globals.opened_wb) == NULL)
	{
		wb_idle_queue_add_action(WB_IDLE_ACTION_ID_OPEN_PROJECT_BY_DOC,
			g_strdup(doc->file_name));
	}
}

#endif


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
	wb_tm_control_init();

	/* At start there is no workbench open:
	   deactive save and close menu item and sidebar */
	menu_set_context(MENU_CONTEXT_WB_CLOSED);
	sidebar_show_intro_message(_("Create or open a workbench\nusing the workbench menu."), FALSE);

	/* Init libgit2. */
	git_libgit2_init();

#if GEANY_API_VERSION < 240
	ui_set_statusbar(TRUE,
		_("Workbench: project switching is not available. Your version of Geany is too old. "
		  "At least version 1.36 is required to enable project switching."));
#endif

	return TRUE;
}


/* Cleanup plugin */
static void plugin_workbench_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	menu_cleanup();
	sidebar_cleanup();
	wb_tm_control_cleanup();

	/* Shutdown/cleanup libgit2. */
	git_libgit2_shutdown();
}


/* Show help */
static void plugin_workbench_help (G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	utils_open_browser("http://plugins.geany.org/workbench.html");
}


static PluginCallback plugin_workbench_callbacks[] = {
	{"document-open", (GCallback) &plugin_workbench_on_doc_open, TRUE, NULL},
	{"document-close", (GCallback) &plugin_workbench_on_doc_close, TRUE, NULL},
#if GEANY_API_VERSION >= 240
	{"document_activate", (GCallback) &plugin_workbench_on_doc_activate, TRUE, NULL},
#endif
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
	plugin->info->version = "1.10";
	plugin->info->author = "LarsGit223";

	/* Set functions */
	plugin->funcs->init = plugin_workbench_init;
	plugin->funcs->cleanup = plugin_workbench_cleanup;
	plugin->funcs->help = plugin_workbench_help;
	plugin->funcs->callbacks = plugin_workbench_callbacks;

	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 235);
}
