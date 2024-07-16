/*
 *  goto_file.c - this file is part of "codenavigation", which is
 *  part of the "geany-plugins" project.
 *
 *  Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
 *  Copyright 2014 Federico Reghenzani <federico(dot)dev(at)reghe(dot)net>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "goto_file.h"
#include "utils.h"

#define MAX_FILENAME_LENGTH 255

/******************* Global variables for the feature *****************/

static GtkWidget* menu_item = NULL;
static gchar *ref_dir = NULL;
static gchar *curr_dir = NULL;
static gint entry_len_before_completion = -1;

/********************** Prototypes *********************/
static void
menu_item_activate(guint);

static GtkTreeModel* 
build_file_list(const gchar*, const gchar*);

static void
directory_check(GtkEntry*, GtkEntryCompletion*);

static gboolean
entry_inline_completion_event(GtkEntryCompletion *, gchar *, GtkEntry *);

static gboolean
entry_key_event(GtkEntry *, GdkEventKey *, GtkEntryCompletion *);

static GtkWidget*
create_dialog(GtkWidget**);

/********************** Functions for the feature *********************/

/**
 * @brief 	Initialization function called in plugin_init
 * @param 	void
 * @return	void
 * 
 */
void
goto_file_init(void)
{
	GtkWidget* edit_menu;

	log_func();

	edit_menu = ui_lookup_widget(geany->main_widgets->window, "edit1_menu");

	/* Add the menu item, sensitive only when a document is opened */
	menu_item = gtk_menu_item_new_with_mnemonic(_("Go to File..."));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(edit_menu), menu_item);
	ui_add_document_sensitive(menu_item);

	/* Callback connection */
	g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(menu_item_activate), NULL);

	/* Initialize the key binding : */
	keybindings_set_item(	plugin_key_group,
 							KEY_ID_GOTO_FILE,
 							(GeanyKeyCallback)(&menu_item_activate),
 							GDK_g, GDK_MOD1_MASK | GDK_SHIFT_MASK,
 							"goto_file",
 							_("Go to File"),	/* used in the Preferences dialog */
 							menu_item);
}

/**
 * @brief 	Cleanup function called in plugin_cleanup
 * @param 	void
 * @return	void
 * 
 */
void
goto_file_cleanup(void)
{
	log_func();
	gtk_widget_destroy(menu_item);
}

/**
 * @brief 	Replace any special characters present in path_name
 * @param 	gchar** path_name
 * @return	void
 * 
 */
static void
path_name_replace_special(gchar **path_name)
{
	GString *str;

	if(*path_name[0] == '~')
	{
		str = g_string_new(*path_name);
		utils_string_replace_first(str, "~", g_get_home_dir());

		free(*path_name);
		*path_name = g_string_free(str, FALSE);
	}
}

/**
 * @brief 	Populate the file list with file list of directory 
 * @param 	const char* dirname	the directory where to find files
 * @param	const char* prefix	file prefix (the path)
 * @return	GtkTreeModel*
 * 
 */
static GtkTreeModel* 
build_file_list(const gchar* dirname, const gchar* prefix)
{
	GtkListStore *ret_list;
	GtkTreeIter iter;
	ret_list = gtk_list_store_new (1, G_TYPE_STRING);

	GSList* file_iterator;
	GSList* files_list; /* used to later free the sub-elements */
	gchar *file;
	guint files_n;

	files_list = file_iterator = utils_get_file_list(dirname, &files_n, NULL);

	for( ; file_iterator; file_iterator = file_iterator->next) 
	{
		file = file_iterator->data;

		gchar *full_path = g_build_filename(dirname, file, NULL);
		gboolean is_dir = g_file_test(full_path, G_FILE_TEST_IS_DIR);

		/* Append the element to model list */
		gtk_list_store_append (ret_list, &iter);
		gtk_list_store_set (ret_list, &iter, 0, g_strconcat(prefix,
			file, (is_dir ? "/" : NULL), NULL), -1);

		g_free(full_path);
	}

	g_slist_foreach(files_list, (GFunc) g_free, NULL);
	g_slist_free(files_list);

	return GTK_TREE_MODEL(ret_list);
}

/**
 * @brief 	Entry callback function for sub-directory search 
 * @param 	GtkEntry* entry			entry object
 * @param	GtkEntryCompletion* completion	completion object
 * @return	void
 * 
 */
