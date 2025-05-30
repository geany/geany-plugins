/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/time.h>
#include <string.h>

#include <geanyplugin.h>

#include "prjorg-project.h"
#include "prjorg-sidebar.h"
#include "prjorg-menu.h"
#include "prjorg-utils.h"


GeanyPlugin *geany_plugin;
GeanyData *geany_data;

PLUGIN_VERSION_CHECK(248)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Project Organizer"),
	_("Project file tree, project-wide indexing and search, extra navigation options (was GProject)"),
	VERSION,
	"Jiri Techet <techet@gmail.com>")

static GtkWidget *properties_tab = NULL;

static struct
{
	GtkWidget *open_entry;
	GtkWidget *term_entry;
} pref_widgets;


static void on_doc_open(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GeanyDocument * doc,
	    G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	if (prjorg_project_is_in_project(doc->file_name))
		prjorg_project_remove_single_tm_file(doc->file_name);

	prjorg_sidebar_update(FALSE);
	set_header_filetype(doc);
}


static void on_doc_activate(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GeanyDocument * doc,
		G_GNUC_UNUSED gpointer user_data)
{
	prjorg_sidebar_update(FALSE);
}


static void on_doc_close(G_GNUC_UNUSED GObject * obj, GeanyDocument * doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);

	if (doc->file_name == NULL)
		return;

	/* tags of open files managed by geany - when the file gets closed,
	 * we should take care of it */
	if (prjorg_project_is_in_project(doc->file_name))
		prjorg_project_add_single_tm_file(doc->file_name);

	prjorg_sidebar_update(FALSE);
}


static void on_build_start(GObject *obj, gpointer user_data)
{
	guint i = 0;

	foreach_document(i)
	{
		if (prjorg_project_is_in_project(documents[i]->file_name))
			document_save_file(documents[i], FALSE);
	}
}


static void on_project_dialog_open(G_GNUC_UNUSED GObject * obj, GtkWidget * notebook,
		G_GNUC_UNUSED gpointer user_data)
{
	if (prj_org && !properties_tab)
		properties_tab = prjorg_project_add_properties_tab(notebook);
}


static void on_project_dialog_confirmed(G_GNUC_UNUSED GObject * obj, GtkWidget * notebook,
		G_GNUC_UNUSED gpointer user_data)
{
	if (prj_org)
	{
		prjorg_project_read_properties_tab();
		prjorg_sidebar_update(TRUE);
	}
}


static void on_project_dialog_close(G_GNUC_UNUSED GObject * obj, GtkWidget * notebook,
		G_GNUC_UNUSED gpointer user_data)
{
	if (properties_tab)
	{
		gtk_widget_destroy(properties_tab);
		properties_tab = NULL;
	}
}


static void on_project_open(G_GNUC_UNUSED GObject * obj, GKeyFile * config,
		G_GNUC_UNUSED gpointer user_data)
{
	if (!prj_org)
	{
		gchar **arr = prjorg_project_load_expanded_paths(config);

		prjorg_project_open(config);
		prjorg_sidebar_update_full(TRUE, arr);
		prjorg_sidebar_activate(TRUE);
		prjorg_menu_activate_menu_items(TRUE);
	}
}



static void on_project_close(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED gpointer user_data)
{
	prjorg_project_close();
	prjorg_sidebar_update(TRUE);
	prjorg_sidebar_activate(FALSE);
	prjorg_menu_activate_menu_items(FALSE);
}


/* May be called also after plugin_init(), see plugin_init() for more info */
static void on_project_save(G_GNUC_UNUSED GObject * obj, GKeyFile * config,
		G_GNUC_UNUSED gpointer user_data)
{
	if (!prj_org)
	{
		/* happens when the project is created or the plugin is loaded */
		prjorg_project_open(config);
		prjorg_sidebar_update(TRUE);
		prjorg_sidebar_activate(TRUE);
		prjorg_menu_activate_menu_items(TRUE);
	}

	prjorg_project_save(config);
}


