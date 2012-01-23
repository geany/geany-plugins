/*
 *			ao_wrapwords.c - this file is part of Addons, a Geany plugin
 *
 *			This program is free software; you can redistribute it and/or modify
 *			it under the terms of the GNU General Public License as published by
 *			the Free Software Foundation; either version 2 of the License, or
 *			(at your option) any later version.
 *
 *			This program is distributed in the hope that it will be useful,
 *			but WITHOUT ANY WARRANTY; without even the implied warranty of
 *			MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *			GNU General Public License for more details.
 *
 *			You should have received a copy of the GNU General Public License
 *			along with this program; if not, write to the Free Software
 *			Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *			MA 02110-1301, USA.
 */

#include "geanyplugin.h"

#include "addons.h"
#include "ao_wrapwords.h"

enum
{
	COLUMN_TITLE,
	COLUMN_PRIOR_CHAR,
	COLUMN_END_CHAR,
	NUM_COLUMNS
};

void wrap_text_action (guint);
gboolean on_key_press (GtkWidget *, GdkEventKey *, gpointer);
void configure_response (GtkDialog *, gint, gpointer);
void wrap_chars_changed (GtkCellRendererText *, gchar *, gchar *, gpointer);

gchar *wrap_chars [8];
gboolean auto_enabled = FALSE;
gboolean wrap_enabled = FALSE;
gchar *config_file;
GtkListStore *chars_list;

/*
 * Called when a keybinding associated with the plugin is pressed.  Wraps the selected text in
 * the characters associated with this keybinding.
 */

void wrap_text_action (guint key_id)
{
	if (!wrap_enabled)
		return;

	gint selection_end;
	gchar insert_chars [2] = {0, 0};
	ScintillaObject *sci_obj = document_get_current ()->editor->sci;

	if (sci_get_selected_text_length (sci_obj) < 2)
		return;

	key_id -= 4;
	selection_end = sci_get_selection_end (sci_obj);

	sci_start_undo_action (sci_obj);
	insert_chars [0] = *wrap_chars [key_id];
	sci_insert_text (sci_obj, sci_get_selection_start (sci_obj), insert_chars);
	insert_chars [0] = *(wrap_chars [key_id] + 1);
	sci_insert_text (sci_obj, selection_end + 1, insert_chars);
	sci_set_current_position (sci_obj, selection_end + 2, TRUE);
	sci_end_undo_action (sci_obj);
}

/*
 * Captures keypresses, if auto-wrapping automatically wraps selected text when certain
 * characters are pressed.
 */

gboolean on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	gint selection_end;
	gchar insert_chars [4] = {0, 0, 0, 0};
	ScintillaObject *sci_obj;

	if (!auto_enabled)
		return FALSE;

	sci_obj = document_get_current ()->editor->sci;

	if (sci_get_selected_text_length (sci_obj) < 2)
		return FALSE;

	switch (event->keyval)
	{
		case '(':
			insert_chars [0] = '(';
			insert_chars [2] = ')';
			break;
		case '[':
			insert_chars [0] = '[';
			insert_chars [2] = ']';
			break;
		case '{':
			insert_chars [0] = '{';
			insert_chars [2] = '}';
			break;
		case '\'':
			insert_chars [0] = '\'';
			insert_chars [2] = '\'';
			break;
		case '\"':
			insert_chars [0] = '\"';
			insert_chars [2] = '\"';
			break;
        case '`':
            insert_chars [0] = '`';
            insert_chars [2] = '`';
			break;
		default:
			return FALSE;
	}

	selection_end = sci_get_selection_end (sci_obj);

	sci_start_undo_action (sci_obj);
	sci_insert_text (sci_obj, sci_get_selection_start (sci_obj), insert_chars);
	sci_insert_text (sci_obj, selection_end + 1, insert_chars+2);
	sci_set_current_position (sci_obj, selection_end + 2, TRUE);
	sci_end_undo_action (sci_obj);

	return TRUE;
}

/*
 * Loads the wrapping characters from the config file and sets keybindings.
 */

void ao_wrapwords_init (gchar *config_file_name, GeanyKeyGroup *key_group)
{
	GKeyFile *config = g_key_file_new();
	gchar *key_name = g_malloc0 (6);
	gint i;

	config_file = g_strdup (config_file_name);
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	g_stpcpy (key_name, "Wrap_x");

	for (i = 0; i < 8; i++)
	{
		key_name [5] = (gchar) (i + '0');
		wrap_chars [i] = utils_get_setting_string (config, "addons", key_name, "  ");
		key_name [5] = (gchar) ((i + 1) + '0');
		keybindings_set_item (key_group, i+4, (GeanyKeyCallback) wrap_text_action, 0, 0, key_name,
			key_name, NULL);

	}

	plugin_signal_connect(geany_plugin, G_OBJECT(geany->main_widgets->window), "key-press-event",
			FALSE, G_CALLBACK(on_key_press), NULL);

	g_free (key_name);
}

