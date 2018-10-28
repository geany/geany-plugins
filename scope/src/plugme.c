/*
 *  plugme.c
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include "common.h"

#include <gp_gtkcompat.h>

#include "geanyplugin.h"

extern GeanyData *geany_data;

/* This file must not depend on Scope */
#include "plugme.h"

static gchar *run_file_chooser(const gchar *title, GtkFileChooserAction action,
		const gchar *utf8_path)
{
	GtkWidget *dialog = gtk_file_chooser_dialog_new(title,
		GTK_WINDOW(geany->main_widgets->window), action,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
	gchar *locale_path;
	gchar *ret_path = NULL;

	gtk_widget_set_name(dialog, "GeanyDialog");
	locale_path = utils_get_locale_from_utf8(utf8_path);
	if (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
	{
		if (g_path_is_absolute(locale_path) && g_file_test(locale_path, G_FILE_TEST_IS_DIR))
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), locale_path);
	}
	else if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
	{
		if (g_path_is_absolute(locale_path))
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), locale_path);
	}
	g_free(locale_path);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		gchar *dir_locale;

		dir_locale = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		ret_path = utils_get_utf8_from_locale(dir_locale);
		g_free(dir_locale);
	}
	gtk_widget_destroy(dialog);
	return ret_path;
}

static void ui_path_box_open_clicked(G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
	GtkWidget *path_box = GTK_WIDGET(user_data);
	GtkFileChooserAction action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(path_box), "action"));
	GtkEntry *entry = g_object_get_data(G_OBJECT(path_box), "entry");
	const gchar *title = g_object_get_data(G_OBJECT(path_box), "title");
	gchar *utf8_path = NULL;

	/* TODO: extend for other actions */
	g_return_if_fail(action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
					 action == GTK_FILE_CHOOSER_ACTION_OPEN);

	if (title == NULL)
		title = (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ?
			_("Select Folder") : _("Select File");

	if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
	{
		utf8_path = run_file_chooser(title, action, gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	else if (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
	{
		gchar *path = g_path_get_dirname(gtk_entry_get_text(GTK_ENTRY(entry)));
		utf8_path = run_file_chooser(title, action, path);
		g_free(path);
	}

	if (utf8_path != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), utf8_path);
		g_free(utf8_path);
	}
}

/* Setup a GtkButton to run a GtkFileChooser, setting entry text if successful.
 * title can be NULL.
 * action is the file chooser mode to use. */
void plugme_ui_setup_open_button_callback(GtkWidget *open_btn, const gchar *title,
		GtkFileChooserAction action, GtkEntry *entry)
{
	GtkWidget *path_entry = GTK_WIDGET(entry);

	if (title)
		g_object_set_data_full(G_OBJECT(open_btn), "title", g_strdup(title),
				(GDestroyNotify) g_free);
	g_object_set_data(G_OBJECT(open_btn), "action", GINT_TO_POINTER(action));
	ui_hookup_widget(open_btn, path_entry, "entry");
	g_signal_connect(open_btn, "clicked", G_CALLBACK(ui_path_box_open_clicked), open_btn);
}

/* Note: this is NOT the Geany function, only similar */
gchar *plugme_editor_get_default_selection(GeanyEditor *editor, gboolean use_current_word,
									const gchar *wordchars)
{
	ScintillaObject *sci = editor->sci;
	gchar *text = NULL;

	if (sci_has_selection(sci))
	{
		if (sci_get_selected_text_length(sci) < GEANY_MAX_WORD_LENGTH)
		{
			text = sci_get_selection_contents(sci);

			if (strchr(text, '\n') != NULL)
				*strchr(text, '\n') = '\0';
		}
	}
	else if (use_current_word)
		text = editor_get_word_at_pos(editor, sci_get_current_position(sci), wordchars);

	return text;
}

static void on_config_file_clicked(G_GNUC_UNUSED GtkWidget *widget, gpointer user_data)
{
	const gchar *file_name = user_data;
	GeanyFiletype *ft = NULL;

	if (strstr(file_name, G_DIR_SEPARATOR_S "filetypes."))
		ft = filetypes_index(GEANY_FILETYPES_CONF);

	if (g_file_test(file_name, G_FILE_TEST_EXISTS))
		document_open_file(file_name, FALSE, ft, NULL);
	else
	{
		gchar *utf8_filename = utils_get_utf8_from_locale(file_name);
		gchar *base_name = g_path_get_basename(file_name);
		gchar *global_file = g_build_filename(geany->app->datadir, base_name, NULL);
		gchar *global_content = NULL;

		/* if the requested file doesn't exist in the user's config dir, try loading the file
		 * from the global data directory and use its contents for the newly created file */
		if (g_file_test(global_file, G_FILE_TEST_EXISTS))
			g_file_get_contents(global_file, &global_content, NULL, NULL);

		document_new_file(utf8_filename, ft, global_content);

		g_free(utf8_filename);
		g_free(base_name);
		g_free(global_file);
		g_free(global_content);
	}
}

static void free_on_closure_notify(gpointer data, G_GNUC_UNUSED GClosure *closure)
{
	g_free(data);
}

GtkWidget *plugme_ui_add_config_file_menu_item(const gchar *real_path, const gchar *label,
		GtkContainer *parent)
{
	GtkWidget *item;

	if (!parent)
	{
		item = ui_lookup_widget(geany->main_widgets->window, "configuration_files1");
		parent = GTK_CONTAINER(gtk_menu_item_get_submenu(GTK_MENU_ITEM(item)));
	}

	if (!label)
	{
		gchar *base_name;

		base_name = g_path_get_basename(real_path);
		item = gtk_menu_item_new_with_label(base_name);
		g_free(base_name);
	}
	else
		item = gtk_menu_item_new_with_mnemonic(label);

	gtk_widget_show(item);
	gtk_container_add(parent, item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_config_file_clicked),
			g_strdup(real_path), free_on_closure_notify, 0);

	return item;
}
