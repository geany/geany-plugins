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
gchar *directory_ref = NULL;

/********************** Prototypes *********************/
static void
menu_item_activate(guint);

static GtkTreeModel* 
build_file_list(const gchar*, const gchar*);

static void
directory_check(GtkEntry*, GtkEntryCompletion*);

static GtkWidget*
create_dialog(GtkWidget**, GtkTreeModel*);

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
	GSList* files_list;	/* used to free later the sub-elements*/
	gchar *file;
	gchar *pathfile;
	guint files_n;

	files_list = file_iterator = utils_get_file_list(dirname, &files_n, NULL);
	
	for( ; file_iterator; file_iterator = file_iterator->next) 
	{
		file = file_iterator->data;		
		
		pathfile = g_build_filename(dirname,file,NULL);   
	
        /* Append the element to model list */
        gtk_list_store_append (ret_list, &iter);
        gtk_list_store_set (ret_list, &iter, 0, 
                            g_strconcat(prefix, file, NULL), -1);
		g_free(pathfile);
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
    static GtkTreeModel *old_model = NULL;
   	GtkTreeModel* completion_list;
    static gchar *curr_dir = NULL;
    gchar *new_dir, *new_dir_path = NULL; 
    const gchar *text;
    
    text = gtk_entry_get_text(entry);
    gint dir_sep = strrpos(text, G_DIR_SEPARATOR_S);
    
    /* No subdir separator found */
    if (dir_sep == -1)
    {
        if (old_model != NULL)
        {   /* Restore the no-sub-directory model */
            log_debug("Restoring old model!");

            gtk_entry_completion_set_model (completion, old_model);
            g_object_unref(old_model);
            old_model = NULL;

            g_free(curr_dir);
            curr_dir = NULL;
        }
        return;
    }
    
    new_dir = g_strndup (text, dir_sep+1);
    /* I've already inserted new model completion for sub-dir elements? */
    if ( g_strcmp0 (new_dir, curr_dir) == 0 )
    {
        g_free(new_dir);
        return;
    }
    if ( curr_dir != NULL )
        g_free(curr_dir);

    curr_dir = new_dir;

    /* Save the completion_mode for future restore. */
    if (old_model == NULL)
    {
        old_model = gtk_entry_completion_get_model(completion);
        g_object_ref(old_model);
    }

    log_debug("New completion list!");

    if ( g_path_is_absolute(new_dir) )
        new_dir_path = g_strdup(new_dir);
    else
        new_dir_path = g_build_filename(directory_ref, new_dir, NULL);

    /* Build the new file list for completion */
    completion_list = build_file_list(new_dir_path, new_dir);
  	gtk_entry_completion_set_model (completion, completion_list);
    g_object_unref(completion_list);

    g_free(new_dir_path);
}


/**
 * @brief 	Create the dialog, return the entry object to get the
 * 		response from user 
 * @param 	GtkWidget **dialog			entry object
 * @param	GtkTreeModel *completion_model	completion object
 * @return	GtkWidget* entry
 * 
 */
static GtkWidget*
create_dialog(GtkWidget **dialog, GtkTreeModel *completion_model)
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
	gtk_entry_set_completion(GTK_ENTRY(entry), completion);
	gtk_entry_completion_set_model (completion, completion_model);
	
	/* Completion options */
	gtk_entry_completion_set_inline_completion(completion, 1);
	gtk_entry_completion_set_text_column (completion, 0);

	/* Signals */
	g_signal_connect_after(GTK_ENTRY(entry), "changed", 
                               G_CALLBACK(directory_check), completion);

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
	GtkTreeModel* completion_list;
	GeanyDocument* current_doc = document_get_current();
	gchar *chosen_path;
	const gchar *chosen_file;
	gint response;

	log_func();

	if(current_doc == NULL || current_doc->file_name == NULL || current_doc->file_name[0] == '\0')
		return;
		
	/* Build current directory listing */
	directory_ref = g_path_get_dirname(current_doc->file_name);
	completion_list = build_file_list(directory_ref, "");

	/* Create the user dialog and get response */
	dialog_entry = create_dialog(&dialog, completion_list);

_show_dialog:
	response = gtk_dialog_run(GTK_DIALOG(dialog));

	/* Filename */
	chosen_file = gtk_entry_get_text(GTK_ENTRY(dialog_entry));
	/* Path + Filename */
	chosen_path = g_build_filename(directory_ref, chosen_file, NULL);

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
													_("%s not found, create it?"), chosen_file);
			gtk_window_set_title(GTK_WINDOW(dialog_new), "Geany");
			response = gtk_dialog_run(GTK_DIALOG(dialog_new));

			if (response == GTK_RESPONSE_OK)
			{
				document_new_file(chosen_path, current_doc->file_type, NULL);
				document_set_text_changed(document_get_current(), TRUE);
			}

			gtk_widget_destroy(dialog_new);

			/* File wasn't found and user denied creating it,
			 * go back to the initial "Go to file" dialog. */
			if (response != GTK_RESPONSE_OK)
				goto _show_dialog;
		}
		else
			document_open_file(chosen_path, FALSE, NULL, NULL);
	}

	/* Freeing memory */
	gtk_widget_destroy(dialog);
	g_free(directory_ref);
	g_object_unref (completion_list);
}