static void
directory_check(GtkEntry* entry, GtkEntryCompletion* completion)
{
	GtkTreeModel* completion_list;
	gchar *text, *new_dir, *new_dir_exp, *new_dir_path;

	/* We need to discern between text written by the user and text filled by the
	 * autocomplete, and to only use what the user wrote for the completion model.
	 * Otherwise, when a directory is 'suggested', the completion model will start
	 * traversing it, even though the user has not yet 'accepted' the suggestion.
	 * One way to tell apart the autocompletion is to check if there is a selection.
	 * However, the "changed" event triggers before the selection occurs, so no dice.
	 * Instead, we watch the completion event (insert-prefix) and back-up (in entry_
	 * inline_completion_event) the user's text. */
	text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, entry_len_before_completion);
	entry_len_before_completion = -1;

	/* We are only interested in the directory part of the path that the
	 * user has given, to generate a file list for a completion model. */
	gint dir_sep = strrpos(text, G_DIR_SEPARATOR_S);
	new_dir = g_strndup(text, dir_sep + 1);
	g_free(text);

	/* We only create a new completion model when the directory changes */
	if(g_strcmp0(new_dir, curr_dir) == 0)
	{
		g_free(new_dir);
		return;
	}

	log_debug("New completion list!");

	g_free(curr_dir);
	curr_dir = new_dir;

	/* Replace special chars here (e.g. '~') */
	new_dir_exp = g_strdup(new_dir);
	path_name_replace_special(&new_dir_exp);

	/* Assemble full path to the entered directory */
	new_dir_path = (g_path_is_absolute(new_dir_exp) ? g_strdup(new_dir_exp)
		: g_build_filename(ref_dir, new_dir_exp, NULL));

	/* Build the new file list for completion */
	completion_list = build_file_list(new_dir_path, new_dir);
	gtk_entry_completion_set_model (completion, completion_list);
	g_object_unref(completion_list);

	g_free(new_dir_exp);
	g_free(new_dir_path);
}

/**
 * @brief 	GtkEntryCompletion insert-prefix callback
 * @param 	GtkEntryCompletion *completion entry completion object
 * @param	gchar *prefix common prefix of all possible completions
 * @param	GtkEntry *entry attached data (path entry field object)
 * @return	gboolean whether the signal has been handled (i.e. run other handlers?)
 * 
 */
static gboolean
entry_inline_completion_event(GtkEntryCompletion *completion, gchar *prefix, GtkEntry *entry)
{
	gint entry_len = gtk_entry_get_text_length(entry);
	gint prefix_len = g_utf8_strlen(prefix, -1);

	/* We want to mark the length of the entry before the completion, but
	 * only if an inline completion is actually about to be suggested. If
	 * a suggestion is not created, the entry text won't be modified, the
	 * 'changed' event won't fire, and thus entry_len_before_completion's
	 * value will be stale, as it won't get cleared in directory_check. */
	if(prefix_len > entry_len)
	{
		entry_len_before_completion = entry_len;
	}

	return FALSE;
}

/**
 * @brief 	Widget key callback (path entry field)
 * @param	GtkEntry *entry
 * @param 	GdkEventKey *key key that triggered event
 * @param	GtkEntryCompletion *completion attached data (entry completion object)
 * @return	gboolean whether the signal has been handled (i.e. run other handlers?)
 * 
 */
static gboolean
entry_key_event(GtkEntry *entry, GdkEventKey *key, GtkEntryCompletion *completion)
{
	gint end_pos;

	if(key->keyval == GDK_KEY_Tab)
	{
		/* The user can use the Tab key to 'accept' the autocomplete's suggestion,
		 * like in shells, and in GtkFileChoose. If there's a suggestion, it will
		 * be selected, and we may simply move the cursor to accept it. */
		if(gtk_editable_get_selection_bounds(GTK_EDITABLE(entry), NULL, &end_pos))
			gtk_editable_set_position(GTK_EDITABLE(entry), end_pos);

		/* The Tab key may also be used to trigger the autocomplete list
		 * to show up. And this trigger right here after 'accepting' an
		 * autocomplete suggestion is also useful in order to calculate
		 * and show completions for the new path. */
		g_signal_emit_by_name(entry, "changed");

		/* The 2nd purpose of the Tab key is to trigger the procedure that
		 * calculates and suggests an inline completion. FYI doing this here
		 * is still useful even if the Tab key was used to accept a suggestion
		 * above, as it will trigger an attempt for inline completion in the
		 * new/next directory. */
		gtk_entry_completion_insert_prefix(completion);

		/* Effectively reserve the Tab key for autocompletion purposes,
		 * so don't let any other handlers run. Do this even when there
		 * wasn't any actual suggestion, to make it safe for the user
		 * to 'spam' the key (mimics GtkFileChooser). */
		return TRUE;
	}
	else if(key->keyval == GDK_KEY_Return)
	{
		if(gtk_editable_get_selection_bounds(GTK_EDITABLE(entry), NULL, &end_pos))
		{
			gtk_editable_set_position(GTK_EDITABLE(entry), end_pos);
			g_signal_emit_by_name(entry, "changed");
			gtk_entry_completion_insert_prefix(completion);

			/* Unlike with the Tab key, only intercept Enter
			 * when there was a completion available. */
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * @brief 	Create the dialog, return the entry object to get the
 * 		response from user 
 * @param 	GtkWidget **dialog			entry object
 * @return	GtkWidget* entry
 * 
 */
static GtkWidget*
create_dialog(GtkWidget **dialog)
{
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkEntryCompletion *completion;

	*dialog = gtk_dialog_new_with_buttons(_("Go to File..."), GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(*dialog), GTK_RESPONSE_ACCEPT);

	gtk_widget_set_name(*dialog, "GotoFile");
	vbox = ui_dialog_vbox_new(GTK_DIALOG(*dialog));

	label = gtk_label_new(_("Enter the file you want to open:"));
	gtk_container_add(GTK_CONTAINER(vbox), label);

	/* Entry definition */
	entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(vbox), entry);
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_FILENAME_LENGTH);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);   /* 'enter' key */

	/* Completion definition */
	completion = gtk_entry_completion_new();

	/* Installed here, so it'll get installed before the respective
	 * handler inside GtkEntryCompletion, and will thus be called first. */
	g_signal_connect(entry, "key-press-event",
		G_CALLBACK(entry_key_event), completion);

	gtk_entry_set_completion(GTK_ENTRY(entry), completion);

	/* Completion options */
	gtk_entry_completion_set_inline_completion(completion, 1);
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_completion_set_minimum_key_length(completion, 0);

	/* Initialize a model for the completion, to avoid errors in code that
	 * might attempt to utilize it, before any 'changed' events are fired. */
	directory_check(GTK_ENTRY(entry), completion);

	/* Manual trigger to show the completion list as soon as the dialog opens */
	g_signal_emit_by_name(entry, "changed");

	/* Signals */
	g_signal_connect_after(entry, "changed",
		G_CALLBACK(directory_check), completion);
	g_signal_connect(completion, "insert-prefix",
		G_CALLBACK(entry_inline_completion_event), entry);

	/* The completion object is tracked in the entry. We may release our local
	 * reference, and it will be deallocated when the entry is destroyed. */
	g_object_unref(completion);

	gtk_widget_show_all(*dialog);

	return entry;
}

