/*
 *  geanyprj - Alternative project support for geany light IDE.
 *
 *  Copyright 2007 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *  Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *  Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
 *  Copyright 2007,2008 Yura Siamashka <yurand2@gmail.com>
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

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif
#include <geanyplugin.h>

#include <sys/time.h>
#include <string.h>

#include "geanyprj.h"

PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
			     "GeanyPrj", _("Alternative project support."), VERSION,
			     "Yura Siamashka <yurand2@gmail.com>")

GeanyData      *geany_data;


static gchar    *config_file;
static gboolean  display_sidebar = TRUE;


/* Plugin config keys */
#define GEANYPRJ_SETTING_SECTION               "geanyprj"
#define GEANYPRJ_SETTING_KEY_SHOW_SIDEBAR      "display_sidebar"
#define GEANYPRJ_SETTING_KEY_SHOW_FILTER_ENTRY "display_sidebar_filter"
#define GEANYPRJ_SETTING_KEY_FILTER_POLICY     "sidebar_filter_policy"
#define GEANYPRJ_SETTING_KEY_SEARCH_POLICY     "sidebar_search_policy"

/* Keybinding(s) */
enum
{
	KB_FIND_IN_PROJECT,
	KB_COUNT
};

PLUGIN_KEY_GROUP(geanyprj, KB_COUNT)

/* Plugin configure dialog main items */
static struct
{
	GtkWidget *sideBarEnabledBtn;
	GtkWidget *sideBarFilterEnabledBtn;
	GtkWidget *sidebarFilterPolicyCombo;
	GtkWidget *sidebarSearchPolicyCombo;
} pluginConfigureWidgets;

static void reload_project(void)
{
	gchar *dir;
	gchar *proj;
	GeanyDocument *doc;

	debug("%s\n", __FUNCTION__);

	doc = document_get_current();
	if (doc == NULL || doc->file_name == NULL)
		return;

	dir = g_path_get_dirname(doc->file_name);
	proj = find_file_path(dir, ".geanyprj");

	if (!proj)
	{
		if (g_current_project)
			xproject_close(TRUE);
		return;
	}

	if (!g_current_project)
	{
		xproject_open(proj);
	}
	else if (strcmp(proj, g_current_project->path) != 0)
	{
		xproject_close(TRUE);
		xproject_open(proj);
	}
	if (proj)
		g_free(proj);
}


static void on_doc_save(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc, G_GNUC_UNUSED gpointer user_data)
{
	gchar *name;

	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	name = g_path_get_basename(doc->file_name);
	if (g_current_project && strcmp(name, ".geanyprj") == 0)
	{
		xproject_close(FALSE);
	}
	reload_project();
	xproject_update_tag(doc->file_name);
}


static void on_doc_open(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GeanyDocument *doc,
						G_GNUC_UNUSED gpointer user_data)
{
	reload_project();
}


static void on_doc_activate(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GeanyDocument *doc,
							G_GNUC_UNUSED gpointer user_data)
{
	reload_project();
}


