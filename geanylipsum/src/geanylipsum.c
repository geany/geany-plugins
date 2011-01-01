/*
 *      geanylipsum.c
 *
 *      Copyright 2008-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include "geanyplugin.h"

#include <string.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;

PLUGIN_VERSION_CHECK(188)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("GeanyLipsum"),
	_("Creating dummy text with Geany"),
	"0.4.3",
	"Frank Lanitz <frank@frank.uvena.de>");

static GtkWidget *main_menu_item = NULL;
static gchar *lipsum = NULL;
static const gchar *default_loremipsum = "\
Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy\
eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam\
voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet \
clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.";


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
			insert_string(doc, lipsum);

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
	GtkTooltips *tooltips = NULL;
	gchar *config_file = NULL;
	GeanyKeyGroup *key_group;

	tooltips = gtk_tooltips_new();

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	config_file = g_strconcat(geany->app->configdir,
		G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"geanylipsum", G_DIR_SEPARATOR_S, "lipsum.conf", NULL);

	/* Initialising options from config file  if there is any*/
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);
	lipsum = utils_get_setting_string(config, "snippets", "lipsumtext", default_loremipsum);

	g_key_file_free(config);
	g_free(config_file);

	/* Building menu entry */
	menu_lipsum = gtk_image_menu_item_new_with_mnemonic(_("_Lipsum"));
	gtk_tooltips_set_tip(tooltips, menu_lipsum,
			     _("Include Pseudotext to your code"), NULL);
	gtk_widget_show(menu_lipsum);
	g_signal_connect((gpointer) menu_lipsum, "activate",
			 G_CALLBACK(lipsum_activated), NULL);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_lipsum);


	ui_add_document_sensitive(menu_lipsum);

	main_menu_item = menu_lipsum;

	/* init keybindings */
	key_group = plugin_set_key_group(geany_plugin, "geanylipsum", COUNT_KB, NULL);
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