/**
 * @brief 	Callback when the menu item is clicked.
 * @param 	guint key_id	not used
 * @return	void
 * 
 */
static void
menu_item_activate(guint key_id)
{
	GtkWidget* dialog;
	GtkWidget* dialog_new = NULL;
	GtkWidget* dialog_entry;
	GeanyDocument* current_doc = document_get_current();
	const gchar *chosen_file;
	gchar *chosen_file_exp, *chosen_path;
	gint response;

	log_func();

	if(current_doc && !EMPTY(current_doc->file_name))
		ref_dir = g_path_get_dirname(current_doc->file_name);
	else
		ref_dir = g_strdup(g_get_home_dir());

	/* Create the user dialog and get response */
	dialog_entry = create_dialog(&dialog);

_show_dialog:
	response = gtk_dialog_run(GTK_DIALOG(dialog));

	chosen_file = gtk_entry_get_text(GTK_ENTRY(dialog_entry));

	chosen_file_exp = g_strdup(chosen_file);
	path_name_replace_special(&chosen_file_exp);

	chosen_path = (g_path_is_absolute(chosen_file_exp) ? g_strdup(chosen_file_exp)
		: g_build_filename(ref_dir, chosen_file_exp, NULL));

	if ( response == GTK_RESPONSE_ACCEPT )
	{
		log_debug("Trying to open: %s", chosen_path);
		if ( ! g_file_test(chosen_path, G_FILE_TEST_EXISTS) )
		{
			log_debug("File not found.");

			dialog_new = gtk_message_dialog_new(GTK_WINDOW(geany_data->main_widgets->window),
													GTK_DIALOG_MODAL,
													GTK_MESSAGE_QUESTION,
													GTK_BUTTONS_OK_CANCEL,
													_("%s not found, create it?"), chosen_path);
			gtk_window_set_title(GTK_WINDOW(dialog_new), "Geany");
			response = gtk_dialog_run(GTK_DIALOG(dialog_new));

			if (response == GTK_RESPONSE_OK)
			{
				document_new_file(chosen_path, (current_doc ?
					current_doc->file_type : NULL), NULL);
				document_set_text_changed(document_get_current(), TRUE);
			}

			gtk_widget_destroy(dialog_new);

			/* File wasn't found and user denied creating it,
			 * go back to the initial "Go to file" dialog. */
			if (response != GTK_RESPONSE_OK)
			{
				g_free(chosen_path);
				goto _show_dialog;
			}
		}
		else if(g_file_test(chosen_path, G_FILE_TEST_IS_DIR))
		{
			log_debug("File is a dir.");

			dialog_new = gtk_message_dialog_new(GTK_WINDOW(geany_data->main_widgets->window),
			                                    GTK_DIALOG_MODAL,
			                                    GTK_MESSAGE_ERROR,
			                                    GTK_BUTTONS_CLOSE,
			                                    _("%s is a directory."), chosen_path);
			gtk_window_set_title(GTK_WINDOW(dialog_new), "Geany");

			gtk_dialog_run(GTK_DIALOG(dialog_new));
			gtk_widget_destroy(dialog_new);

			goto _show_dialog;
		}
		else
			document_open_file(chosen_path, FALSE, NULL, NULL);
	}

	/* Freeing memory */

	gtk_widget_destroy(dialog);

	g_free(chosen_file_exp);
	g_free(chosen_path);

	g_free(ref_dir);
	ref_dir = NULL;

	g_free(curr_dir);
	curr_dir = NULL;
}
