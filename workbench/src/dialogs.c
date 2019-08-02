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
 * This file contains all the code for the dialogs.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "wb_globals.h"
#include "dialogs.h"
#include "utils.h"

extern GeanyPlugin *geany_plugin;


/** Shows the dialog "Create new file".
 *
 * The dialog lets the user create a new file (filter *).
 *
 * @param path The current folder
 * @return The filename
 *
 **/
gchar *dialogs_create_new_file(const gchar *path)
{
	gchar *filename = NULL;
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new(_("Create new file"),
		GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window), GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("C_reate"), GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	if (path != NULL)
	{
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), path);
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}

	gtk_widget_destroy(dialog);

	return filename;
}


/** Shows the dialog "Create new directory".
 *
 * The dialog lets the user create a new directory.
 *
 * @param path The current folder
 * @return The filename
 *
 **/
gchar *dialogs_create_new_directory(const gchar *path)
{
	gchar *filename = NULL;
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new(_("Create new directory"),
		GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window), GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("C_reate"), GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	if (path != NULL)
	{
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), path);
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}

	gtk_widget_destroy(dialog);

	return filename;
}


/** Shows the dialog "Create new workbench".
 *
 * The dialog lets the user create a new workbench file (filter *.geanywb).
 *
 * @return The filename
 *
 **/
gchar *dialogs_create_new_workbench(void)
{
	gchar *filename = NULL;
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new(_("Create new workbench"),
		GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window), GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("C_reate"), GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "new.geanywb");
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}

	gtk_widget_destroy(dialog);

	return filename;
}


/** Shows the dialog "Open workbench".
 *
 * The dialog lets the user choose an existing workbench file (filter *.geanywb).
 *
 * @return The filename
 *
 **/
gchar *dialogs_open_workbench(void)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new(_("Open workbench"),
		GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window), GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Open"), GTK_RESPONSE_ACCEPT, NULL);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Workbench files (.geanywb)"));
	gtk_file_filter_add_pattern(filter, "*.geanywb");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All Files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}

	gtk_widget_destroy(dialog);

	return filename;
}


/** Shows the dialog "Add project".
 *
 * The dialog lets the user choose an existing project file
 * (filter *.geany or *).
 *
 * @return The filename
 *
 **/
gchar *dialogs_add_project(void)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new(_("Add project"),
		GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window), GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Add"), GTK_RESPONSE_ACCEPT, NULL);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Project files (.geany)"));
	gtk_file_filter_add_pattern(filter, "*.geany");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All Files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}

	gtk_widget_destroy(dialog);

	return filename;
}


/** Shows the dialog "Add directory".
 *
 * The dialog lets the user choose an existing folder.
 *
 * @return The filename
 *
 **/
gchar *dialogs_add_directory(WB_PROJECT *project)
{
	gchar *filename = NULL;
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new(_("Add directory"),
		GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Add"), GTK_RESPONSE_ACCEPT, NULL);
	if (project != NULL)
	{
		const gchar *path;

		/* Set the current folder to the location of the project file */
		path = wb_project_get_filename(project);
		if (path != NULL)
		{
			gchar *dirname = g_path_get_dirname(path);
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dirname);
			g_free(dirname);
		}
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}

	gtk_widget_destroy(dialog);

	return filename;
}


/* Split patterns in str into gchar** */
static gchar **split_patterns(const gchar *str)
{
	GString *tmp;
	gchar **ret;
	gchar *input;

	input = g_strdup(str);

	g_strstrip(input);
	tmp = g_string_new(input);
	g_free(input);
	do {} while (utils_string_replace_all(tmp, "  ", " "));
	ret = g_strsplit(tmp->str, " ", -1);
	g_string_free(tmp, TRUE);
	return ret;
}

typedef struct S_DIALOG_DIR_SETTINGS_DATA
{
	GtkWidget *dialog;
	GtkWidget *w_label_file_patterns;
	GtkWidget *w_file_patterns;
	GtkWidget *w_label_ignored_dirs_patterns;
	GtkWidget *w_ignored_dirs_patterns;
	GtkWidget *w_label_ignored_file_patterns;
	GtkWidget *w_ignored_file_patterns;
	GtkWidget *w_scan_mode_wb;
	GtkWidget *w_scan_mode_git;
}DIALOG_DIR_SETTINGS_DATA;