PluginCallback plugin_callbacks[] = {
	{"document-open", (GCallback) & on_doc_open, TRUE, NULL},
	{"document-save", (GCallback) & on_doc_save, TRUE, NULL},
	{"document-activate", (GCallback) & on_doc_activate, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


/* Keybinding callback */
static void kb_find_in_project(guint key_id)
{
	on_find_in_project(NULL, NULL);
}


static void load_settings(void)
{
	GKeyFile *config = g_key_file_new();
	gboolean  display_sidebar_tmp;
	GError   *display_sidebar_err      = NULL;
	gboolean  display_sidebar_filter_tmp;
	GError   *display_sidebar_filter_err = NULL;
	gint      sidebar_filter_policy_tmp;
	GError   *sidebar_filter_policy_err = NULL;
	gint      sidebar_search_policy_tmp;
	GError   *sidebar_search_policy_err = NULL;

	config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"geanyprj", G_DIR_SEPARATOR_S, "geanyprj.conf", NULL);
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	/* sidebar visibility */
	display_sidebar_tmp = g_key_file_get_boolean(config, GEANYPRJ_SETTING_SECTION, GEANYPRJ_SETTING_KEY_SHOW_SIDEBAR, &display_sidebar_err);
	if (display_sidebar_err)
		g_error_free(display_sidebar_err);
	else
		display_sidebar = display_sidebar_tmp;

	/* sidebar filter visibility */
	display_sidebar_filter_tmp = g_key_file_get_boolean(config, GEANYPRJ_SETTING_SECTION, GEANYPRJ_SETTING_KEY_SHOW_FILTER_ENTRY, &display_sidebar_filter_err);
	if (display_sidebar_filter_err)
		g_error_free(display_sidebar_filter_err);
	else
		sidebar_set_kbdfilter_enabled(display_sidebar_filter_tmp);

	/* sidebar filter policy */
	sidebar_filter_policy_tmp = g_key_file_get_integer(config, GEANYPRJ_SETTING_SECTION, GEANYPRJ_SETTING_KEY_FILTER_POLICY, &sidebar_filter_policy_err);
	if (sidebar_filter_policy_err)
		g_error_free(sidebar_filter_policy_err);
	else if (sidebar_filter_policy_tmp < 0)
		debug("%s filter policy is expected to be positive", __FUNCTION__);
	else if (sidebar_filter_policy_tmp >= KBDSEARCH_POLICY_ENUM_SIZE)
		debug("%s filter policy is out of bounds", __FUNCTION__);
	else
		sidebar_set_kbdfilter_policy((kbdsearch_policy)sidebar_filter_policy_tmp);

	/* sidebar search policy */
	sidebar_search_policy_tmp = g_key_file_get_integer(config, GEANYPRJ_SETTING_SECTION, GEANYPRJ_SETTING_KEY_SEARCH_POLICY, &sidebar_search_policy_err);
	if (sidebar_search_policy_err)
		g_error_free(sidebar_search_policy_err);
	else if (sidebar_search_policy_tmp < 0)
		debug("%s search policy is expected to be positive", __FUNCTION__);
	else if (sidebar_search_policy_tmp >= KBDSEARCH_POLICY_ENUM_SIZE)
		debug("%s search policy is out of bounds", __FUNCTION__);
	else
		sidebar_set_kbdsearch_policy((kbdsearch_policy)sidebar_search_policy_tmp);


	g_key_file_free(config);
}


static void save_settings(void)
{
	GKeyFile *config = g_key_file_new();
	gchar    *data;
	gchar    *config_dir = g_path_get_dirname(config_file);

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	g_key_file_set_boolean(config, GEANYPRJ_SETTING_SECTION, GEANYPRJ_SETTING_KEY_SHOW_SIDEBAR, display_sidebar);
	g_key_file_set_boolean(config, GEANYPRJ_SETTING_SECTION, GEANYPRJ_SETTING_KEY_SHOW_FILTER_ENTRY, sidebar_get_kbdfilter_enabled());
	g_key_file_set_integer(config, GEANYPRJ_SETTING_SECTION, GEANYPRJ_SETTING_KEY_FILTER_POLICY, (gint)sidebar_get_kbdfilter_policy());
	g_key_file_set_integer(config, GEANYPRJ_SETTING_SECTION, GEANYPRJ_SETTING_KEY_SEARCH_POLICY, (gint)sidebar_get_kbdsearch_policy());

	if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
	}
	else
	{
		/* write config to file */
		data = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(config_file, data);
		g_free(data);
	}
	g_free(config_dir);
	g_key_file_free(config);
}

static void on_configure_sidebar_filter_toggle(GtkToggleButton *togglebutton, GtkWidget *sidebarFilterCombo)
{
	gtk_widget_set_sensitive( sidebarFilterCombo, gtk_toggle_button_get_active(togglebutton) );
}

static void on_configure_sidebar_toggle(GtkToggleButton *togglebutton, GtkWidget *sidebarOptsWidget)
{
	gtk_widget_set_sensitive( sidebarOptsWidget, gtk_toggle_button_get_active(togglebutton) );
}

