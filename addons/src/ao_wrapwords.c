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

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <geanyplugin.h>

#include "addons.h"
#include "ao_wrapwords.h"

enum
{
	COLUMN_TITLE,
	COLUMN_PRIOR_CHAR,
	COLUMN_END_CHAR,
	NUM_COLUMNS
};

void enclose_text_action (guint);
gboolean on_key_press (GtkWidget *, GdkEventKey *, gpointer);
void configure_response (GtkDialog *, gint, gpointer);
void enclose_chars_changed (GtkCellRendererText *, gchar *, gchar *, gpointer);

gchar *enclose_chars [AO_WORDWRAP_KB_COUNT];
gboolean auto_enabled = FALSE;
gboolean enclose_enabled = FALSE;
gchar *config_file;
GtkListStore *chars_list;

/*
 * Called when a keybinding associated with the plugin is pressed.  Encloses the selected text in
 * the characters associated with this keybinding.
 */

void enclose_text_action (guint key_id)
{
	gint selection_end;
	gchar insert_chars [2] = {0, 0};
	ScintillaObject *sci_obj;

	if (!enclose_enabled)
		return;

	sci_obj = document_get_current ()->editor->sci;

	if (sci_get_selected_text_length2 (sci_obj) == 0)
		return;

	key_id -= KB_COUNT;
	selection_end = sci_get_selection_end (sci_obj);

	sci_start_undo_action (sci_obj);
	insert_chars [0] = *enclose_chars [key_id];
	sci_insert_text (sci_obj, sci_get_selection_start (sci_obj), insert_chars);
	insert_chars [0] = *(enclose_chars [key_id] + 1);
	sci_insert_text (sci_obj, selection_end + 1, insert_chars);
	sci_set_current_position (sci_obj, selection_end + 2, TRUE);
	sci_end_undo_action (sci_obj);
}

/*
 * Captures keypresses, if auto-enclosing automatically encloses selected text when certain
 * characters are pressed.
 */

gboolean on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	gint selection_end;
	gchar insert_chars [4] = {0, 0, 0, 0};
	ScintillaObject *sci_obj;

	if (!auto_enabled)
		return FALSE;

	if (document_get_current () == NULL)
		return FALSE;

	sci_obj = document_get_current ()->editor->sci;

	if (sci_get_selected_text_length2 (sci_obj) == 0)
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
 * Loads the enclosing characters from the config file and sets keybindings.
 */

void ao_enclose_words_init (gchar *config_file_name, GeanyKeyGroup *key_group, gint ao_kb_count)
{
	GKeyFile *config = g_key_file_new();
	gchar key_name[] = "Enclose_x";
	gint i;

	config_file = g_strdup (config_file_name);
	g_key_file_load_from_file (config, config_file, G_KEY_FILE_NONE, NULL);

	for (i = 0; i < AO_WORDWRAP_KB_COUNT; i++)
	{
		key_name [8] = (gchar) (i + '0');
		enclose_chars [i] = utils_get_setting_string (config, "addons", key_name, "  ");
		key_name [8] = (gchar) ((i + 1) + '0');
		keybindings_set_item (key_group, i + ao_kb_count,
			(GeanyKeyCallback) enclose_text_action, 0, 0, key_name, key_name, NULL);
	}

	g_key_file_free(config);

	plugin_signal_connect(geany_plugin, G_OBJECT(geany->main_widgets->window), "key-press-event",
			FALSE, G_CALLBACK(on_key_press), NULL);
}

void ao_enclose_words_set_enabled (gboolean enabled_w, gboolean enabled_a)
{
	auto_enabled = enabled_a;
	enclose_enabled = enabled_w;
}

/*
 * Called when the user closes the plugin preferences dialog.  If the user accepted or ok'd,
 * update the array of enclosing characters and config file with the new enclosing characters
 */