static void button_filter_workbench_toggled(GtkToggleButton *togglebutton,
											gpointer user_data)
{
	DIALOG_DIR_SETTINGS_DATA *data;

	data = user_data;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->w_scan_mode_wb)))
	{
		gtk_widget_set_sensitive (data->w_label_file_patterns, TRUE);
		gtk_widget_set_sensitive (data->w_file_patterns, TRUE);
		gtk_widget_set_sensitive (data->w_label_ignored_dirs_patterns, TRUE);
		gtk_widget_set_sensitive (data->w_ignored_dirs_patterns, TRUE);
		gtk_widget_set_sensitive (data->w_label_ignored_file_patterns, TRUE);
		gtk_widget_set_sensitive (data->w_ignored_file_patterns, TRUE);
	}
	else
	{
		gtk_widget_set_sensitive (data->w_label_file_patterns, FALSE);
		gtk_widget_set_sensitive (data->w_file_patterns, FALSE);
		gtk_widget_set_sensitive (data->w_label_ignored_dirs_patterns, FALSE);
		gtk_widget_set_sensitive (data->w_ignored_dirs_patterns, FALSE);
		gtk_widget_set_sensitive (data->w_label_ignored_file_patterns, FALSE);
		gtk_widget_set_sensitive (data->w_ignored_file_patterns, FALSE);
	}
}

static void button_filter_git_toggled(GtkToggleButton *togglebutton,
									gpointer user_data)
{
	/* Nothing to do for now... */
}

/** Shows the directory settings dialog.
 *
 * The dialog lets the user edit the settings for file patterns, irnored file patterns and
 * ignored directories patterns. On accept the result is directly stored in @a directory.
 *
 * @param  project   The project to which directory belongs to
 * @param  directory Location of WB_PROJECT_DIR to store the settings into
 * @return TRUE if the settings have changed, FALSE otherwise
 *
 **/
gboolean dialogs_directory_settings(WB_PROJECT *project, WB_PROJECT_DIR *directory)
{
	DIALOG_DIR_SETTINGS_DATA *data;
	GtkWidget *label, *content_area;
	GtkWidget *vbox, *hbox, *hbox1;
#if GTK_CHECK_VERSION(3, 4, 0)
	GtkWidget *grid;
#else
	GtkWidget *table;
#endif
	GtkDialogFlags flags;
	gchar *file_patterns_old, *ignored_file_patterns_old, *ignored_dirs_patterns_old;
	WB_PROJECT_SCAN_MODE scan_mode_old, scan_mode;
	gchar *abs_path;
	gboolean changed;
	guint row = 0;

	data = g_new0(DIALOG_DIR_SETTINGS_DATA, 1);

	/* Create the widgets */
	flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
	data->dialog = gtk_dialog_new_with_buttons(_("Directory settings"),
											   GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window),
											   flags,
											   _("_Cancel"), GTK_RESPONSE_CANCEL,
											   _("_OK"), GTK_RESPONSE_ACCEPT,
											   NULL);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG (data->dialog));

	vbox = gtk_vbox_new(FALSE, 0);

#if GTK_CHECK_VERSION(3, 4, 0)
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
	gtk_grid_set_column_spacing (GTK_GRID(grid), 10);
#else
	table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
#endif

	data->w_scan_mode_wb = gtk_radio_button_new_with_label (NULL, _("filter files using the workbench plugin"));
	g_signal_connect(data->w_scan_mode_wb, "toggled",
			G_CALLBACK(button_filter_workbench_toggled), data);
	gtk_box_pack_start(GTK_BOX(vbox), data->w_scan_mode_wb, FALSE, FALSE, 6);

	data->w_label_file_patterns = gtk_label_new(_("File patterns:"));
#if GTK_CHECK_VERSION(3, 16, 0)
	gtk_label_set_xalign (GTK_LABEL(data->w_label_file_patterns), 0);
	gtk_label_set_yalign (GTK_LABEL(data->w_label_file_patterns), 0);
#else
	gtk_misc_set_alignment(GTK_MISC(data->w_label_file_patterns), 0, 0);
