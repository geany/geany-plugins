/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
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

#include <sys/time.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "gproject-project.h"
#include "gproject-sidebar.h"
#include "gproject-menu.h"

PLUGIN_VERSION_CHECK(214)
PLUGIN_SET_INFO(_("GProject"),
	_("Glob-pattern-based project management plugin for Geany."),
	VERSION,
	"Jiri Techet <techet@gmail.com>")

GeanyPlugin *geany_plugin;
GeanyData *geany_data;
GeanyFunctions *geany_functions;


static gint page_index = -1;


void plugin_init(G_GNUC_UNUSED GeanyData * data);
void plugin_cleanup(void);


static void on_doc_open(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GeanyDocument * doc,
	    G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	/* tags of open files managed by geany*/
	if (gprj_project_is_in_project(doc->file_name))
		gprj_project_remove_file_tag(doc->file_name);

	gprj_sidebar_update(FALSE);
}


static void on_doc_activate(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GeanyDocument * doc,
		G_GNUC_UNUSED gpointer user_data)
{
	gprj_sidebar_update(FALSE);
}


static void on_doc_close(G_GNUC_UNUSED GObject * obj, GeanyDocument * doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);

	if (doc->file_name == NULL)
		return;

	/* tags of open files managed by geany - when the file gets closed, 
	 * we should take care of it */
	if (gprj_project_is_in_project(doc->file_name))
		gprj_project_add_file_tag(doc->file_name);

	gprj_sidebar_update(FALSE);
}


static void on_build_start(GObject *obj, gpointer user_data)
{
	guint i;

	foreach_document(i)
	{
		if (gprj_project_is_in_project(documents[i]->file_name))
			document_save_file(documents[i], FALSE);
	}
}


static void on_project_dialog_open(G_GNUC_UNUSED GObject * obj, GtkWidget * notebook,
		G_GNUC_UNUSED gpointer user_data)
{
	if (g_prj && page_index == -1)
		page_index = gprj_project_add_properties_tab(notebook);
}


static void on_project_dialog_confirmed(G_GNUC_UNUSED GObject * obj, GtkWidget * notebook,
		G_GNUC_UNUSED gpointer user_data)
{
	if (g_prj)
	{
		gprj_project_read_properties_tab();
		gprj_sidebar_update(TRUE);
	}
}


static void on_project_dialog_close(G_GNUC_UNUSED GObject * obj, GtkWidget * notebook,
		G_GNUC_UNUSED gpointer user_data)
{
	if (page_index != -1)
	{
		gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), page_index);
		page_index = -1;
	}
}


static void on_project_open(G_GNUC_UNUSED GObject * obj, GKeyFile * config,
		G_GNUC_UNUSED gpointer user_data)
{
	gprj_project_open(config);
	gprj_sidebar_update(TRUE);
	gprj_sidebar_activate(TRUE);
	gprj_menu_activate_menu_items(TRUE);
}



static void on_project_close(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED gpointer user_data)
{
	gprj_project_close();
	gprj_sidebar_update(TRUE);
	gprj_sidebar_activate(FALSE);
	gprj_menu_activate_menu_items(FALSE);
}


static void on_project_save(G_GNUC_UNUSED GObject * obj, GKeyFile * config,
		G_GNUC_UNUSED gpointer user_data)
{
	if (!g_prj)
	{
		/* happens when the project is created */
		gprj_project_open(config);
		gprj_sidebar_update(TRUE);
		gprj_sidebar_activate(TRUE);
		gprj_menu_activate_menu_items(TRUE);
	}

	gprj_project_save(config);
}


PluginCallback plugin_callbacks[] = {
	{"document-open", (GCallback) & on_doc_open, TRUE, NULL},
	{"document-activate", (GCallback) & on_doc_activate, TRUE, NULL},
	{"document-close", (GCallback) & on_doc_close, TRUE, NULL},
	{"build-start", (GCallback) & on_build_start, TRUE, NULL},
	{"project-dialog-open", (GCallback) & on_project_dialog_open, TRUE, NULL},
	{"project-dialog-confirmed", (GCallback) & on_project_dialog_confirmed, TRUE, NULL},
	{"project-dialog-close", (GCallback) & on_project_dialog_close, TRUE, NULL},
	{"project-open", (GCallback) & on_project_open, TRUE, NULL},
	{"project-close", (GCallback) & on_project_close, TRUE, NULL},
	{"project-save", (GCallback) & on_project_save, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	gprj_menu_init();
	gprj_sidebar_init();
}


void plugin_cleanup(void)
{
	if (geany_data->app->project)
	{
		gprj_project_close();
		gprj_sidebar_update(TRUE);
	}

	gprj_menu_cleanup();
	gprj_sidebar_cleanup();
}
