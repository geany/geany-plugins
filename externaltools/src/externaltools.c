/*
 *  externaltools.c
 *
 *  Copyright 2024 George Katevenis <george_kate@hotmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <geanyplugin.h>

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
	_("External Tools"),
	_("Quickly open external tools, like a terminal or a file manager."),
	"0.1", "George Katevenis <george_kate@hotmail.com>")

// ----------------------

static void on_open_terminal_activate(GtkMenuItem *menuitem, gpointer user_data);
static void on_open_directory_activate(GtkMenuItem *menuitem, gpointer user_data);

// ----------------------

enum
{
	TOOL_OPEN_TERMINAL = 0,
	TOOL_OPEN_DIRECTORY,
	TOOL_COUNT
};

struct tool_t
{
	GtkWidget *menu_item; // widget under Tools menu
	GtkWidget *config_entry; // GtkEntry for tool command in preferences

	const gchar *cmd; // command to run as taken from config_entry
	const gchar *default_cmd; // fallback command

	const gchar *name_id; // internal name, used e.g. in the config file

	const gchar *name; // name of tool (used in Keybindings, Preferences)
	const gchar *name_menu; // for use under Tools menu
	const gchar *icon; // name of icon for the Tools menu item
	const gchar *tooltip; // tooltip for tool in preferences

	void (*activate) (GtkMenuItem *, gpointer); // function to execute tool
};

#define TOOLTIP_REPLACEMENTS "%f and %d are substituted with the filename and the directory of the current document, respectively"

static struct tool_t tools[TOOL_COUNT] =
{
	[TOOL_OPEN_TERMINAL] =
	(struct tool_t) {
		.menu_item = NULL,
		.config_entry = NULL,

		.cmd = NULL,

		#ifdef G_OS_WIN32
			.default_cmd = "cmd",
		#elif defined(__APPLE__)
			.default_cmd = "open -a terminal",
		#else
			.default_cmd = "xterm",
		#endif

		.name_id = "open_terminal",

		.name = N_("Open Terminal"),
		.name_menu = N_("Open _Terminal"),
		.icon = "utilities-terminal-symbolic",
		.tooltip = N_("Command to open a virtual terminal emulator"
			" (" TOOLTIP_REPLACEMENTS ")"),

		.activate = on_open_terminal_activate
	},

	[TOOL_OPEN_DIRECTORY] =
	(struct tool_t) {
		.menu_item = NULL,
		.config_entry = NULL,

		.cmd = NULL,

		#ifdef G_OS_WIN32
			.default_cmd = "explorer %d",
		#elif defined(__APPLE__)
			.default_cmd = "open %d",
		#else
			.default_cmd = "xdg-open %d",
		#endif

		.name_id = "open_directory",

		.name = N_("Open Directory"),
		.name_menu = N_("Open _Directory"),
		.icon = "folder-symbolic",
		.tooltip = N_("Command to open your file manager of choice"
			" (" TOOLTIP_REPLACEMENTS ")"),

		.activate = on_open_directory_activate
	},
};

static gchar *config_file;

// -----------------------------------------------

/* Generic function to spawn a tool (e.g. terminal or file manager)
 * in the same directory/context as the current document. The function
 * calls `command`, in the same working dir as the current document
 * (or in the home dir, if the current document is unnamed), while
 * performing the following replacements:
 *
 *   %f: Replaced with the file name of the current document,
 *       or erased if the current document is unnamed.
 *
 *   %d: Replaced with the directory containing the document,
 *       or with the home directory if the document is unnamed.
 */
static void spawn_tool_generic(const gchar *command)
{
	GeanyDocument *doc;

	gchar *doc_name_utf8;
	gchar *doc_name_locale;

	gchar *dir_utf8;
	gchar *dir_locale;

	gchar *cmd_locale;

	GError *error = NULL;

	// --

	doc = document_get_current();

	if (doc && doc->file_name)
	{
		dir_utf8 = g_path_get_dirname(doc->file_name);
		dir_locale = utils_get_locale_from_utf8(dir_utf8);
	}
	else
	{
		dir_locale = g_strdup(g_get_home_dir());
		dir_utf8 = utils_get_utf8_from_locale(dir_locale);
	}

	doc_name_utf8 = (doc ? g_strdup(doc->file_name) : NULL);
	doc_name_locale = utils_get_locale_from_utf8(doc_name_utf8);

	// --

	cmd_locale = utils_get_locale_from_utf8(command);

	GString *cmd_str;

	cmd_str = g_string_new(cmd_locale);
	g_free(cmd_locale);

	if (!EMPTY(doc_name_locale))
	{
		utils_string_replace_all(cmd_str, "%f", "\"%f\"");
		utils_string_replace_all(cmd_str, "%f", doc_name_utf8);
	}
	else
	{
		utils_string_replace_all(cmd_str, "%f", "");
	}

	if (!EMPTY(dir_locale))
	{
		utils_string_replace_all(cmd_str, "%d", "\"%d\"");
		utils_string_replace_all(cmd_str, "%d", dir_locale);
	}
	else
	{
		utils_string_replace_all(cmd_str, "%d", "");
	}

	cmd_locale = g_string_free(cmd_str, FALSE);

	// --

	if (!spawn_async(dir_utf8, cmd_locale, NULL, NULL, NULL, &error))
	{
		gchar *cmd_utf8 = utils_get_utf8_from_locale(cmd_locale);

		ui_set_statusbar(TRUE, _("externaltools: Spawning '%s' failed "
			" (%s). Check the configured command in the plugin's "
			"preferences"), cmd_utf8, error->message);

		g_free(cmd_utf8);
		g_error_free(error);
	}

	g_free(doc_name_utf8);
	g_free(doc_name_locale);

	g_free(dir_utf8);
	g_free(dir_locale);

	g_free(cmd_locale);
}