#endif
	data->w_file_patterns = gtk_entry_new();
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_grid_attach (GTK_GRID(grid), data->w_label_file_patterns, 0, row, 1, 1);
	gtk_grid_attach (GTK_GRID(grid), data->w_file_patterns, 1, row, 1, 1);
	gtk_widget_set_valign (data->w_label_file_patterns, GTK_ALIGN_BASELINE);
	gtk_widget_set_hexpand (data->w_label_file_patterns, FALSE);
	gtk_widget_set_valign (data->w_file_patterns, GTK_ALIGN_BASELINE);
	gtk_widget_set_hexpand (data->w_file_patterns, TRUE);
#else
	ui_table_add_row(GTK_TABLE(table), row + 1, data->w_label_file_patterns, data->w_file_patterns, NULL);
#endif
	row++;
	ui_entry_add_clear_icon(GTK_ENTRY(data->w_file_patterns));
	gtk_widget_set_tooltip_text(data->w_file_patterns,
		_("Space separated list of patterns that are used to identify files "
		  "that shall be displayed in the directory tree."));
	file_patterns_old = g_strjoinv(" ", wb_project_dir_get_file_patterns(directory));
	gtk_entry_set_text(GTK_ENTRY(data->w_file_patterns), file_patterns_old);

	data->w_label_ignored_file_patterns = gtk_label_new(_("Ignored file patterns:"));
#if GTK_CHECK_VERSION(3, 16, 0)
	gtk_label_set_xalign (GTK_LABEL(data->w_label_ignored_file_patterns), 0);
	gtk_label_set_yalign (GTK_LABEL(data->w_label_ignored_file_patterns), 0);
#else
	gtk_misc_set_alignment(GTK_MISC(data->w_label_ignored_file_patterns), 0, 0);
#endif
	data->w_ignored_file_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(data->w_ignored_file_patterns));
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_grid_attach (GTK_GRID(grid), data->w_label_ignored_file_patterns, 0, row, 1, 1);
	gtk_grid_attach (GTK_GRID(grid), data->w_ignored_file_patterns, 1, row, 1, 1);
	gtk_widget_set_valign (data->w_label_ignored_file_patterns, GTK_ALIGN_BASELINE);
	gtk_widget_set_hexpand (data->w_label_ignored_file_patterns, FALSE);
	gtk_widget_set_valign (data->w_ignored_file_patterns, GTK_ALIGN_BASELINE);
	gtk_widget_set_hexpand (data->w_ignored_file_patterns, TRUE);
#else
	ui_table_add_row(GTK_TABLE(table), row + 1, data->w_label_ignored_file_patterns, data->w_ignored_file_patterns, NULL);
#endif
	row++;
	gtk_widget_set_tooltip_text(data->w_ignored_file_patterns,
		_("Space separated list of patterns that are used to identify files "
		  "that shall not be displayed in the directory tree."));
	ignored_file_patterns_old = g_strjoinv(" ", wb_project_dir_get_ignored_file_patterns(directory));
	gtk_entry_set_text(GTK_ENTRY(data->w_ignored_file_patterns), ignored_file_patterns_old);

	data->w_label_ignored_dirs_patterns = gtk_label_new(_("Ignored directory patterns:"));
#if GTK_CHECK_VERSION(3, 16, 0)
	gtk_label_set_xalign (GTK_LABEL(data->w_label_ignored_dirs_patterns), 0);
	gtk_label_set_yalign (GTK_LABEL(data->w_label_ignored_dirs_patterns), 0);
#else
	gtk_misc_set_alignment(GTK_MISC(data->w_label_ignored_dirs_patterns), 0, 0);
#endif
	data->w_ignored_dirs_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(data->w_ignored_dirs_patterns));
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_grid_attach (GTK_GRID(grid), data->w_label_ignored_dirs_patterns, 0, row, 1, 1);
	gtk_grid_attach (GTK_GRID(grid), data->w_ignored_dirs_patterns, 1, row, 1, 1);
	gtk_widget_set_valign (data->w_label_ignored_dirs_patterns, GTK_ALIGN_BASELINE);
	gtk_widget_set_hexpand (data->w_label_ignored_dirs_patterns, FALSE);
	gtk_widget_set_valign (data->w_ignored_dirs_patterns, GTK_ALIGN_BASELINE);
	gtk_widget_set_hexpand (data->w_ignored_dirs_patterns, TRUE);
