/*
 * Copyright 2017 LarsGit223
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * This file contains all the code for handling the "Search projects"
 * menu item/action.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "wb_globals.h"
#include "dialogs.h"
#include "sidebar.h"
#include "menu.h"


typedef enum
{
	SCAN_DIR_STATE_ENTER,
	SCAN_DIR_STATE_CONTINUE,
}SCAN_DIR_STATE;


enum
{
	SEARCH_PROJECTS_COLUMN_IMPORT,
	SEARCH_PROJECTS_COLUMN_PATH,
	SEARCH_PROJECTS_N_COLUMNS,
};


typedef struct
{
	gchar *path;
	gchar *locale_path;
	gchar *real_path;
	GDir *dir;
}SCAN_DIR_STATE_DATA;


typedef struct
{
	SCAN_DIR_STATE state;
	gchar *searchdir;
	glong prj_count;
	GHashTable *visited_paths;
	GPtrArray *data;
}SCAN_DIR_PARAMS;


typedef struct S_SEARCH_PROJECTS_DIALOG
{
	gboolean stopped;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *label_dir;
	GtkWidget *list_vbox;
	GtkWidget *list_view;
	GtkListStore *list_store;
	SCAN_DIR_PARAMS *params;
}SEARCH_PROJECTS_DIALOG;


/** Shows the dialog "Select search directory".
 *
 * The dialog lets the user choose an existing folder.
 *
 * @return The filename
 *
 **/
static gchar *dialogs_select_search_directory(void)
{
	gchar *filename = NULL;
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new(_("Select search directory"),
		GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Add"), GTK_RESPONSE_ACCEPT, NULL);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}

	gtk_widget_destroy(dialog);

	return filename;
}


/* Free data used during searching.
   Freeing the widgets is done by gtk through calling 'destroy()'. */
static void search_projects_free_data(SEARCH_PROJECTS_DIALOG *data)
{
	if (data->params != NULL)
	{
		g_free(data->params->searchdir);
		g_hash_table_destroy(data->params->visited_paths);
		g_ptr_array_free(data->params->data, FALSE);
		g_free(data->params);
		data->params = NULL;
	}

	g_free(data);
}


/* Callback function for clicking on a list item */
static void list_view_on_row_activated (GtkTreeView *treeview,
	GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *col, G_GNUC_UNUSED gpointer userdata)
{
	gboolean value;
	GtkTreeModel *model;
	GtkTreeIter iter;
	SEARCH_PROJECTS_DIALOG *data = userdata;

	model = gtk_tree_view_get_model(treeview);

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_tree_model_get(model, &iter, SEARCH_PROJECTS_COLUMN_IMPORT, &value, -1);
		gtk_list_store_set(data->list_store, &iter,
			SEARCH_PROJECTS_COLUMN_IMPORT, !value,
			-1);
	}
}

static void search_projects_shutdown(SEARCH_PROJECTS_DIALOG *data)
{
	/* Close and free dialog. */
	gtk_widget_destroy(GTK_WIDGET(data->dialog));

	menu_set_context(MENU_CONTEXT_WB_OPENED);

	/* Free data. */
	search_projects_free_data(data);
}


/* Handle "OK" and "Cancel" button. */
static void dialog_on_button_pressed(GtkDialog *dialog, gint response_id,
									 gpointer user_data)
{
	gboolean value;
	gchar *filename;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GError *error = NULL;
	SEARCH_PROJECTS_DIALOG *data = user_data;

	if (response_id == GTK_RESPONSE_ACCEPT)
	{
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(data->list_view));
		if (gtk_tree_model_get_iter_first(model, &iter))
		{
			do
			{
				gtk_tree_model_get(model, &iter, SEARCH_PROJECTS_COLUMN_IMPORT, &value, -1);
				if (value == TRUE)
				{
					gtk_tree_model_get(model, &iter, SEARCH_PROJECTS_COLUMN_PATH, &filename, -1);
					workbench_add_project(wb_globals.opened_wb, filename);
				}
			}while (gtk_tree_model_iter_next(model, &iter));
		}

		/* Save the workbench file (.geanywb). */
		if (!workbench_save(wb_globals.opened_wb, &error))
		{
			dialogs_show_msgbox(GTK_MESSAGE_INFO, _("Could not save workbench file: %s"), error->message);
		}
		sidebar_update(SIDEBAR_CONTEXT_PROJECT_ADDED, NULL);
	}


	if (response_id == GTK_RESPONSE_ACCEPT ||
		response_id == GTK_RESPONSE_CANCEL ||
		data->stopped == TRUE)
	{
		search_projects_shutdown(data);
	}
	else
	{
		/* Set stop marker (scanning might still be in progress). */
		data->stopped = TRUE;
	}
}


/* Push scan data on the stack. */
static void scan_dir_data_push(SCAN_DIR_PARAMS *params, gchar *path, 
	gchar *locale_path, gchar *real_path, GDir *dir)
{
	SCAN_DIR_STATE_DATA *state_data;

	state_data = g_new0(SCAN_DIR_STATE_DATA, 1);
	state_data->path = path;
	state_data->locale_path = locale_path;
	state_data->real_path = real_path;
	state_data->dir = dir;

	g_ptr_array_add(params->data, state_data);
}