static void on_configure_response(G_GNUC_UNUSED GtkDialog *dialog, gint response, G_GNUC_UNUSED gpointer userdata)
{
	gboolean save_settings_required = FALSE;
	gboolean old_display_sidebar = display_sidebar;
	gboolean new_display_sidebar;
	gboolean old_display_sidebar_filter = sidebar_get_kbdfilter_enabled();
	gboolean new_display_sidebar_filter;
	kbdsearch_policy old_sidebar_filter_policy = sidebar_get_kbdfilter_policy();
	kbdsearch_policy new_sidebar_filter_policy;
	kbdsearch_policy old_sidebar_search_policy = sidebar_get_kbdsearch_policy();
	kbdsearch_policy new_sidebar_search_policy;

	/* Compute settings user wants to reach */
	switch(response)
	{
	case GTK_RESPONSE_CANCEL:
		load_settings();
		new_display_sidebar = display_sidebar;
		new_display_sidebar_filter = sidebar_get_kbdfilter_enabled();
		new_sidebar_filter_policy = sidebar_get_kbdfilter_policy();
		new_sidebar_search_policy = sidebar_get_kbdsearch_policy();
		break;
		
	default:
		new_display_sidebar = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pluginConfigureWidgets.sideBarEnabledBtn));
		new_display_sidebar_filter = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pluginConfigureWidgets.sideBarFilterEnabledBtn));
		new_sidebar_filter_policy = gtk_combo_box_get_active(GTK_COMBO_BOX(pluginConfigureWidgets.sidebarFilterPolicyCombo));
		new_sidebar_search_policy = gtk_combo_box_get_active(GTK_COMBO_BOX(pluginConfigureWidgets.sidebarSearchPolicyCombo));
		break;
	}

	/* Conditionally apply settings */
	switch(response)
	{
	case GTK_RESPONSE_ACCEPT:
	case GTK_RESPONSE_OK:
	case GTK_RESPONSE_YES:
	case GTK_RESPONSE_APPLY:
	case GTK_RESPONSE_CANCEL: /* Targetted settings has previously been faked in this cancel case */
		display_sidebar = new_display_sidebar;
		sidebar_set_kbdfilter_enabled(new_display_sidebar_filter);
		sidebar_set_kbdfilter_policy(new_sidebar_filter_policy);
		sidebar_set_kbdsearch_policy(new_sidebar_search_policy);

		if (display_sidebar ^ old_display_sidebar)
		{
			if (display_sidebar)
			{
				create_sidebar();
				sidebar_refresh();
			}
			else
			{
				destroy_sidebar();
			}
			save_settings_required = TRUE;
		}
		
		if( new_display_sidebar_filter != old_display_sidebar_filter )
		{
			save_settings_required = TRUE;
		}
		
		if( new_sidebar_filter_policy != old_sidebar_filter_policy )
		{
			save_settings_required = TRUE;
		}
		
		if( new_sidebar_search_policy != old_sidebar_search_policy )
		{
			save_settings_required = TRUE;
		}
		break;
		
	default:
		/* Do not apply */
		break;
	}
	
	/* Conditionally save settings */
	switch(response)
	{
	case GTK_RESPONSE_ACCEPT:
	case GTK_RESPONSE_OK:
	case GTK_RESPONSE_YES:
		if (save_settings_required == TRUE)
		{
			save_settings();
		}
		break;
	
	case GTK_RESPONSE_APPLY:
	default:
		/* Do not save */
		break;
	}
}


/* Called by Geany to initialize the plugin */
void plugin_init(G_GNUC_UNUSED GeanyData *data)
{
	load_settings();
	tools_menu_init();

	xproject_init();
	if (display_sidebar)
		create_sidebar();
	reload_project();

	keybindings_set_item(plugin_key_group, KB_FIND_IN_PROJECT,
		kb_find_in_project, 0, 0, "find_in_project",
			_("Find a text in geanyprj's project"), NULL);
}