#else
	ui_table_add_row(GTK_TABLE(table), row + 1, data->w_label_ignored_dirs_patterns, data->w_ignored_dirs_patterns, NULL);
#endif
	row++;
	gtk_widget_set_tooltip_text(data->w_ignored_dirs_patterns,
		_("Space separated list of patterns that are used to identify directories "
		  "that shall not be scanned for source files."));
	ignored_dirs_patterns_old = g_strjoinv(" ", wb_project_dir_get_ignored_dirs_patterns(directory));
	gtk_entry_set_text(GTK_ENTRY(data->w_ignored_dirs_patterns), ignored_dirs_patterns_old);

#if GTK_CHECK_VERSION(3, 12, 0)
	gtk_widget_set_margin_start (grid, 32);
#endif
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 6);
#else
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 6);
#endif

	data->w_scan_mode_git = gtk_radio_button_new_with_label (
		gtk_radio_button_get_group(GTK_RADIO_BUTTON (data->w_scan_mode_wb)),
		_("filter files using git (.gitignore)"));
	g_signal_connect(data->w_scan_mode_git, "toggled",
			G_CALLBACK(button_filter_git_toggled), data);
	gtk_box_pack_start(GTK_BOX(vbox), data->w_scan_mode_git, FALSE, FALSE, 6);

	hbox1 = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Note: the patterns above affect only the workbench directory and are not used in the Find in Files\n"
	"dialog."));
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 6);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 6);

	/* Add the label, and show everything we’ve added */
	gtk_container_add(GTK_CONTAINER (content_area), label);
	gtk_container_add(GTK_CONTAINER (content_area), hbox);


	scan_mode_old = wb_project_dir_get_scan_mode(directory);
	abs_path = get_combined_path(wb_project_get_filename(project),
		wb_project_dir_get_base_dir(directory));
	if (!is_git_repository(abs_path))
	{
		/* Disable git filter option if the directory is not a git repository. */
		gtk_widget_set_sensitive (data->w_scan_mode_git, FALSE);
	}
	else
	{
		/* Both options are possible, set the active option from
		   the current settings. */
		if (scan_mode_old == WB_PROJECT_SCAN_MODE_WORKBENCH)
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->w_scan_mode_wb), TRUE);
		}
		else
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->w_scan_mode_git), TRUE);
		}
	}
	g_free(abs_path);


	gtk_widget_show_all(data->dialog);
	gint result = gtk_dialog_run(GTK_DIALOG(data->dialog));
	changed = FALSE;
	if (result == GTK_RESPONSE_ACCEPT)
	{
		const gchar *str;
		gchar **file_patterns, **ignored_dirs_patterns, **ignored_file_patterns;

		scan_mode = WB_PROJECT_SCAN_MODE_WORKBENCH;
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->w_scan_mode_git)))
		{
			scan_mode = WB_PROJECT_SCAN_MODE_GIT;
		}
		if (scan_mode_old != scan_mode)
		{
			changed = TRUE;
		}

		str = gtk_entry_get_text(GTK_ENTRY(data->w_file_patterns));
		if (g_strcmp0(str, file_patterns_old) != 0)
		{
			changed = TRUE;
		}
		file_patterns = split_patterns(str);

		str = gtk_entry_get_text(GTK_ENTRY(data->w_ignored_dirs_patterns));
		if (g_strcmp0(str, ignored_dirs_patterns_old) != 0)
		{
			changed = TRUE;
		}
		ignored_dirs_patterns = split_patterns(str);

		str = gtk_entry_get_text(GTK_ENTRY(data->w_ignored_file_patterns));
		if (g_strcmp0(str, ignored_file_patterns_old) != 0)
		{
			changed = TRUE;
		}
		ignored_file_patterns = split_patterns(str);

		wb_project_dir_set_scan_mode(project, directory, scan_mode);
		wb_project_dir_set_file_patterns(directory, file_patterns);
		wb_project_dir_set_ignored_dirs_patterns(directory, ignored_dirs_patterns);
		wb_project_dir_set_ignored_file_patterns(directory, ignored_file_patterns);

		g_strfreev(file_patterns);
		g_strfreev(ignored_dirs_patterns);
		g_strfreev(ignored_file_patterns);
	}

	g_free(file_patterns_old);
	g_free(ignored_file_patterns_old);
	g_free(ignored_dirs_patterns_old);
	gtk_widget_destroy(data->dialog);

	return changed;
}


