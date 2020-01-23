/*
 *      lipsum.c
 *
 *      Copyright 2008-2017 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif

#include <string.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include <geanyplugin.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;

PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Lipsum"),
	_("Creating dummy text with Geany"),
	VERSION,
	"Frank Lanitz <frank@frank.uvena.de>")

static GtkWidget *main_menu_item = NULL;
static gchar *lipsum = NULL;
static const gchar *default_loremipsum = "\
Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do\
eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim\
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut\
aliquip ex ea commodo consequat. Duis aute irure dolor in\
reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla\
pariatur. Excepteur sint occaecat cupidatat non proident, sunt in\
culpa qui officia deserunt mollit anim id est laborum. ";


/* Doing some basic keybinding stuff */
enum
{
	LIPSUM_KB_INSERT,
	COUNT_KB
};





static void
insert_string(GeanyDocument *doc, const gchar *string)
{
	if (doc != NULL)
	{
		gint pos = sci_get_current_position(doc->editor->sci);
		sci_insert_text(doc->editor->sci, pos, string);
	}
}


static void
lipsum_activated(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata)
{
	GeanyDocument *doc = NULL;

	/* Setting default length to 1500 characters */
	gdouble value = 1500;

	doc = document_get_current();

	if (doc != NULL && dialogs_show_input_numeric(_("Lipsum-Generator"),
		_("Enter the length of Lipsum text here"), &value, 1, 5000, 1))
	{

		int tmp = 0;
		int x = 0;
		int i = 0;
		int missing = 0;

		/* Checking what we have */
		tmp = strlen(lipsum);

		if (tmp > value)
		{
			x = 0;
			missing = value - (x * tmp);
		}
		else if (tmp == (int)value)
		{
			x = 1;
		}
		else if (tmp > 0)
		{
			x = value / tmp;
			missing = value - (x * tmp);
		}

		sci_start_undo_action(doc->editor->sci);

		/* Insert lipsum snippet as often as needed ... */
		if (missing > 0)
		{
			gchar *missing_text = g_strndup(lipsum, missing);
			insert_string(doc, missing_text);
			g_free(missing_text);
		}
		for (i = 0; i < x; i++)
		{
			insert_string(doc, lipsum);
		}
		sci_end_undo_action(doc->editor->sci);
	}
}


/* Called when keystroke were pressed */
static void kblipsum_insert(G_GNUC_UNUSED guint key_id)
{
	lipsum_activated(NULL, NULL);
}


/* Called by Geany to initialize the plugin */
void
plugin_init(G_GNUC_UNUSED GeanyData *data)
{
	GtkWidget *menu_lipsum = NULL;
	GKeyFile *config = g_key_file_new();
	gchar *config_file = NULL;
	gchar *config_file_old = NULL;
	gchar *config_dir = NULL;
	gchar *config_dir_old = NULL;
	GeanyKeyGroup *key_group;


	config_file = g_strconcat(geany->app->configdir,
		G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"geanylipsum", G_DIR_SEPARATOR_S, "lipsum.conf", NULL);

	#ifndef G_OS_WIN32
	/* We try only to move if we are on not Windows platform */
	config_dir_old = g_build_filename(geany->app->configdir,
		"plugins", "geanylipsum", NULL);
	config_file_old = g_build_filename(config_dir_old,
		"lipsum.conf", NULL);
	config_dir = g_build_filename(geany->app->configdir,
		"plugins", "lipsum", NULL);
	if (g_file_test(config_file_old, G_FILE_TEST_EXISTS))
	{
		if (dialogs_show_question(
			_("Renamed plugin detected!\n"
			  "\n"
			  "As you may have already noticed, GeanyLipsum has been "
			  "renamed to just Lipsum. \n"
			  "Geany is able to migrate your old plugin configuration by "
			  "moving the old configuration file to new location.\n"
			  "Warning: This will not include your keybindings.\n"
			  "Move now?")))
		{
			if (g_rename(config_dir_old, config_dir) == 0)
			{
				dialogs_show_msgbox(GTK_MESSAGE_INFO,
					_("Your configuration directory has been "
					  "successfully moved from \"%s\" to \"%s\"."),
					config_dir_old, config_dir);
			}
			else
			{
				/* If there was an error on migrating we need
				 * to load from original one.
				 * When saving new configuration it will go to
				 * new folder so migration should
				 * be implicit. */
				g_free(config_file);
				config_file = g_strdup(config_file_old);
				dialogs_show_msgbox(
					GTK_MESSAGE_WARNING,
					_("Your old configuration directory \"%s\" could "
					  "not be moved to \"%s\" (%s). "
					  "Please manually move the directory to the new location."),
					config_dir_old,
					config_dir,
					g_strerror(errno));
			}
		}
	}

	g_free(config_dir_old);
	g_free(config_dir);
	g_free(config_file_old);
	#endif

	/* Initialising options from config file  if there is any*/
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);
	lipsum = utils_get_setting_string(config, "snippets", "lipsumtext", default_loremipsum);

	g_key_file_free(config);
	g_free(config_file);

	/* Building menu entry */
#if GTK_CHECK_VERSION(3, 10, 0)
	menu_lipsum = gtk_menu_item_new_with_mnemonic(_("_Lipsum..."));
#else
	menu_lipsum = gtk_image_menu_item_new_with_mnemonic(_("_Lipsum..."));
#endif
	gtk_widget_set_tooltip_text(menu_lipsum, _("Include Pseudotext to your code"));
	gtk_widget_show(menu_lipsum);
	g_signal_connect((gpointer) menu_lipsum, "activate",
			 G_CALLBACK(lipsum_activated), NULL);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_lipsum);


	ui_add_document_sensitive(menu_lipsum);

	main_menu_item = menu_lipsum;

	/* init keybindings */
	key_group = plugin_set_key_group(geany_plugin, "lipsum", COUNT_KB, NULL);
	keybindings_set_item(key_group, LIPSUM_KB_INSERT, kblipsum_insert,
		0, 0, "insert_lipsum", _("Insert Lipsum text"), menu_lipsum);
}


/* Called by Geany before unloading the plugin. */
void plugin_cleanup(void)
{
	/* remove the menu item added in plugin_init() */
	gtk_widget_destroy(main_menu_item);

	/* free lipsum snippet */
	g_free(lipsum);
}