/* Pop scan data from the stack. */
static void scan_dir_data_pop(SCAN_DIR_PARAMS *params)
{
	SCAN_DIR_STATE_DATA *state_data;

	if (params->data->len > 0)
	{
		state_data = params->data->pdata[params->data->len - 1];
		g_free(state_data->path);
		g_free(state_data->locale_path);
		g_free(state_data->real_path);

		g_ptr_array_remove_index(params->data, params->data->len - 1);
	}
}


/* The function needs to be called if scanning comes to an end (scanning
   done or an error occurred. */
static void search_projects_scan_directory_end (SEARCH_PROJECTS_DIALOG *data)
{
	gchar *text;

	if (data->stopped == FALSE)
	{
		text = g_strdup_printf(_("Found %lu project files in directory \"%s\".\nPlease select the projects to add to the workbench."),
			data->params->prj_count, data->params->searchdir);
		gtk_label_set_text((GtkLabel *)data->label, text);
		g_free(text);

		gtk_widget_destroy(data->label_dir);
		gtk_widget_set_sensitive(data->dialog, TRUE);

		data->stopped = TRUE;
	}
	else
	{
		/* Dialog has been deleted as timer was running. */
		search_projects_shutdown(data);
	}
}


/* Start/continue scanning of the directory. This is a timer callback
   function to enable working with geany during long scans instead of
   blocking everything. */
static gboolean search_projects_scan_directory_do_work (gpointer user_data)
{
	SEARCH_PROJECTS_DIALOG *data = user_data;
	SCAN_DIR_STATE_DATA *state_data;
	gchar *text;
	guint repeats = 0;

	if (data->stopped == TRUE ||
		data->params->data->len == 0)
	{
		/* Abort. */
		search_projects_scan_directory_end(data);
		return FALSE;
	}

	while (repeats < 50)
	{
		const gchar *locale_name;
		gchar *locale_filename, *utf8_filename, *utf8_name;
		GtkTreeIter iter;

		if (data->stopped == TRUE)
		{
			/* Abort. */
			search_projects_scan_directory_end(data);
			return FALSE;
		}

		repeats++;
		state_data = data->params->data->pdata[data->params->data->len - 1];

		if (data->params->state == SCAN_DIR_STATE_ENTER)
		{
			/* Data must already be on the stack. */

			/* Add missing state data. */
			state_data->locale_path = utils_get_locale_from_utf8(state_data->path);
			state_data->real_path = utils_get_real_path(state_data->locale_path);

			if (state_data->real_path &&
				g_hash_table_lookup(data->params->visited_paths, state_data->real_path))
			{
				/* Seen this already. Leave dir. */
				scan_dir_data_pop(data->params);
				data->params->state = SCAN_DIR_STATE_CONTINUE;
				return TRUE;
			}

			state_data->dir = g_dir_open(state_data->locale_path, 0, NULL);
			if (!state_data->dir || !state_data->real_path)
			{
				if (state_data->dir != NULL)
				{
					g_dir_close(state_data->dir);
				}

				/* Abort on error. */
				search_projects_scan_directory_end(data);
				return FALSE;
			}

			g_hash_table_insert(data->params->visited_paths, g_strdup(state_data->real_path), GINT_TO_POINTER(1));

			text = g_strdup_printf("%s", state_data->locale_path);
			gtk_label_set_text((GtkLabel *)data->label_dir, text);
			g_free(text);
		}


		if (data->params->data->len == 0)
		{
			/* Internal error. Abort. */
			search_projects_scan_directory_end(data);
			return FALSE;
		}
		data->params->state = SCAN_DIR_STATE_CONTINUE;
		state_data = data->params->data->pdata[data->params->data->len - 1];

		/* Get next directory entry. */
		locale_name = g_dir_read_name(state_data->dir);
		if (!locale_name)
		{
			g_dir_close(state_data->dir);
			scan_dir_data_pop(data->params);
			break;
		}

		utf8_name = utils_get_utf8_from_locale(locale_name);
		locale_filename = g_build_filename(state_data->locale_path, locale_name, NULL);
		utf8_filename = utils_get_utf8_from_locale(locale_filename);

		if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
		{
			scan_dir_data_push(data->params, g_strdup(locale_filename), NULL, NULL, NULL);
			data->params->state = SCAN_DIR_STATE_ENTER;
		}
		else if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR))
		{
			if (g_str_has_suffix(utf8_name, ".geany"))
			{
				data->params->prj_count++;
				gtk_list_store_append (data->list_store, &iter);
				gtk_list_store_set (data->list_store, &iter,
					SEARCH_PROJECTS_COLUMN_PATH, locale_filename,
					SEARCH_PROJECTS_COLUMN_IMPORT, FALSE,
					-1);
			}
		}

		g_free(utf8_filename);
		g_free(locale_filename);
		g_free(utf8_name);
	}

	if (data->params->data->len == 0)
	{
		/* Finished. */
		search_projects_scan_directory_end(data);
		return FALSE;
	}

	/* Continue. */
	return TRUE;
}