void configure_response (GtkDialog *dialog, gint response, gpointer char_tree_view)
{
	GtkTreeIter char_iter;
	GKeyFile *config;
	gchar *config_data = NULL;
	gchar key_name[] = "Enclose_x";
	gint i;

	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_ACCEPT)
		return;

	gtk_tree_model_get_iter_first (GTK_TREE_MODEL(chars_list), &char_iter);

	config = g_key_file_new();
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	for (i = 0; i < AO_WORDWRAP_KB_COUNT; i++)
	{
		gchar *prior_char_str, *end_char_str;

		key_name [8] = (gchar) (i + '0');

		gtk_tree_model_get (GTK_TREE_MODEL(chars_list), &char_iter,
			COLUMN_PRIOR_CHAR, &prior_char_str, COLUMN_END_CHAR, &end_char_str, -1);
		*enclose_chars [i] = prior_char_str [0];
		*(enclose_chars [i] + 1) = end_char_str [0];
		gtk_tree_model_iter_next (GTK_TREE_MODEL(chars_list), &char_iter);

		g_key_file_set_string (config, "addons", key_name, enclose_chars [i]);

		g_free (prior_char_str);
		g_free (end_char_str);
	}

	config_data = g_key_file_to_data (config, NULL, NULL);
	utils_write_file (config_file, config_data);

	g_free (config_data);
	g_key_file_free(config);
}

/*
 * When the user changes an entry in the preferences dialog, this updates the list of
 * enclosing characters with the new entry
 */

void enclose_chars_changed (GtkCellRendererText *renderer, gchar *path, gchar *new_char_str,
	gpointer column_num)
{
	GtkTreeIter chars_iter;
	gchar new_chars [2] = {0, 0};
	new_chars [0] = new_char_str [0];
	gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (chars_list), &chars_iter, path);
	gtk_list_store_set (chars_list, &chars_iter, GPOINTER_TO_INT (column_num), new_chars, -1);
}

/*
 * Dialog box for setting enclose characters
 */

void ao_enclose_words_config (GtkButton *button, GtkWidget *config_window)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkTreeIter chars_iter;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *label_column, *char_one_column, *char_two_column;
	GtkTreeView *chars_tree_view;
	gchar insert_chars [2] = {0, 0};
	gint i;

	dialog = gtk_dialog_new_with_buttons(_("Enclose Characters"), GTK_WINDOW(config_window),
						GTK_DIALOG_DESTROY_WITH_PARENT, _("Accept"), GTK_RESPONSE_ACCEPT,
						_("Cancel"), GTK_RESPONSE_CANCEL, _("OK"), GTK_RESPONSE_OK, NULL);

	vbox = ui_dialog_vbox_new (GTK_DIALOG (dialog));
	chars_list = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	renderer = gtk_cell_renderer_text_new ();
	chars_tree_view = (GtkTreeView *) gtk_tree_view_new ();

	for (i = 0; i < AO_WORDWRAP_KB_COUNT; i++)
	{
		gchar *title = g_strdup_printf (_("Enclose combo %d"), i + 1);

		gtk_list_store_append (chars_list, &chars_iter);
		gtk_list_store_set (chars_list, &chars_iter, COLUMN_TITLE, title, -1);
		insert_chars [0] = *enclose_chars [i];
		gtk_list_store_set (chars_list, &chars_iter, COLUMN_PRIOR_CHAR, insert_chars, -1);
		insert_chars [0] = *(enclose_chars [i] + 1);
		gtk_list_store_set (chars_list, &chars_iter, COLUMN_END_CHAR, insert_chars, -1);

		g_free(title);
	}

	label_column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", 0, NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	char_one_column = gtk_tree_view_column_new_with_attributes (_("Opening Character"), renderer,
		"text", COLUMN_PRIOR_CHAR, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (enclose_chars_changed),
		GINT_TO_POINTER (COLUMN_PRIOR_CHAR));

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	char_two_column = gtk_tree_view_column_new_with_attributes (_("Closing Character"), renderer,
		"text", COLUMN_END_CHAR, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (enclose_chars_changed),
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
}

