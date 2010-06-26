/*
 *  geanyprj - Alternative project support for geany light IDE.
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gstdio.h>

#include "geanyprj.h"

PluginFields *plugin_fields;
extern GeanyData *geany_data;

static struct
{
	GtkWidget *new_project;
	GtkWidget *delete_project;

	GtkWidget *add_file;

	GtkWidget *preferences;

	GtkWidget *find_in_files;
} menu_items;

// simple struct to keep references to the elements of the properties dialog
typedef struct _PropertyDialogElements
{
	GtkWidget *dialog;
	GtkWidget *name;
	GtkWidget *description;
	GtkWidget *file_name;
	GtkWidget *base_path;
	GtkWidget *make_in_base_path;
	GtkWidget *run_cmd;
	GtkWidget *regenerate;
	GtkWidget *type;
	GtkWidget *patterns;
} PropertyDialogElements;

static PropertyDialogElements *
build_properties_dialog(gboolean properties)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *image;
	GtkWidget *button;
	GtkWidget *bbox;
	GtkWidget *label;
	GtkTooltips *tooltips =
		GTK_TOOLTIPS(ui_lookup_widget(geany->main_widgets->window, "tooltips"));
	PropertyDialogElements *e;
	gchar *dir = NULL;
	gchar *basename = NULL;
	gint i;
	GeanyDocument *doc;

	doc = document_get_current();

	if (doc && doc->file_name != NULL && g_path_is_absolute(doc->file_name))
	{
		dir = g_path_get_dirname(doc->file_name);
	}
	else
	{
		dir = g_strdup("");
	}
	basename = g_path_get_basename(dir);

	e = g_new0(PropertyDialogElements, 1);

	if (properties)
	{
		e->dialog =
			gtk_dialog_new_with_buttons(_("Project Preferences"),
						    GTK_WINDOW(geany->main_widgets->window),
						    GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	}
	else
	{
		e->dialog =
			gtk_dialog_new_with_buttons(_("New Project"),
						    GTK_WINDOW(geany->main_widgets->window),
						    GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

		gtk_widget_set_name(e->dialog, "GeanyDialogProject");
		bbox = gtk_hbox_new(FALSE, 0);
		button = gtk_button_new();
		image = gtk_image_new_from_stock("gtk-new", GTK_ICON_SIZE_BUTTON);
		label = gtk_label_new_with_mnemonic(_("C_reate"));
		gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 3);
		gtk_container_add(GTK_CONTAINER(button), bbox);
		gtk_dialog_add_action_widget(GTK_DIALOG(e->dialog), button, GTK_RESPONSE_OK);
	}

	vbox = ui_dialog_vbox_new(GTK_DIALOG(e->dialog));

	table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("Name:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->name = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(e->name), MAX_NAME_LEN);
	gtk_entry_set_text(GTK_ENTRY(e->name), basename);

	ui_table_add_row(GTK_TABLE(table), 0, label, e->name, NULL);

	label = gtk_label_new(_("Location:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);
	e->file_name = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(e->file_name), 30);

	if (properties)
	{
		gtk_widget_set_sensitive(e->file_name, FALSE);
		ui_table_add_row(GTK_TABLE(table), 1, label, e->file_name, NULL);
	}
	else
	{
		button = gtk_button_new();
		image = gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_BUTTON);
		gtk_container_add(GTK_CONTAINER(button), image);
		bbox = ui_path_box_new(_("Choose Project Location"),
				       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				       GTK_ENTRY(e->file_name));
		gtk_entry_set_text(GTK_ENTRY(e->file_name), dir);
		ui_table_add_row(GTK_TABLE(table), 1, label, bbox, NULL);
	}


	label = gtk_label_new(_("Base path:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->base_path = gtk_entry_new();
	gtk_tooltips_set_tip(tooltips, e->base_path,
			     _("Base directory of all files that make up the project. "
			       "This can be a new path, or an existing directory tree. "
			       "You can use paths relative to the project filename."), NULL);
	bbox = ui_path_box_new(_("Choose Project Base Path"),
			       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, GTK_ENTRY(e->base_path));
	gtk_entry_set_text(GTK_ENTRY(e->base_path), dir);

	ui_table_add_row(GTK_TABLE(table), 2, label, bbox, NULL);

	label = gtk_label_new("");
	e->regenerate = gtk_check_button_new_with_label(_("Generate file list on load"));
	gtk_tooltips_set_tip(tooltips, e->regenerate,
			     _("Automatically add files that match project type on project load "
			       "automatically. You can't manually add/remove files if "
			       "you checked this option, since your modification will be lost on "
			       "on next project load"), NULL);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->regenerate), TRUE);
	ui_table_add_row(GTK_TABLE(table), 3, label, e->regenerate, NULL);


	label = gtk_label_new(_("Type:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->type = gtk_combo_box_new_text();
	for (i = 0; i < NEW_PROJECT_TYPE_SIZE; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(e->type), project_type_string[i]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(e->type), 0);

	ui_table_add_row(GTK_TABLE(table), 4, label, e->type, NULL);

	gtk_container_add(GTK_CONTAINER(vbox), table);
	g_free(dir);
	g_free(basename);

	return e;
}

void
on_new_project(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	PropertyDialogElements *e;
	gint response;

	e = build_properties_dialog(FALSE);
	gtk_widget_show_all(e->dialog);

      retry:
	response = gtk_dialog_run(GTK_DIALOG(e->dialog));
	if (response == GTK_RESPONSE_OK)
	{
		gchar *path;
		struct GeanyPrj *prj;

		path = g_build_filename(gtk_entry_get_text(GTK_ENTRY(e->file_name)), ".geanyprj",
					NULL);

		if (g_file_test(path, G_FILE_TEST_EXISTS))
		{
			ui_set_statusbar(TRUE, _("Project file \"%s\" already exists"), path);
			g_free(path);
			goto retry;
		}
		prj = geany_project_new();

		geany_project_set_path(prj, path);
		geany_project_set_base_path(prj, gtk_entry_get_text(GTK_ENTRY(e->base_path)));
		geany_project_set_name(prj, gtk_entry_get_text(GTK_ENTRY(e->name)));
		geany_project_set_description(prj, "");
		geany_project_set_run_cmd(prj, "");
		geany_project_set_type_int(prj, gtk_combo_box_get_active(GTK_COMBO_BOX(e->type)));
		geany_project_set_regenerate(prj,
					     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
									  (e->regenerate)));

		geany_project_regenerate_file_list(prj);

		geany_project_save(prj);
		geany_project_free(prj);
		document_open_file(path, FALSE, NULL, NULL);
	}

	gtk_widget_destroy(e->dialog);
	g_free(e);
}

void
on_preferences(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	PropertyDialogElements *e;
	gint response;
	gchar *project_dir;

	e = build_properties_dialog(TRUE);

	project_dir = g_path_get_dirname(g_current_project->path);
	gtk_entry_set_text(GTK_ENTRY(e->file_name), project_dir);
	gtk_entry_set_text(GTK_ENTRY(e->name), g_current_project->name);
	gtk_entry_set_text(GTK_ENTRY(e->base_path), g_current_project->base_path);
	gtk_combo_box_set_active(GTK_COMBO_BOX(e->type), g_current_project->type);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->regenerate),
				     g_current_project->regenerate);

	gtk_widget_show_all(e->dialog);

	response = gtk_dialog_run(GTK_DIALOG(e->dialog));
	if (response == GTK_RESPONSE_OK)
	{
		geany_project_set_base_path(g_current_project,
					    gtk_entry_get_text(GTK_ENTRY(e->base_path)));
		geany_project_set_name(g_current_project, gtk_entry_get_text(GTK_ENTRY(e->name)));
		geany_project_set_description(g_current_project, "");
		geany_project_set_run_cmd(g_current_project, "");
		geany_project_set_type_int(g_current_project,
					   gtk_combo_box_get_active(GTK_COMBO_BOX(e->type)));
		geany_project_set_regenerate(g_current_project,
					     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
									  (e->regenerate)));
		geany_project_save(g_current_project);

		if (g_current_project->regenerate)
		{
			geany_project_regenerate_file_list(g_current_project);
		}
		sidebar_refresh();
	}

	gtk_widget_destroy(e->dialog);
	g_free(e);
	g_free(project_dir);
}

void
on_delete_project(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *path;
	if (!g_current_project)
		return;

	if (dialogs_show_question("Do you really wish to delete current project:\n%s?",
				  g_current_project->name))
	{
		path = utils_get_locale_from_utf8(g_current_project->path);
		xproject_close(FALSE);
		g_unlink(path);
		g_free(path);
	}
}

void
on_add_file(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc && doc->file_name != NULL && g_path_is_absolute(doc->file_name));

	if (!g_current_project)
		return;

	xproject_add_file(doc->file_name);
}

void
on_find_in_project(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *dir;
	if (!g_current_project)
		return;

	dir = g_path_get_dirname(g_current_project->path);
	search_show_find_in_files_dialog(dir);
	g_free(dir);
}

static void
update_menu_items()
{
	gboolean cur_file_exists;
	gboolean badd_file;
	GeanyDocument *doc;

	doc = document_get_current();
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	cur_file_exists = doc && doc->file_name != NULL && g_path_is_absolute(doc->file_name);

	badd_file = (g_current_project ? TRUE : FALSE) &&
		!g_current_project->regenerate &&
		cur_file_exists && !g_hash_table_lookup(g_current_project->tags, doc->file_name);

	gtk_widget_set_sensitive(menu_items.new_project, TRUE);
	gtk_widget_set_sensitive(menu_items.delete_project, g_current_project ? TRUE : FALSE);

	gtk_widget_set_sensitive(menu_items.add_file, badd_file);

	gtk_widget_set_sensitive(menu_items.preferences, g_current_project ? TRUE : FALSE);

	gtk_widget_set_sensitive(menu_items.find_in_files, g_current_project ? TRUE : FALSE);
}

void
tools_menu_init()
{
	GtkWidget *item, *image;

	GtkWidget *menu_prj = NULL;
	GtkWidget *menu_prj_menu = NULL;
	GtkTooltips *tooltips = NULL;

	tooltips = gtk_tooltips_new();

	menu_prj = gtk_image_menu_item_new_with_mnemonic(_("_Project"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_prj);

	g_signal_connect((gpointer) menu_prj, "activate", G_CALLBACK(update_menu_items), NULL);

	menu_prj_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_prj), menu_prj_menu);

	//


	image = gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("New Project"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu_prj_menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_new_project), NULL);
	menu_items.new_project = item;

	image = gtk_image_new_from_stock(GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Delete Project"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu_prj_menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_delete_project), NULL);
	menu_items.delete_project = item;

	gtk_container_add(GTK_CONTAINER(menu_prj_menu), gtk_separator_menu_item_new());

	image = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Add File"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu_prj_menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_add_file), NULL);
	menu_items.add_file = item;

	gtk_container_add(GTK_CONTAINER(menu_prj_menu), gtk_separator_menu_item_new());

	image = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Preferences"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu_prj_menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_preferences), NULL);
	menu_items.preferences = item;

	gtk_container_add(GTK_CONTAINER(menu_prj_menu), gtk_separator_menu_item_new());

	image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Find in Project"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu_prj_menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_find_in_project), NULL);
	menu_items.find_in_files = item;

	gtk_widget_show_all(menu_prj);

	plugin_fields->menu_item = menu_prj;
	plugin_fields->flags = PLUGIN_IS_DOCUMENT_SENSITIVE;
}

void
tools_menu_uninit()
{
	gtk_widget_destroy(plugin_fields->menu_item);
}