void ao_wrapwords_set_enabled (gboolean enabled_w, gboolean enabled_a)
{
	auto_enabled = enabled_a;
	wrap_enabled = enabled_w;
}

/*
 * Called when the user closes the plugin preferences dialog.  If the user accepted or ok'd,
 * update the array of wrapping characters and config file with the new wrapping characters
 */

void configure_response (GtkDialog *dialog, gint response, gpointer char_tree_view)
{
	GtkTreeIter char_iter;
	GKeyFile *config = g_key_file_new();
	gchar *config_data = NULL;
	gchar *prior_char_str, *end_char_str;
	gchar *key_name = g_malloc0 (6);
	gint i;

	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_ACCEPT) {
		g_free (key_name);
		return;
	}

	gtk_tree_model_get_iter_first (GTK_TREE_MODEL(chars_list), &char_iter);

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	g_stpcpy (key_name, "Wrap_x");

	for (i = 0; i < 8; i++)
	{
		key_name [5] = (gchar) (i + '0');

		gtk_tree_model_get (GTK_TREE_MODEL(chars_list), &char_iter,
			COLUMN_PRIOR_CHAR, &prior_char_str, COLUMN_END_CHAR, &end_char_str, -1);
		*wrap_chars [i] = prior_char_str [0];
		*(wrap_chars [i] + 1) = end_char_str [0];
		gtk_tree_model_iter_next (GTK_TREE_MODEL(chars_list), &char_iter);

		g_key_file_set_string (config, "addons", key_name, wrap_chars [i]);
	}

	config_data = g_key_file_to_data (config, NULL, NULL);
	utils_write_file (config_file, config_data);

	g_free (prior_char_str);
	g_free (end_char_str);
	g_free (config_data);
	g_free (key_name);
	g_key_file_free(config);
}

/*
 * When the user changes an entry in the preferences dialog, this updates the list of
 * wrapping characters with the new entry
 */

void wrap_chars_changed (GtkCellRendererText *renderer, gchar *path, gchar *new_char_str,
	gpointer column_num)
{
	GtkTreeIter chars_iter;
	gchar new_chars [2] = {0, 0};
	new_chars [0] = new_char_str [0];
	gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (chars_list), &chars_iter, path);
	gtk_list_store_set (chars_list, &chars_iter, GPOINTER_TO_INT (column_num), new_chars, -1);
}

/*
 * Dialog box for setting wrap characters
 */

void ao_wrapwords_config (GtkButton *button, GtkWidget *config_window)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkTreeIter chars_iter;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *label_column, *char_one_column, *char_two_column;
	GtkTreeView *chars_tree_view;
	gchar insert_chars [2] = {0, 0};
	gchar *title;
	gint i;

	dialog = gtk_dialog_new_with_buttons(_("Plugins"), GTK_WINDOW(config_window),
						GTK_DIALOG_DESTROY_WITH_PARENT, "Accept", GTK_RESPONSE_ACCEPT,
						"Cancel", GTK_RESPONSE_CANCEL, "OK", GTK_RESPONSE_OK, NULL);

	vbox = ui_dialog_vbox_new (GTK_DIALOG (dialog));
	chars_list = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	renderer = gtk_cell_renderer_text_new ();
	chars_tree_view = (GtkTreeView *) gtk_tree_view_new ();

	for (i = 0; i < 8; i++)
	{
		gtk_list_store_append (chars_list, &chars_iter);
		title = g_strdup_printf (_("Wrap combo %d"), i + 1);
		gtk_list_store_set (chars_list, &chars_iter, COLUMN_TITLE, title, -1);
		insert_chars [0] = *wrap_chars [i];
		gtk_list_store_set (chars_list, &chars_iter, COLUMN_PRIOR_CHAR, insert_chars, -1);
		insert_chars [0] = *(wrap_chars [i] + 1);
		gtk_list_store_set (chars_list, &chars_iter, COLUMN_END_CHAR, insert_chars, -1);
	}

	label_column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", 0, NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	char_one_column = gtk_tree_view_column_new_with_attributes (_("Opening Character"), renderer,
		"text", COLUMN_PRIOR_CHAR, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (wrap_chars_changed),
		GINT_TO_POINTER (COLUMN_PRIOR_CHAR));

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	char_two_column = gtk_tree_view_column_new_with_attributes (_("Closing Character"), renderer,
		"text", COLUMN_END_CHAR, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (wrap_chars_changed),
		GINT_TO_POINTER (COLUMN_END_CHAR));

	gtk_tree_view_append_column (chars_tree_view, label_column);
	gtk_tree_view_append_column (chars_tree_view, char_one_column);
	gtk_tree_view_append_column (chars_tree_view, char_two_column);

	gtk_tree_view_set_model (chars_tree_view, GTK_TREE_MODEL (chars_list));
	gtk_box_pack_start(GTK_BOX(vbox), (GtkWidget *) chars_tree_view, FALSE, FALSE, 3);

	gtk_widget_show_all (vbox);
	g_signal_connect (dialog, "response", G_CALLBACK (configure_response), NULL);
	while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	g_free (title);
}

