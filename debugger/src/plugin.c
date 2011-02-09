/*
 *      plugin.c
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
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
 
#include "geanyplugin.h"
#include "breakpoint.h"
#include "breakpoints.h"
#include "callbacks.h"
#include "debug.h"
#include "tpage.h"
#include "utils.h"

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin		*geany_plugin;
GeanyData			*geany_data;
GeanyFunctions	*geany_functions;


/* Check that the running Geany supports the plugin API version used below, and check
 * for binary compatibility. */
PLUGIN_VERSION_CHECK(147)

/* All plugins must set name, description, version and author. */
PLUGIN_SET_INFO(_("Debugger"), _("Various debuggers integration."), "0.1" , "Alexander.Petukhov@mail.ru")

/* vbox for keeping breaks/stack/watch notebook */
static GtkWidget *vbox = NULL;

PluginCallback plugin_callbacks[] =
{
	/* Set 'after' (third field) to TRUE to run the callback @a after the default handler.
	 * If 'after' is FALSE, the callback is run @a before the default handler, so the plugin
	 * can prevent Geany from processing the notification. Use this with care. */
	{ "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
	{ "document_open", (GCallback) &on_document_open, FALSE, NULL },
	{ "document_activate", (GCallback) &on_document_activate, FALSE, NULL },
	{ "document_close", (GCallback) &on_document_close, FALSE, NULL },
	{ "document_save", (GCallback) &on_document_save, FALSE, NULL },
	{ "document_new", (GCallback) &on_document_new, FALSE, NULL },
	
	{ NULL, NULL, FALSE, NULL }
};

/* Called by Geany to initialize the plugin.
 * Note: data is the same as geany_data. */
void plugin_init(GeanyData *data)
{
	/* intialize gettext */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	vbox = gtk_vbox_new(1, 0);
	
	GtkWidget *debug_notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(debug_notebook), GTK_POS_TOP);

	keys_init();
	
	/* add target page */
	tpage_init();
	gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook),
		tpage_get_widget(),
		gtk_label_new(_("Target")));
	
	/* init brekpoints */
	breaks_init(editor_open_position);
	gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook),
		breaks_get_widget(),
		gtk_label_new(_("Breakpoints")));
		
	/* init markers */
	markers_init();

	/* init debug */
	debug_init(debug_notebook);

	gtk_widget_show_all(debug_notebook);
	
	gtk_box_pack_start(GTK_BOX(vbox), debug_notebook, 1, 1, 0);
	gtk_widget_show_all(vbox);
	
	gtk_notebook_append_page(
		GTK_NOTEBOOK(geany->main_widgets->message_window_notebook),
		vbox,
		gtk_label_new(_("Debug")));

	/* if we have config in current path - load it */
	if (tpage_have_config())
		tpage_load_config();
}



/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * plugin_init() was called.
 * You can omit this function if the plugin doesn't need to be configured.
 * Note: parent is the parent window which can be used as the transient window for the created
 *       dialog. */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	/* configuration dialog */
	vbox = gtk_vbox_new(FALSE, 6);

	gtk_widget_show_all(vbox);
	return vbox;
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed, memory freed and any other finalization done.
 * Be sure to leave Geany as it was before plugin_init(). */
void plugin_cleanup(void)
{
	/* stop debugger if running */
	if (DBS_IDLE != debug_get_state())
	{
		debug_stop();
		while (DBS_IDLE != debug_get_state())
			g_main_context_iteration(NULL,FALSE);
	}

	/* destroy debug-related stuff */
	debug_destroy();

	/* destroy breaks */
	breaks_destroy();

	/* release other allocated strings and objects */
	gtk_widget_destroy(vbox);
}
