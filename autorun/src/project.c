/*
 * Copyright 2026 WrapEarnPass
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
#include <geanyplugin.h>

#include "autorun.h"
#include "project.h"
#include "utils.h"

// container for GtkWidget references
typedef struct {
	GtkWidget* notebook;
	GtkWidget* tab;
	GtkWidget* before_save_command;
	GtkWidget* before_save_workdir;
	GtkWidget* on_save_command;
	GtkWidget* on_save_workdir;
} AUTORUN_PROJECT_DATA;
static AUTORUN_PROJECT_DATA project_data;

/* callback to save changes to the Project to disk
 * config keyfile provided by Geany */
void project_save_properties_tab(GKeyFile* config) {
	// grab the current filetype
	if (!document_get_current()) {
		// Auto-run only supports file specific handlers
		return;
	}
	gchar* filetype_name = document_get_current()->file_type->name;
	gchar *before_save_command, *before_save_workdir, *on_save_command, *on_save_workdir;
	before_save_command = g_strdup(gtk_entry_get_text(GTK_ENTRY(project_data.before_save_command)));
	before_save_workdir = g_strdup(gtk_entry_get_text(GTK_ENTRY(project_data.before_save_workdir)));
	on_save_command = g_strdup(gtk_entry_get_text(GTK_ENTRY(project_data.on_save_command)));
	on_save_workdir = g_strdup(gtk_entry_get_text(GTK_ENTRY(project_data.on_save_workdir)));

	// write the configs to file.
	gchar *bs_cm_key, *bs_wd_key, *os_cm_key, *os_wd_key;
	bs_cm_key = g_strconcat(filetype_name, "_", "BS", "_", "00", "_", "CM", NULL);
	if (before_save_command && strlen(before_save_command) > 1) {
		// set it
		g_key_file_set_string(config, "autorun", bs_cm_key, before_save_command);
	} else {
		// delete it
		g_key_file_remove_key(config, "autorun", bs_cm_key, NULL);
	}
	bs_wd_key = g_strconcat(filetype_name, "_", "BS", "_", "00", "_", "WD", NULL);
	if (before_save_workdir && strlen(before_save_command) > 1) {
		// set it
		g_key_file_set_string(config, "autorun", bs_wd_key, before_save_workdir);
	} else {
		// delete it
		g_key_file_remove_key(config, "autorun", bs_wd_key, NULL);
	}
	os_cm_key = g_strconcat(filetype_name, "_", "OS", "_", "00", "_", "CM", NULL);
	if (on_save_command && strlen(before_save_command) > 1) {
		// set it
		g_key_file_set_string(config, "autorun", os_cm_key, on_save_command);
	} else {
		// delete it
		g_key_file_remove_key(config, "autorun", os_cm_key, NULL);
	}
	os_wd_key = g_strconcat(filetype_name, "_", "OS", "_", "00", "_", "WD", NULL);
	if (on_save_workdir && strlen(before_save_command) > 1) {
		// set it
		g_key_file_set_string(config, "autorun", os_wd_key, on_save_workdir);
	} else {
		// delete it
		g_key_file_remove_key(config, "autorun", os_wd_key, NULL);
	}

	g_free(before_save_command);
	g_free(before_save_workdir);
	g_free(on_save_command);
	g_free(on_save_workdir);
	g_free(bs_cm_key);
	g_free(bs_wd_key);
	g_free(os_cm_key);
	g_free(os_wd_key);
}

/* ensure that the Project>Auto-run is hidden when not in use
 * notebook a reference to Project Properties from Geany */
void project_hide_properties_tab(GtkWidget* notebook) {
	gtk_notebook_detach_tab(GTK_NOTEBOOK(notebook), project_data.tab);
	project_data.notebook = NULL;
}

/* ensure that the Project>Auto-run is shown when needed
 * notebook a reference to Project Properties from Geany */