PluginCallback plugin_callbacks[] = {
	{"document-open", (GCallback) & on_doc_open, TRUE, NULL},
	{"document-activate", (GCallback) & on_doc_activate, TRUE, NULL},
	{"document-close", (GCallback) & on_doc_close, TRUE, NULL},
	{"build-start", (GCallback) & on_build_start, TRUE, NULL},
	{"project-dialog-open", (GCallback) & on_project_dialog_open, TRUE, NULL},
	{"project-dialog-confirmed", (GCallback) & on_project_dialog_confirmed, TRUE, NULL},
	{"project-dialog-close", (GCallback) & on_project_dialog_close, TRUE, NULL},
	{"project-open", (GCallback) & on_project_open, TRUE, NULL},
	{"project-close", (GCallback) & on_project_close, TRUE, NULL},
	{"project-save", (GCallback) & on_project_save, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


static void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		set_commands(gtk_entry_get_text(GTK_ENTRY(pref_widgets.open_entry)),
			gtk_entry_get_text(GTK_ENTRY(pref_widgets.term_entry)));
	}
}


GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox, *hbox, *ebox, *table_box;
	GtkWidget *label;
	GtkSizeGroup *size_group;
	gchar *cmd;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	table_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_set_spacing(GTK_BOX(table_box), 6);

	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	label = gtk_label_new(_("File manager command:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	pref_widgets.open_entry = gtk_entry_new();
	gtk_widget_set_size_request(pref_widgets.open_entry, 250, -1);
	ui_entry_add_clear_icon(GTK_ENTRY(pref_widgets.open_entry));
	gtk_widget_set_tooltip_text(pref_widgets.open_entry,
		_("Command used for 'Open File Manager' sidebar context menu item;\n"
		  "%d will be replaced with the full path of the selected file without the filename.\n"
		  "The command will be executed in the directory of the selected file."));
	cmd = get_open_cmd(FALSE, NULL);
	gtk_entry_set_text(GTK_ENTRY(pref_widgets.open_entry), cmd);
	g_free(cmd);
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), pref_widgets.open_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	label = gtk_label_new(_("Terminal command:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	pref_widgets.term_entry = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(pref_widgets.term_entry));
	gtk_widget_set_tooltip_text(pref_widgets.term_entry,
		_("Command used for 'Open Terminal' sidebar context menu item;\n"
		  "%d will be replaced with the full path of the selected file without the filename.\n"
		  "The command will be executed in the directory of the selected file."));
	cmd = get_terminal_cmd(FALSE, NULL);
	gtk_entry_set_text(GTK_ENTRY(pref_widgets.term_entry), cmd);
	g_free(cmd);
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), pref_widgets.term_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), table_box, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

	gtk_widget_show_all(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}


static gboolean write_config_cb(gpointer user_data)
{
	if (geany_data->app->project && !prj_org)
		project_write_config();
	return FALSE;
}


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	prjorg_menu_init();
	prjorg_sidebar_init();
	/* If the project is already open, we won't get the project-open signal
	 * when the plugin is enabled in the plugin manager so the plugin won't
	 * reload the project tree. However, we can force the call of the
	 * project-save signal emission whose handler does a similar thing like
	 * project-open so we get the same effect.
	 *
	 * For some reason this doesn't work when called directly from plugin_init()
	 * so perform on idle instead. Also use low priority so hopefully normal
	 * project opening happens before and we don't unnecessarily rewrite the
	 * project file. */
	g_idle_add_full(G_PRIORITY_LOW, write_config_cb, NULL, NULL);
}


void plugin_cleanup(void)
{
	if (geany_data->app->project)
	{
		prjorg_project_close();
		prjorg_sidebar_update(TRUE);
	}

	prjorg_menu_cleanup();
	prjorg_sidebar_cleanup();
}


void plugin_help (void)
{
	utils_open_browser("https://plugins.geany.org/projectorganizer.html");
}