/** Shows the workbench settings dialog.
 *
 * The dialog lets the user edit the settings for option "Rescan all projects on open".
 * On accept the result is directly stored in @a workbench.
 *
 * @param  workbench Location of WORKBENCH to store the settings into
 * @return TRUE if the settings have changed, FALSE otherwise
 *
 **/
gboolean dialogs_workbench_settings(WORKBENCH *workbench)
{
	gint result;
	GtkWidget *w_rescan_projects_on_open, *w_enable_live_update, *w_expand_on_hover;
	GtkWidget *dialog, *content_area, *w_enable_tree_lines, *w_auto_open_by_doc;
	GtkWidget *vbox, *hbox;
#if GTK_CHECK_VERSION(3, 4, 0)
	GtkWidget *grid;
#else
	GtkWidget *table;
#endif
	GtkDialogFlags flags;
	gboolean changed, rescan_projects_on_open, rescan_projects_on_open_old;
	gboolean enable_live_update, enable_live_update_old;
	gboolean expand_on_hover, expand_on_hover_old;
	gboolean enable_tree_lines, enable_tree_lines_old;
	gboolean enable_auto_open_by_doc, enable_auto_open_by_doc_old;

	/* Create the widgets */
	flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons(_("Workbench settings"),
										 GTK_WINDOW(wb_globals.geany_plugin->geany_data->main_widgets->window),
										 flags,
										 _("_Cancel"), GTK_RESPONSE_CANCEL,
										 _("_OK"), GTK_RESPONSE_ACCEPT,
										 NULL);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG (dialog));

	vbox = gtk_vbox_new(FALSE, 0);

#if GTK_CHECK_VERSION(3, 4, 0)
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
	gtk_grid_set_column_spacing (GTK_GRID(grid), 10);
#else
	table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
#endif

	w_rescan_projects_on_open = gtk_check_button_new_with_mnemonic(_("_Rescan all projects on open"));
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_grid_attach (GTK_GRID(grid), w_rescan_projects_on_open, 0, 0, 1, 1);
	gtk_widget_set_halign (w_rescan_projects_on_open, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand (w_rescan_projects_on_open, TRUE);
	gtk_widget_set_valign (w_rescan_projects_on_open, GTK_ALIGN_CENTER);
	gtk_widget_set_vexpand (w_rescan_projects_on_open, TRUE);
#else
	ui_table_add_row(GTK_TABLE(table), 0, w_rescan_projects_on_open, NULL);
#endif
	gtk_widget_set_tooltip_text(w_rescan_projects_on_open,
		_("If the option is activated (default), then all projects will be re-scanned "
		  "on opening of the workbench."));
	rescan_projects_on_open_old = workbench_get_rescan_projects_on_open(workbench);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w_rescan_projects_on_open), rescan_projects_on_open_old);

	w_enable_live_update = gtk_check_button_new_with_mnemonic(_("_Enable live update"));
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_grid_attach (GTK_GRID(grid), w_enable_live_update, 0, 1, 1, 1);
	gtk_widget_set_halign (w_enable_live_update, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand (w_enable_live_update, TRUE);
	gtk_widget_set_valign (w_enable_live_update, GTK_ALIGN_CENTER);
	gtk_widget_set_vexpand (w_enable_live_update, TRUE);
#else
	ui_table_add_row(GTK_TABLE(table), 1, w_enable_live_update, NULL);
#endif
	gtk_widget_set_tooltip_text(w_enable_live_update,
		_("If the option is activated (default), then the list of files and the sidebar "
		  "will be updated automatically if a file or directory is created, removed or renamed. "
		  "A manual re-scan is not required if the option is enabled."));
	enable_live_update_old = workbench_get_enable_live_update(workbench);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w_enable_live_update), enable_live_update_old);

	w_expand_on_hover = gtk_check_button_new_with_mnemonic(_("_Expand on hover"));
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_grid_attach (GTK_GRID(grid), w_expand_on_hover, 0, 2, 1, 1);
	gtk_widget_set_halign (w_expand_on_hover, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand (w_expand_on_hover, TRUE);
	gtk_widget_set_valign (w_expand_on_hover, GTK_ALIGN_CENTER);
	gtk_widget_set_vexpand (w_expand_on_hover, TRUE);
#else
	ui_table_add_row(GTK_TABLE(table), 2, w_expand_on_hover, NULL);
#endif
	gtk_widget_set_tooltip_text(w_expand_on_hover,
		_("If the option is activated, then a tree node in the sidebar "
		  "will be expanded or collapsed by hovering over it with the mouse cursor."));
	expand_on_hover_old = workbench_get_expand_on_hover(workbench);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w_expand_on_hover), expand_on_hover_old);

	w_enable_tree_lines = gtk_check_button_new_with_mnemonic(_("_Enable tree lines"));
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_grid_attach (GTK_GRID(grid), w_enable_tree_lines, 0, 3, 1, 1);
	gtk_widget_set_halign (w_enable_tree_lines, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand (w_enable_tree_lines, TRUE);
	gtk_widget_set_valign (w_enable_tree_lines, GTK_ALIGN_CENTER);
	gtk_widget_set_vexpand (w_enable_tree_lines, TRUE);
#else
	ui_table_add_row(GTK_TABLE(table), 3, w_enable_tree_lines, NULL);
#endif
	gtk_widget_set_tooltip_text(w_enable_tree_lines,
		_("If the option is activated, lines will be drawn between "
		  "the nodes in the sidebar tree."));
	enable_tree_lines_old = workbench_get_enable_tree_lines(workbench);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w_enable_tree_lines), enable_tree_lines_old);

	w_auto_open_by_doc = gtk_check_button_new_with_mnemonic(_("_Enable auto-open project by document"));