/* The function prepares the required data for scaning and starts a timer
   for calling 'search_projects_scan_directory_do_work()' periodically
   until work is done. */
static void search_projects_scan_directory_start (const gchar *searchdir, SEARCH_PROJECTS_DIALOG *data)
{
	SCAN_DIR_PARAMS *params;

	params = g_new0(SCAN_DIR_PARAMS, 1);

	params->state = SCAN_DIR_STATE_ENTER;
	params->searchdir = g_strdup(searchdir);
	params->prj_count = 0;
	params->visited_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	params->data = g_ptr_array_new();

	scan_dir_data_push(params, g_strdup(searchdir), NULL, NULL, NULL);
	data->params = params;

	menu_set_context(MENU_CONTEXT_SEARCH_PROJECTS_SCANING);
	plugin_timeout_add(wb_globals.geany_plugin, 1, search_projects_scan_directory_do_work, data);
}


/** Search for projects.
 * 
 * Search for projects to be added to the workbench. First the function
 * 'dialogs_select_search_directory()' is called to open the dialog
 * "Select search directory". Scanning then started by calling
 * 'search_projects_scan_directory_start()'. The dialog shows a list
 * of found project files ("*.geany") and is held insenstive until search
 * has finished. Then the user can check the projects we likes to add.
 * Clicking "OK" then makes the projects being added to the workbench.
 * Clicking "Cancel" or closing the dialog aborts everything and the
 * workbench is un-changed.
 *
 * @param wb The workbench to add the found/selected projects to.
 *
 **/
void search_projects(WORKBENCH *wb)
{
	gchar *directory;
	GtkWidget *content_area;
	GtkDialogFlags flags;
	GtkWidget *scrollwin;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;
	GList *focus_chain = NULL;
	SEARCH_PROJECTS_DIALOG *search_projects;

	/* First, the user needs to select a directory to scan. */
	directory = dialogs_select_search_directory();
	if (directory == NULL)
	{
		return;
	}

	search_projects = g_new0(SEARCH_PROJECTS_DIALOG, 1);

	/* Create the widgets */
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	search_projects->dialog = gtk_dialog_new_with_buttons(_("Search projects"),
		GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window),
		flags,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_ACCEPT,
		NULL);
	g_signal_connect(search_projects->dialog, "response", G_CALLBACK(dialog_on_button_pressed), search_projects);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG (search_projects->dialog));
	gtk_widget_set_sensitive(search_projects->dialog, FALSE);

	/* VBox for the list view */
	search_projects->vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(search_projects->vbox), 12);


	/* List view */
	search_projects->list_view = gtk_tree_view_new();
	g_signal_connect(search_projects->list_view, "row-activated", (GCallback)list_view_on_row_activated, search_projects);

	search_projects->list_store = gtk_list_store_new(SEARCH_PROJECTS_N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(search_projects->list_view), GTK_TREE_MODEL(search_projects->list_store));

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_spacing(column, 10);
	gtk_tree_view_column_set_resizable(column, FALSE);
	gtk_tree_view_column_set_title(column, _("Add to workbench?"));
	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "active", SEARCH_PROJECTS_COLUMN_IMPORT);
	gtk_tree_view_append_column(GTK_TREE_VIEW(search_projects->list_view), column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_spacing(column, 10);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_title(column, _("Project path"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", SEARCH_PROJECTS_COLUMN_PATH);
	gtk_tree_view_append_column(GTK_TREE_VIEW(search_projects->list_view), column);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(search_projects->list_view), TRUE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(search_projects->list_view), FALSE);

	ui_widget_modify_font_from_string(search_projects->list_view,
		wb_globals.geany_plugin->geany_data->interface_prefs->tagbar_font);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(search_projects->list_view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);

	/*Add label to vbox */
	search_projects->label = gtk_label_new (_("Scanning directory:"));
	gtk_box_pack_start(GTK_BOX(search_projects->vbox), search_projects->label, FALSE, FALSE, 6);

	/*Add label for current directory to vbox */
	search_projects->label_dir = gtk_label_new (NULL);
	gtk_box_pack_start(GTK_BOX(search_projects->vbox), search_projects->label_dir, FALSE, FALSE, 6);

	/*Add list view to vbox */
	focus_chain = g_list_prepend(focus_chain, search_projects->list_view);
	gtk_container_set_focus_chain(GTK_CONTAINER(search_projects->vbox), focus_chain);
	g_list_free(focus_chain);
	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrollwin, 400, 200);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_set_vexpand(scrollwin, TRUE);
#endif
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrollwin), search_projects->list_view);
	gtk_box_pack_start(GTK_BOX(search_projects->vbox), scrollwin, TRUE, TRUE, 0);

	gtk_widget_show_all(search_projects->vbox);

	gtk_container_add(GTK_CONTAINER (content_area), search_projects->vbox);
	gtk_widget_show_all(search_projects->dialog);

	search_projects_scan_directory_start(directory, search_projects);
	g_free(directory);

	return;
}