// -----------------------------------------------

static void on_open_terminal_activate(GtkMenuItem *menuitem,
	gpointer user_data)
{
	spawn_tool_generic(tools[TOOL_OPEN_TERMINAL].cmd);
}

static void on_open_directory_activate(GtkMenuItem *menuitem,
	gpointer user_data)
{
	spawn_tool_generic(tools[TOOL_OPEN_DIRECTORY].cmd);
}

static void on_kb(guint tool_id)
{
	if (tool_id >= 0 && tool_id < TOOL_COUNT)
	{
		tools[tool_id].activate(NULL, NULL);
	}
}

// -----------------------------------------------

static void save_config(void)
{
	GKeyFile *config;
	gchar *config_dir;
	gchar *data;
	gint err;

	// --

	config_dir = g_path_get_dirname(config_file);

	if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)
			&& (err = utils_mkdir(config_dir, TRUE)) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Plugin configuration directory "
		 "(%s) could not be created. (%s)"), config_dir, g_strerror(err));

		g_free(config_dir);
		return;
	}

	g_free(config_dir);

	// --

	config = g_key_file_new();
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	for (guint tool_id = 0; tool_id < TOOL_COUNT; tool_id++)
	{
		struct tool_t *t = &tools[tool_id];

		g_key_file_set_string(config, "externaltools", t->name_id, t->cmd);
	}

	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(config_file, data);

	g_free(data);
	g_key_file_free(config);
}

static void load_config(void)
{
	GKeyFile *config = g_key_file_new();

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	for (guint tool_id = 0; tool_id < TOOL_COUNT; tool_id++)
	{
		struct tool_t *t = &tools[tool_id];

		t->cmd = utils_get_setting_string(config,
			"externaltools", t->name_id, t->default_cmd);
	}

	g_key_file_free(config);
}

static void on_configure_response(GtkDialog *dialog,
	gint response, gpointer user_data)
{
	if (!(response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY))
		return;

	for (guint tool_id = 0; tool_id < TOOL_COUNT; tool_id++)
	{
		struct tool_t *t = &tools[tool_id];

		t->cmd = gtk_editable_get_chars(GTK_EDITABLE(t->config_entry), 0, -1);
	}

	save_config();
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *grid;
	GtkWidget *label;
	GtkWidget *entry;

	grid = gtk_grid_new();

	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 6);

	for (guint tool_id = 0; tool_id < TOOL_COUNT; tool_id++)
	{
		struct tool_t *t = &tools[tool_id];

		label = gtk_label_new(t->name);
		entry = gtk_entry_new();

		gtk_entry_set_text(GTK_ENTRY(entry), t->cmd);
		gtk_widget_set_tooltip_text(entry, t->tooltip);

		gtk_widget_set_halign(label, GTK_ALIGN_START);

		gtk_grid_attach(GTK_GRID(grid), label, 0, tool_id, 1, 1);
		gtk_grid_attach(GTK_GRID(grid), entry, 1, tool_id, 1, 1);

		t->config_entry = entry;

	}

	gtk_widget_show_all(grid);

	g_signal_connect(dialog, "response",
		G_CALLBACK(on_configure_response), NULL);

	return grid;
}

// -----------------------------------------------

void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;

	config_file = g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir,
		"plugins", "externaltools", "externaltools.conf", NULL);

	load_config();

	key_group = plugin_set_key_group(geany_plugin,
		"externaltools", TOOL_COUNT, NULL);

	for (guint tool_id = 0; tool_id < TOOL_COUNT; tool_id++)
	{
		struct tool_t *t = &tools[tool_id];

		/* Translations. Here because tools[]
		 * needs to initialized statically. */
		t->name = _(t->name);
		t->name_menu = _(t->name_menu);
		t->tooltip = _(t->tooltip);

		/* Menu/Tools item */

		t->menu_item = gtk_image_menu_item_new_with_mnemonic(t->name_menu);

		if (t->icon)
		{
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(t->menu_item),
				gtk_image_new_from_icon_name(t->icon, GTK_ICON_SIZE_MENU));
		}

		gtk_widget_show(t->menu_item);

		gtk_container_add(GTK_CONTAINER(geany->
			main_widgets->tools_menu), t->menu_item);

		g_signal_connect(t->menu_item, "activate",
			G_CALLBACK(t->activate), NULL);

		keybindings_set_item(key_group, tool_id, on_kb,
			0, 0, t->name_id, t->name, t->menu_item);
	}
}

void plugin_cleanup(void)
{
	for (int tool_id = 0; tool_id < TOOL_COUNT; tool_id++)
	{
		struct tool_t *t = &tools[tool_id];

		gtk_widget_destroy(t->menu_item);
	}

	g_free(config_file);
}