#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_grid_attach (GTK_GRID(grid), w_auto_open_by_doc, 0, 4, 1, 1);
	gtk_widget_set_halign (w_auto_open_by_doc, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand (w_auto_open_by_doc, TRUE);
	gtk_widget_set_valign (w_auto_open_by_doc, GTK_ALIGN_CENTER);
	gtk_widget_set_vexpand (w_auto_open_by_doc, TRUE);
#else
	ui_table_add_row(GTK_TABLE(table), 4, w_auto_open_by_doc, NULL);
#endif
	gtk_widget_set_tooltip_text(w_auto_open_by_doc,
		_("If the option is activated, a project will be auto-opened "
		  "if a document belonging to the project becomes active."));
	enable_auto_open_by_doc_old = workbench_get_enable_auto_open_by_doc(workbench);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w_auto_open_by_doc), enable_auto_open_by_doc_old);
#if GEANY_API_VERSION < 240
	gtk_widget_set_sensitive (w_auto_open_by_doc, FALSE);
#endif

#if GTK_CHECK_VERSION(3, 4, 0)
	gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 6);
#else
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 6);
#endif

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 6);

	/* Show everything we’ve added, run dialog */
	gtk_container_add(GTK_CONTAINER (content_area), hbox);
	gtk_widget_show_all(dialog);
	result = gtk_dialog_run(GTK_DIALOG(dialog));

	changed = FALSE;
	if (result == GTK_RESPONSE_ACCEPT)
	{
		rescan_projects_on_open = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w_rescan_projects_on_open));
		if (rescan_projects_on_open != rescan_projects_on_open_old)
		{
			changed = TRUE;
			workbench_set_rescan_projects_on_open(workbench, rescan_projects_on_open);
		}
		enable_live_update = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w_enable_live_update));
		if (enable_live_update != enable_live_update_old)
		{
			changed = TRUE;
			workbench_set_enable_live_update(workbench, enable_live_update);
		}
		expand_on_hover = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w_expand_on_hover));
		if (expand_on_hover != expand_on_hover_old)
		{
			changed = TRUE;
			workbench_set_expand_on_hover(workbench, expand_on_hover);
		}
		enable_tree_lines = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w_enable_tree_lines));
		if (enable_tree_lines != enable_tree_lines_old)
		{
			changed = TRUE;
			workbench_set_enable_tree_lines(workbench, enable_tree_lines);
		}
		enable_auto_open_by_doc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w_auto_open_by_doc));
		if (enable_auto_open_by_doc != enable_auto_open_by_doc_old)
		{
			changed = TRUE;
			workbench_set_enable_auto_open_by_doc(workbench, enable_auto_open_by_doc);
		}
	}

	gtk_widget_destroy(dialog);

	return changed;
}
