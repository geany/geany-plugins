/*
 *      plugin.c
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
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
 */

/*
 * 		geany debugger plugin
 */
 
#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "breakpoints.h"
#include "callbacks.h"
#include "debug.h"
#include "tpage.h"
#include "utils.h"
#include "btnpanel.h"
#include "keys.h"
#include "dconfig.h"
#include "dpaned.h"
#include "tabs.h"
#include "envtree.h"
#include "pixbuf.h"

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin		*geany_plugin;
GeanyData			*geany_data;

/* vbox for keeping breaks/stack/watch notebook */
static GtkWidget *hbox = NULL;

static PluginCallback plugin_debugger_callbacks[] =
{
	/* Set 'after' (third field) to TRUE to run the callback @a after the default handler.
	 * If 'after' is FALSE, the callback is run @a before the default handler, so the plugin
	 * can prevent Geany from processing the notification. Use this with care. */
	{ "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
	{ "document_open", (GCallback) &on_document_open, FALSE, NULL },
	{ "document_save", (GCallback) &on_document_save, FALSE, NULL },
	{ "document_before_save", (GCallback) &on_document_before_save, FALSE, NULL },
	{ "project_open", (GCallback) &config_on_project_open, FALSE, NULL },
	{ "project_close", (GCallback) &config_on_project_close, FALSE, NULL },
	{ "project_save", (GCallback) &config_on_project_save, FALSE, NULL },
	
	{ NULL, NULL, FALSE, NULL }
};

static void on_paned_mode_changed(GtkToggleButton *button, gpointer user_data)
{
	gboolean state = gtk_toggle_button_get_active(button);
	dpaned_set_tabbed(state);
	tpage_pack_widgets(state);
}

/* Called by Geany to initialize the plugin.
 * Note: data is the same as geany_data. */
static gboolean plugin_debugger_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget* vbox;
	guint i;

	geany_plugin = plugin;
	geany_data = plugin->geany_data;

	plugin_module_make_resident(geany_plugin);

	keys_init();
	
	pixbufs_init();

	/* main box */
#if GTK_CHECK_VERSION(3, 0, 0)
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
#else
	hbox = gtk_hbox_new(FALSE, 7);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);

	/* add target page */
	tpage_init();
	
	/* init brekpoints */
	breaks_init(editor_open_position);
	
	/* init markers */
	markers_init();

	/* init debug */
	debug_init();

	/* load config */
	config_init();

	/* init paned */
	dpaned_init();
	tpage_pack_widgets(config_get_tabbed());

	vbox = btnpanel_create(on_paned_mode_changed);

	gtk_box_pack_start(GTK_BOX(hbox), dpaned_get_paned(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	gtk_widget_show_all(hbox);
	
	gtk_notebook_append_page(
		GTK_NOTEBOOK(geany->main_widgets->message_window_notebook),
		hbox,
		gtk_label_new(_("Debug")));

	if (geany_data->app->project)
	{
		config_update_project_keyfile();
	}
	config_set_debug_store(
		config_get_save_to_project() && geany_data->app->project ? DEBUG_STORE_PROJECT : DEBUG_STORE_PLUGIN
	);

	/* set calltips for all currently opened documents */
	foreach_document(i)
	{
		scintilla_send_message(document_index(i)->editor->sci, SCI_SETMOUSEDWELLTIME, 500, 0);
		scintilla_send_message(document_index(i)->editor->sci, SCI_CALLTIPUSESTYLE, 20, (long)NULL);
	}

	return TRUE;
}

/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * plugin_init() was called.
 * You can omit this function if the plugin doesn't need to be configured.
 * Note: parent is the parent window which can be used as the transient window for the created
 *       dialog. */
static GtkWidget *plugin_debugger_configure(G_GNUC_UNUSED GeanyPlugin *plugin, GtkDialog *dialog, G_GNUC_UNUSED gpointer pdata)
{
	return config_plugin_configure(dialog);
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed, memory freed and any other finalization done.
 * Be sure to leave Geany as it was before plugin_init(). */
static void plugin_debugger_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	/* stop debugger if running */
	if (DBS_IDLE != debug_get_state())
	{
		debug_stop();
		while (DBS_IDLE != debug_get_state())
			g_main_context_iteration(NULL,FALSE);
	}

	config_destroy();
	pixbufs_destroy();
	debug_destroy();
	breaks_destroy();
	dpaned_destroy();
	envtree_destroy();
	
	/* release other allocated strings and objects */
	gtk_widget_destroy(hbox);
}


/* Show help */
static void plugin_debugger_help (G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	utils_open_browser("https://plugins.geany.org/debugger.html");
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	/* Set metadata */
	plugin->info->name = _("Debugger");
	plugin->info->description = _("Various debuggers integration.");
	plugin->info->version = VERSION;
	plugin->info->author = "Alexander Petukhov <devel@apetukhov.ru>";

	/* Set functions */
	plugin->funcs->init = plugin_debugger_init;
	plugin->funcs->cleanup = plugin_debugger_cleanup;
	plugin->funcs->help = plugin_debugger_help;
	plugin->funcs->configure = plugin_debugger_configure;
	plugin->funcs->callbacks = plugin_debugger_callbacks;

	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
