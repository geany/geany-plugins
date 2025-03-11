/*
 *  geanyprj - Alternative project support for geany light IDE.
 *
 *  Copyright 2007 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *  Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *  Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
 *  Copyright 2007, 2008 Yura Siamashka <yurand2@gmail.com>
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

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif
#include <geanyplugin.h>

#include <sys/time.h>
#include <string.h>

#include "geanyprj.h"

PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
	"GeanyPrj", _("Alternative project support. \nThis plugin currently "
		          "has no maintainer. Would you like to help by contributing "
		          "to this plugin?"),
	VERSION,
	"Yura Siamashka <yurand2@gmail.com>")

GeanyPlugin    *geany_plugin;
GeanyData      *geany_data;


static gchar    *config_file;
static gboolean  display_sidebar = TRUE;


/* Keybinding(s) */
enum
{
	KB_FIND_IN_PROJECT,
	KB_COUNT
};


static void reload_project(void)
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
		set_non_ft_build_wdir("");
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
	set_non_ft_build_wdir(g_current_project->working_dir);
	if (proj)
		g_free(proj);
}


static void on_doc_save(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc, G_GNUC_UNUSED gpointer user_data)
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


static void on_doc_open(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GeanyDocument *doc,
						G_GNUC_UNUSED gpointer user_data)
{
	reload_project();
}


static void on_doc_activate(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GeanyDocument *doc,
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


/* Keybinding callback */
static void kb_find_in_project(guint key_id)
{
	on_find_in_project(NULL, NULL);
}


static void load_settings(void)
{
	GKeyFile *config = g_key_file_new();
	GError   *err    = NULL;
	gboolean  tmp;

	config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"geanyprj", G_DIR_SEPARATOR_S, "geanyprj.conf", NULL);
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	tmp = g_key_file_get_boolean(config, "geanyprj", "display_sidebar", &err);

	if (err)
		g_error_free(err);
	else
		display_sidebar = tmp;

	g_key_file_free(config);
}


static void save_settings(void)
{
	GKeyFile *config = g_key_file_new();
	gchar    *data;
	gchar    *config_dir = g_path_get_dirname(config_file);

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	g_key_file_set_boolean(config, "geanyprj", "display_sidebar", display_sidebar);

	if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
	}
	else
	{
		/* write config to file */
		data = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(config_file, data);
		g_free(data);
	}
	g_free(config_dir);
	g_key_file_free(config);
}


static void on_configure_response(G_GNUC_UNUSED GtkDialog *dialog, G_GNUC_UNUSED gint response, GtkWidget *checkbox)
{
	gboolean old_display_sidebar = display_sidebar;

	display_sidebar = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox));

	if (display_sidebar ^ old_display_sidebar)
	{
		if (display_sidebar)
		{
			create_sidebar();
			sidebar_refresh();
		}
		else
		{
			destroy_sidebar();
		}
		save_settings();
	}
}


/* Called by Geany to initialize the plugin */
void plugin_init(G_GNUC_UNUSED GeanyData *data)
{
	GeanyKeyGroup *key_group;

	load_settings();
	tools_menu_init();

	xproject_init();
	if (display_sidebar)
		create_sidebar();
	reload_project();

	key_group = plugin_set_key_group(geany_plugin, "geanyprj", KB_COUNT, NULL);
	keybindings_set_item(key_group, KB_FIND_IN_PROJECT,
		kb_find_in_project, 0, 0, "find_in_project",
			_("Find a text in geanyprj's project"), NULL);
}


/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * plugin_init() was called.
 */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox;
	GtkWidget *checkbox;

	vbox = gtk_vbox_new(FALSE, 6);

	checkbox = gtk_check_button_new_with_label(_("Display sidebar"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), display_sidebar);

	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	/* Connect a callback for when the user clicks a dialog button */
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), checkbox);

	return vbox;
}


/* Called by Geany before unloading the plugin. */
void plugin_cleanup(void)
{
	tools_menu_uninit();

	if (g_current_project)
		geany_project_free(g_current_project);
	g_current_project = NULL;

	g_free(config_file);

	xproject_cleanup();
	destroy_sidebar();
}