void project_show_properties_tab(GtkWidget* notebook) {
	// grab the current filetype
	if (g_strcmp0("None", document_get_current()->file_type->name) == 0 || !autorun_globals->data->app->project) {
		// Auto-run only supports file specific handlers
		// hide the tab
		return;
	}

	// um, are we on this noteboot?
	if (!project_data.notebook) {
		GtkWidget* label = gtk_label_new("Auto-run");
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), GTK_WIDGET(g_object_ref(project_data.tab)), label);
		gtk_widget_show_all(notebook);
		project_data.notebook = notebook;
	}

	gchar* filetype_name = document_get_current()->file_type->name;

	// attach the configs to the project
	GKeyFile* config = g_key_file_new();
	gboolean ret = g_key_file_load_from_file(config, autorun_globals->data->app->project->file_name, G_KEY_FILE_NONE, NULL);
	if (ret) {
		gchar *before_save_command, *before_save_workdir, *on_save_command, *on_save_workdir;

		// read the current configs
		gchar* key_name;
		/* this looks wierd, but _ BS OS CM are all fixed strings and prevents
		 * static allocation of a fixed string only used here. */
		key_name = g_strconcat(filetype_name, "_", "BS", "_", "00", "_", "CM", NULL);
		before_save_command = utils_get_setting_string(config, "autorun", key_name, "");
		g_free(key_name);
		key_name = g_strconcat(filetype_name, "_", "BS", "_", "00", "_", "WD", NULL);
		before_save_workdir = utils_get_setting_string(config, "autorun", key_name, "");
		g_free(key_name);
		key_name = g_strconcat(filetype_name, "_", "OS", "_", "00", "_", "CM", NULL);
		on_save_command = utils_get_setting_string(config, "autorun", key_name, "");
		g_free(key_name);
		key_name = g_strconcat(filetype_name, "_", "OS", "_", "00", "_", "WD", NULL);
		on_save_workdir = utils_get_setting_string(config, "autorun", key_name, "");
		g_free(key_name);

		// update the notebook
		gtk_entry_set_text(GTK_ENTRY(project_data.before_save_command), before_save_command);
		gtk_entry_set_text(GTK_ENTRY(project_data.before_save_workdir), before_save_workdir);
		gtk_entry_set_text(GTK_ENTRY(project_data.on_save_command), on_save_command);
		gtk_entry_set_text(GTK_ENTRY(project_data.on_save_workdir), on_save_workdir);

		if (before_save_command) {
			g_free(before_save_command);
		}
		if (before_save_workdir) {
			g_free(before_save_workdir);
		}
		if (on_save_command) {
			g_free(on_save_command);
		}
		if (on_save_workdir) {
			g_free(on_save_workdir);
		}
	}
	g_key_file_free(config);
}

/* Gtk teardown for Auto-run tab */
void project_properties_tab_cleanup(void) {
	gtk_widget_destroy(GTK_WIDGET(project_data.before_save_command));
	project_data.before_save_command = NULL;
	gtk_widget_destroy(GTK_WIDGET(project_data.before_save_workdir));
	project_data.before_save_workdir = NULL;
	gtk_widget_destroy(GTK_WIDGET(project_data.on_save_command));
	project_data.on_save_command = NULL;
	gtk_widget_destroy(GTK_WIDGET(project_data.on_save_workdir));
	project_data.on_save_workdir = NULL;
	g_object_ref_sink(project_data.tab);
	gtk_widget_destroy(GTK_WIDGET(project_data.tab));
	project_data.tab = NULL;
	// noteboot isn't ours.
	project_data.notebook = NULL;
}

/* Gtk setup for Auto-run tab */
void project_properties_tab_init(void) {
	// do we need to build a tab?
	if (project_data.before_save_command) {
		// this smells familiar, lets not.
		return;
	}
	GtkWidget *vbox, *hbox, *ebox, *table_box;
	GtkWidget *label, *seperator;
	GtkSizeGroup* size_group;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	table_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_set_spacing(GTK_BOX(table_box), 6);

	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	seperator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(table_box), seperator, TRUE, FALSE, 0);

	label = gtk_label_new(_("Before Save Command"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	project_data.before_save_command = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(project_data.before_save_command));
	gtk_widget_set_tooltip_text(
		project_data.before_save_command,
		_("supports replacements: %f = filename, %a = filename with path, %d = path, %p = project directory")
	);

	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), project_data.before_save_command, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	label = gtk_label_new(_("Working Directory"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	project_data.before_save_workdir = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(project_data.before_save_workdir));
	gtk_widget_set_tooltip_text(
		project_data.before_save_workdir,
		_("supports replacements: %f = filename, %a = filename with path, %d = path, %p = project directory")
	);

	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), project_data.before_save_workdir, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	seperator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(table_box), seperator, TRUE, FALSE, 0);

	label = gtk_label_new(_("On Save Command"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	project_data.on_save_command = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(project_data.on_save_command));
	gtk_widget_set_tooltip_text(
		project_data.on_save_command,
		_("supports replacements: %f = filename, %a = filename with path, %d = path, %p = project directory")
	);
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), project_data.on_save_command, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);
	label = gtk_label_new(_("Working Directory"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	project_data.on_save_workdir = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(project_data.on_save_workdir));
	gtk_widget_set_tooltip_text(
		project_data.on_save_workdir,
		_("supports replacements: %f = filename, %a = filename with path, %d = path, %p = project directory")
	);
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), project_data.on_save_workdir, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	seperator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(table_box), seperator, TRUE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), table_box, FALSE, FALSE, 6);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 6);

	// grab a reference to the new tab, so we can detach/retach later.
	project_data.tab = GTK_WIDGET(g_object_ref(hbox));
}