/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * plugin_init() was called.
 */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox;
	GtkWidget *sidebarOpts;
	GtkWidget *table;
	gint       tableRowCount;
	GtkWidget *label;
	GtkWidget *checkboxDisplaySidebar;
	GtkWidget *checkboxDisplaySidebarFilter;
	GtkWidget *comboboxFilter;
	GtkWidget *comboboxSearch;
	gint i;

	/* Global VBox */
	vbox = gtk_vbox_new(FALSE, 6);

	/* Enable/Disable sidebar */
	checkboxDisplaySidebar = gtk_check_button_new_with_label(_("Display sidebar"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkboxDisplaySidebar), display_sidebar);
	gtk_box_pack_start(GTK_BOX(vbox), checkboxDisplaySidebar, FALSE, FALSE, 0);

	/* Sidebar options */
	sidebarOpts = gtk_frame_new(_("Sidebar options"));
	table = gtk_table_new(3, 2, FALSE);
	tableRowCount = 0;
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	
	/* - Filter visibility */
	label = gtk_label_new(_("Enable filtering file listing:"));
	gtk_widget_set_tooltip_markup(label,
	                      _("Allow you to filter visible files in the listing\n"
	                        "by typing some words in the filter input box\n"
	                        "above the file listing."));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	checkboxDisplaySidebarFilter = gtk_check_button_new_with_label(_("Yes"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkboxDisplaySidebarFilter), sidebar_get_kbdfilter_enabled());

	ui_table_add_row(GTK_TABLE(table), tableRowCount++, label, checkboxDisplaySidebarFilter, NULL);
	
	/* - Filter policy */
	label = gtk_label_new(_("Filter policy:"));
	gtk_widget_set_tooltip_markup(label,
	                      _("You can filter visible files in the listing\n"
	                        "by typing some words in the filter input box\n"
	                        "above the file listing."));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	comboboxFilter = gtk_combo_box_new_text();
	for (i = 0; i < KBDSEARCH_POLICY_ENUM_SIZE; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(comboboxFilter), sidebar_get_kdbsearch_name(i) );
	gtk_combo_box_set_active(GTK_COMBO_BOX(comboboxFilter), (gint) sidebar_get_kbdfilter_policy() );

	gtk_widget_set_sensitive( comboboxFilter, sidebar_get_kbdfilter_enabled() );
	ui_table_add_row(GTK_TABLE(table), tableRowCount++, label, comboboxFilter, NULL);
	
	/* - Search policy */
	label = gtk_label_new(_("Keyboard fast-search policy:"));
	gtk_widget_set_tooltip_markup(label,
	                      _("Select items using keyboard when sidebar is focused.\n"
	                        "Use arrows to select sibling matches.\n"
	                        "Hit Enter to open the selected file."));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	comboboxSearch = gtk_combo_box_new_text();
	for (i = 0; i < KBDSEARCH_POLICY_ENUM_SIZE; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(comboboxSearch), sidebar_get_kdbsearch_name(i) );
	gtk_combo_box_set_active(GTK_COMBO_BOX(comboboxSearch), (gint) sidebar_get_kbdsearch_policy() );

	ui_table_add_row(GTK_TABLE(table), tableRowCount++, label, comboboxSearch, NULL);
	
	/* End of sidebar options */
	gtk_container_add(GTK_CONTAINER(sidebarOpts), table);
	gtk_box_pack_start(GTK_BOX(vbox), sidebarOpts, FALSE, FALSE, 0);
	gtk_widget_set_sensitive( sidebarOpts, display_sidebar );
	g_signal_connect(checkboxDisplaySidebar, "toggled", G_CALLBACK(on_configure_sidebar_toggle), sidebarOpts);
	g_signal_connect(checkboxDisplaySidebarFilter, "toggled", G_CALLBACK(on_configure_sidebar_filter_toggle), comboboxFilter);
	

	/* Dialog is almost ready */
	gtk_widget_show_all(vbox);

	pluginConfigureWidgets.sideBarEnabledBtn = checkboxDisplaySidebar;
	pluginConfigureWidgets.sideBarFilterEnabledBtn = checkboxDisplaySidebarFilter;
	pluginConfigureWidgets.sidebarFilterPolicyCombo = comboboxFilter;
	pluginConfigureWidgets.sidebarSearchPolicyCombo = comboboxSearch;

	/* Connect a callback for when the user clicks a dialog button */
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}


/* Called by Geany before unloading the plugin. */
void plugin_cleanup(void)
{
	tools_menu_uninit();

	if (g_current_project)
		geany_project_free(g_current_project);
	g_current_project = NULL;

	g_free(config_file);

	xproject_cleanup();
	destroy_sidebar();
}
