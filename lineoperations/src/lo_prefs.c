/*
 *      lo_prefs.c - Line operations, remove duplicate lines, empty lines,
 *                 lines with only whitespace, sort lines.
 *
 *      Copyright 2015 Sylvan Mostert <smostert.dev@gmail.com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifdef HAVE_CONFIG_H
    #include "config.h" /* for the gettext domain */
#endif

#include "lo_prefs.h"


static struct
{
	GtkWidget *collation_cb;
} config_widgets;


LineOpsInfo *lo_info = NULL;


/* handle button presses in the preferences dialog box */
void
lo_configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		gchar *config_dir = g_path_get_dirname(lo_info->config_file);
		gchar *data;

		/* Grabbing options that has been set */
		lo_info->use_collation_compare =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_widgets.collation_cb));

		/* Write preference to file */
		g_key_file_load_from_file(config, lo_info->config_file, G_KEY_FILE_NONE, NULL);

		g_key_file_set_boolean(config, "general", "use_collation_compare",
			lo_info->use_collation_compare);

		if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)
			&& utils_mkdir(config_dir, TRUE) != 0)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Plugin configuration directory could not be created."));
		}
		else
		{
			/* write config to file */
			data = g_key_file_to_data(config, NULL, NULL);
			utils_write_file(lo_info->config_file, data);
			g_free(data);
		}

		g_free(config_dir);
		g_key_file_free(config);
	}
}


/* Configure the preferences GUI and callbacks */
GtkWidget *
lo_configure(G_GNUC_UNUSED GeanyPlugin *plugin, GtkDialog *dialog, G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget *vbox;

	vbox = gtk_vbox_new(FALSE, 0);

	config_widgets.collation_cb = gtk_check_button_new_with_label(
		_("Use collation based string compare"));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_widgets.collation_cb),
		lo_info->use_collation_compare);

	gtk_box_pack_start(GTK_BOX(vbox), config_widgets.collation_cb, FALSE, FALSE, 2);


	gtk_widget_show_all(vbox);
	g_signal_connect(dialog, "response", G_CALLBACK(lo_configure_response_cb), NULL);

	return vbox;
}


/* Initialize preferences */
void
lo_init_prefs(GeanyPlugin *plugin)
{
	GeanyData *geany_data = plugin->geany_data;
	GKeyFile *config = g_key_file_new();

	/* load preferences from file into lo_info */
	lo_info = g_new0(LineOpsInfo, 1);
	lo_info->config_file = g_strconcat(geany->app->configdir,
		G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"lineoperations", G_DIR_SEPARATOR_S, "general.conf", NULL);

	g_key_file_load_from_file(config, lo_info->config_file, G_KEY_FILE_NONE, NULL);

	lo_info->use_collation_compare = utils_get_setting_boolean(config,
		"general", "use_collation_compare", FALSE);

	/* printf("VALUE: %d\n", lo_info->use_collation_compare); */

	g_key_file_free(config);
}


/* Free config */
void
lo_free_info() {
	g_free(lo_info->config_file);
	g_free(lo_info);
}
