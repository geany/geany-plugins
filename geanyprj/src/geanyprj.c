/*
 *  geanyprj - Alternative project support for geany light IDE.
 *
 *  Copyright 2007 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *  Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *  Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
 *  Copyright 2007,2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/time.h>
#include <string.h>

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "geanyprj.h"

PLUGIN_VERSION_CHECK(147);
PLUGIN_SET_INFO(_("Project"), _("Alternative project support."), VERSION,
		"Yura Siamashka <yurand2@gmail.com>");

GeanyData *geany_data;
GeanyFunctions *geany_functions;

static void
reload_project()
{
	gchar *dir;
	gchar *proj;
	GeanyDocument *doc;

	debug("%s\n", __FUNCTION__);

	doc = document_get_current();
	if (doc == NULL || doc->file_name == NULL)
		return;

	dir = g_path_get_dirname(doc->file_name);
	proj = find_file_path(dir, ".geanyprj");

	if (!proj)
	{
		if (g_current_project)
			xproject_close(TRUE);
		return;
	}

	if (!g_current_project)
	{
		xproject_open(proj);
	}
	else if (strcmp(proj, g_current_project->path) != 0)
	{
		xproject_close(TRUE);
		xproject_open(proj);
	}
	if (proj)
		g_free(proj);
}

static void
on_doc_save(G_GNUC_UNUSED GObject * obj, GeanyDocument * doc, G_GNUC_UNUSED gpointer user_data)
{
	gchar *name;

	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	name = g_path_get_basename(doc->file_name);
	if (g_current_project && strcmp(name, ".geanyprj") == 0)
	{
		xproject_close(FALSE);
	}
	reload_project();
	xproject_update_tag(doc->file_name);
}

static void
on_doc_open(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GeanyDocument * doc,
	    G_GNUC_UNUSED gpointer user_data)
{
	reload_project();
}

static void
on_doc_activate(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GeanyDocument * doc,
		G_GNUC_UNUSED gpointer user_data)
{
	reload_project();
}

PluginCallback plugin_callbacks[] = {
	{"document-open", (GCallback) & on_doc_open, TRUE, NULL},
	{"document-save", (GCallback) & on_doc_save, TRUE, NULL},
	{"document-activate", (GCallback) & on_doc_activate, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};

/* Called by Geany to initialize the plugin */
void
plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	tools_menu_init();

	xproject_init();
	create_sidebar();
	reload_project();
}

/* Called by Geany before unloading the plugin. */
void
plugin_cleanup()
{
	tools_menu_uninit();

	if (g_current_project)
		geany_project_free(g_current_project);
	g_current_project = NULL;

	xproject_cleanup();
	destroy_sidebar();
}
