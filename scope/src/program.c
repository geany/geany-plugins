/*
 *  program.c
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static StashGroup *program_group;
static StashGroup *options_group;
static StashGroup *thread_group;
static StashGroup *terminal_group;

static void stash_foreach(GFunc func, gpointer gdata)
{
	func(program_group, gdata);
	func(options_group, gdata);
	func(thread_group, gdata);
	func(terminal_group, gdata);
}

static void stash_group_destroy(StashGroup *group, G_GNUC_UNUSED gpointer gdata)
{
	utils_stash_group_free(group);
}

enum
{
	PROGRAM_NAME,
	PROGRAM_ID    /* 1-based */
};

#define RECENT_COUNT 28

static ScpTreeStore *recent_programs;
static guint recent_bitmap;
static GtkWidget *recent_menu;

static gboolean recent_program_load(GKeyFile *config, const char *section)
{
	gchar *name = utils_get_setting_string(config, section, "name", NULL);
	gint id = utils_get_setting_integer(config, section, "id", 0);
	gboolean valid = FALSE;

	if (name && id > 0 && id <= RECENT_COUNT && (recent_bitmap & (1 << id)) == 0)
	{
		scp_tree_store_append_with_values(recent_programs, NULL, NULL, PROGRAM_NAME, name,
			PROGRAM_ID, id, -1);
		recent_bitmap |= 1 << id;
		valid = TRUE;
	}

	g_free(name);
	return valid;
}

static gboolean recent_program_save(GKeyFile *config, const char *section, GtkTreeIter *iter)
{
	const gchar *name;
	gint id;

	scp_tree_store_get(recent_programs, iter, PROGRAM_NAME, &name, PROGRAM_ID, &id, -1);
	g_key_file_set_string(config, section, "name", name);
	g_key_file_set_integer(config, section, "id", id);
	return TRUE;
}

static void recent_menu_item_destroy(GtkWidget *widget, G_GNUC_UNUSED gpointer gdata)
{
	gtk_widget_destroy(widget);
}

static void on_recent_menu_item_activate(GtkMenuItem *menuitem, const gchar *name);

static gint recent_menu_count;

static void recent_menu_item_create(GtkTreeIter *iter, G_GNUC_UNUSED gpointer gdata)
{
	if (recent_menu_count < pref_show_recent_items)
	{
		const gchar *name;
		GtkWidget *item;

		scp_tree_store_get(recent_programs, iter, PROGRAM_NAME, &name, -1);
		item = gtk_menu_item_new_with_label(name);
		gtk_menu_shell_append(GTK_MENU_SHELL(recent_menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_recent_menu_item_activate),
			(gpointer) name);
		recent_menu_count++;
	}
}

static void recent_menu_create(void)
{
	gtk_container_foreach(GTK_CONTAINER(recent_menu), recent_menu_item_destroy, NULL);
	recent_menu_count = 0;
	store_foreach(recent_programs, (GFunc) recent_menu_item_create, NULL);
	gtk_widget_show_all(GTK_WIDGET(recent_menu));
}

static char *recent_file_name(gint id)
{
	char *programfile = g_strdup_printf("program_%d.conf", id);
	char *configfile = g_build_filename(geany->app->configdir, "plugins", "scope",
		programfile, NULL);

	g_free(programfile);
	return configfile;
}

gchar *program_executable;
gchar *program_arguments;
gchar *program_environment;
gchar *program_working_dir;
gchar *program_load_script;
gboolean program_auto_run_exit;
gboolean program_non_stop_mode;
gboolean program_temp_breakpoint;
gchar *program_temp_break_location;

static gint program_compare(ScpTreeStore *store, GtkTreeIter *iter, const char *name)
{
	const char *name1;

	scp_tree_store_get(store, iter, PROGRAM_NAME, &name1, -1);
	return !utils_filenamecmp(name1, name);
}

#define program_find(iter, name) scp_tree_store_traverse(recent_programs, FALSE, (iter), \
	NULL, (ScpTreeStoreTraverseFunc) program_compare, (gpointer) (name))

static void save_program_settings(void)
{
	const gchar *program_name = *program_executable ? program_executable :
		program_load_script;

	if (*program_name)
	{
		GtkTreeIter iter;
		gint id;
		GKeyFile *config = g_key_file_new();
		char *configfile;

		if (program_find(&iter, program_name))
		{
			scp_tree_store_get(recent_programs, &iter, PROGRAM_ID, &id, -1);
			scp_tree_store_move(recent_programs, &iter, 0);
		}
		else
		{
			if (scp_tree_store_iter_nth_child(recent_programs, &iter, NULL,
				RECENT_COUNT - 1))
			{
				scp_tree_store_get(recent_programs, &iter, PROGRAM_ID, &id, -1);
				scp_tree_store_remove(recent_programs, &iter);
			}
			else
			{
				for (id = 1; id < RECENT_COUNT; id++)
					if ((recent_bitmap & (1 << id)) == 0)
						break;

				recent_bitmap |= 1 << id;
			}

			scp_tree_store_prepend_with_values(recent_programs, &iter, NULL,
				PROGRAM_NAME, program_name, PROGRAM_ID, id, -1);
		}

		configfile = recent_file_name(id);
		stash_foreach((GFunc) stash_group_save_to_key_file, config);
		breaks_save(config);
		watches_save(config);
		inspects_save(config);
		registers_save(config);
		parse_save(config);
		utils_key_file_write_to_file(config, configfile);
		g_free(configfile);
		g_key_file_free(config);
	}
}

gboolean option_open_panel_on_load;
gboolean option_open_panel_on_start;
gboolean option_update_all_views;
gint option_high_bit_mode;
gboolean option_member_names;
gboolean option_argument_names;
gboolean option_long_mr_format;
gboolean option_inspect_expand;
gint option_inspect_count;
gboolean option_library_messages;
gboolean option_editor_tooltips;

extern gboolean thread_show_group;
extern gboolean thread_show_core;
extern gboolean stack_show_address;

static void program_configure(void)
{
	view_column_set_visible("thread_group_id_column", thread_show_group);
	view_column_set_visible("thread_core_column", thread_show_core);
	view_column_set_visible("stack_addr_column", stack_show_address);
}

static void on_recent_menu_item_activate(G_GNUC_UNUSED GtkMenuItem *menuitem, const gchar *name)
{
	GtkTreeIter iter;

	if (utils_filenamecmp(name, *program_executable ? program_executable :
		program_load_script) && program_find(&iter, name))
	{
		gint id;
		char *configfile;
		GKeyFile *config = g_key_file_new();
		GError *gerror = NULL;
		gchar *message;

		scp_tree_store_get(recent_programs, &iter, PROGRAM_ID, &id, -1);
		configfile = recent_file_name(id);

		if (g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, &gerror))
		{
			scp_tree_store_move(recent_programs, &iter, 0);
			save_program_settings();
			stash_foreach((GFunc) stash_group_load_from_key_file, config);
			if ((unsigned) option_inspect_expand > EXPAND_MAX)
				option_inspect_expand = 100;
			breaks_load(config);
			watches_load(config);
			inspects_load(config);
			registers_load(config);
			parse_load(config);
			message = g_strdup_printf(_("Loaded debug settings for %s."), name);
			program_find(&iter, name);
			scp_tree_store_move(recent_programs, &iter, 0);
			recent_menu_create();
			program_configure();
		}
		else
		{
			message = g_strdup_printf(_("Could not load debug settings file %s: %s."),
				configfile, gerror->message);
			g_error_free(gerror);
		}

		if (menuitem)
			ui_set_statusbar(TRUE, "%s", message);
		else
			msgwin_status_add("%s", message);

		g_free(message);
		g_key_file_free(config);
		g_free(configfile);
	}
}

static const gchar *build_get_execute(GeanyBuildCmdEntries field)
{
	return build_get_group_count(GEANY_GBG_EXEC) > 1 ?
		build_get_current_menu_item(GEANY_GBG_EXEC, 1, field) : NULL;
}

gboolean recent_menu_items(void)
{
	return recent_menu_count != 0;
}

void program_context_changed(void)
{
	const gchar *name = build_get_execute(GEANY_BC_COMMAND);

	if (name && debug_state() == DS_INACTIVE)
		on_recent_menu_item_activate(NULL, name);
}

static GtkWidget *program_dialog;
static GtkWidget *program_page_vbox;
static GtkEntry *program_exec_entry;
static GtkEntry *working_dir_entry;
static GtkEntry *load_script_entry;
static GtkTextBuffer *environment;
static GtkButton *long_mr_format;
static GtkWidget *auto_run_exit;
static GtkWidget *temp_breakpoint;
static GtkToggleButton *delete_all_items;
static GtkWidget *import_button;
static gboolean dialog_long_mr_format;
static const gchar *LONG_MR_FORMAT[2];

#define build_check_execute() (build_get_execute(GEANY_BC_COMMAND) || \
	build_get_execute(GEANY_BC_WORKING_DIR))
static gboolean last_state_inactive = TRUE;

void program_update_state(DebugState state)
{
	gboolean inactive = state == DS_INACTIVE;

	if (inactive != last_state_inactive)
	{
		gtk_widget_set_sensitive(program_page_vbox, inactive);
		gtk_widget_set_sensitive(import_button, inactive && build_check_execute());
		last_state_inactive = inactive;
	}
}

static void on_program_name_entry_changed(G_GNUC_UNUSED GtkEditable *editable,
	G_GNUC_UNUSED gpointer gdata)
{
	gboolean sensitive = *gtk_entry_get_text(program_exec_entry) ||
		*gtk_entry_get_text(load_script_entry);

	gtk_widget_set_sensitive(auto_run_exit, sensitive);
	gtk_widget_set_sensitive(temp_breakpoint, sensitive);
	g_signal_emit_by_name(temp_breakpoint, "toggled");
}

void on_program_setup(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gtk_text_buffer_set_text(environment, program_environment, -1);
	stash_foreach((GFunc) stash_group_display, NULL);
	gtk_button_set_label(long_mr_format, LONG_MR_FORMAT[option_long_mr_format]);
	dialog_long_mr_format = option_long_mr_format;
	gtk_widget_set_sensitive(import_button, last_state_inactive && build_check_execute());
	on_program_name_entry_changed(NULL, NULL);
	gtk_toggle_button_set_active(delete_all_items, FALSE);
	if (debug_state() == DS_INACTIVE)
		gtk_widget_grab_focus(GTK_WIDGET(program_exec_entry));
	gtk_dialog_run(GTK_DIALOG(program_dialog));
}

static void on_long_mr_format_clicked(GtkButton *button, G_GNUC_UNUSED gpointer gdata)
{
	dialog_long_mr_format ^= TRUE;
	gtk_button_set_label(button, LONG_MR_FORMAT[dialog_long_mr_format]);
}

static void on_temp_breakpoint_toggled(GtkToggleButton *togglebutton, GtkWidget *widget)
{
	gtk_widget_set_sensitive(widget, gtk_widget_get_sensitive(temp_breakpoint) &&
		gtk_toggle_button_get_active(togglebutton));
}

static void on_program_import_button_clicked(G_GNUC_UNUSED GtkButton *button,
	G_GNUC_UNUSED gpointer gdata)
{
	const gchar *executable = build_get_execute(GEANY_BC_COMMAND);
	const char *workdir = build_get_execute(GEANY_BC_WORKING_DIR);
	gtk_entry_set_text(program_exec_entry, executable ? executable : "");
	gtk_entry_set_text(working_dir_entry, workdir ? workdir : "");
}

static gboolean check_dialog_path(GtkEntry *entry, gboolean file, int mode)
{
	const gchar *pathname = gtk_entry_get_text(entry);

	if (utils_check_path(pathname, file, mode))
		return TRUE;

	if (errno == ENOENT)
	{
		return dialogs_show_question(_("%s: %s.\n\nContinue?"), pathname,
			g_strerror(errno));
	}

	show_errno(pathname);
	return FALSE;
}

static void on_program_ok_button_clicked(G_GNUC_UNUSED GtkButton *button,
	G_GNUC_UNUSED gpointer gdata)
{
	if (check_dialog_path(program_exec_entry, TRUE, R_OK | X_OK) &&
		check_dialog_path(working_dir_entry, FALSE, X_OK) &&
		check_dialog_path(load_script_entry, TRUE, R_OK))
	{
		const gchar *program_name = gtk_entry_get_text(program_exec_entry);

		if (*program_name == '\0')
			program_name = gtk_entry_get_text(load_script_entry);

		if (utils_filenamecmp(program_name, *program_executable ? program_executable :
			program_load_script))
		{
			save_program_settings();
		}

		stash_foreach((GFunc) stash_group_update, NULL);
		option_long_mr_format = dialog_long_mr_format;
		g_free(program_environment);
		program_environment = utils_text_buffer_get_text(environment, -1);
		save_program_settings();
		recent_menu_create();
		program_configure();
		gtk_widget_hide(program_dialog);

		if (gtk_toggle_button_get_active(delete_all_items) &&
			dialogs_show_question(_("Delete all breakpoints, watches et cetera?")))
		{
			breaks_delete_all();
			watches_delete_all();
			inspects_delete_all();
			registers_delete_all();
		}
	}
}

void program_load_config(GKeyFile *config)
{
	utils_load(config, "recent", recent_program_load);
	recent_menu_create();
	config = g_key_file_new();
	/* load from empty config file == set defaults */
	stash_foreach((GFunc) stash_group_load_from_key_file, config);
	g_key_file_free(config);
	program_configure();
}

void program_init(void)
{
	GtkWidget *widget;
	StashGroup *group = stash_group_new("program");
	extern gboolean thread_select_on_running;
	extern gboolean thread_select_on_stopped;
	extern gboolean thread_select_on_exited;
	extern gboolean thread_select_follow;

	program_dialog = dialog_connect("program_dialog");
	program_page_vbox = get_widget("program_page_vbox");

	program_exec_entry = GTK_ENTRY(get_widget("program_executable_entry"));
	gtk_entry_set_max_length(program_exec_entry, PATH_MAX);
	stash_group_add_entry(group, &program_executable, "executable", "", program_exec_entry);
	ui_setup_open_button_callback(get_widget("program_executable_button"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN, program_exec_entry);

	stash_group_add_entry(group, &program_arguments, "arguments", "",
		get_widget("program_arguments_entry"));
	widget = get_widget("program_environment");
	environment = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
	stash_group_add_string(group, &program_environment, "environment", "");

	working_dir_entry = GTK_ENTRY(get_widget("program_working_dir_entry"));
	gtk_entry_set_max_length(working_dir_entry, PATH_MAX);
	stash_group_add_entry(group, &program_working_dir, "working_dir", "", working_dir_entry);
	ui_setup_open_button_callback(get_widget("program_working_dir_button"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN, working_dir_entry);

	load_script_entry = GTK_ENTRY(get_widget("program_load_script_entry"));
	gtk_entry_set_max_length(load_script_entry, PATH_MAX);
	stash_group_add_entry(group, &program_load_script, "load_script", "", load_script_entry);
	ui_setup_open_button_callback(get_widget("program_load_script_button"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN, load_script_entry);

	auto_run_exit = get_widget("program_auto_run_exit");
	stash_group_add_toggle_button(group, &program_auto_run_exit, "auto_run_exit", TRUE,
		auto_run_exit);
	stash_group_add_toggle_button(group, &program_non_stop_mode, "non_stop_mode", FALSE,
		get_widget("program_non_stop_mode"));

	temp_breakpoint = get_widget("program_temp_breakpoint");
	stash_group_add_toggle_button(group, &program_temp_breakpoint, "temp_breakpoint", FALSE,
		temp_breakpoint);
	widget = get_widget("program_temp_break_location");
	gtk_entry_set_max_length(GTK_ENTRY(widget), PATH_MAX + 0xFF);
	stash_group_add_entry(group, &program_temp_break_location, "temp_break_location", "",
		widget);
	program_group = group;
	delete_all_items = GTK_TOGGLE_BUTTON(get_widget("program_delete_all_items"));

	g_signal_connect(program_exec_entry, "changed",
		G_CALLBACK(on_program_name_entry_changed), NULL);
	g_signal_connect(load_script_entry, "changed",
		G_CALLBACK(on_program_name_entry_changed), NULL);
	g_signal_connect(temp_breakpoint, "toggled", G_CALLBACK(on_temp_breakpoint_toggled),
		widget);

	import_button = get_widget("program_import");
	g_signal_connect(import_button, "clicked", G_CALLBACK(on_program_import_button_clicked),
		NULL);

	widget = get_widget("program_ok");
	g_signal_connect(widget, "clicked", G_CALLBACK(on_program_ok_button_clicked), NULL);
	gtk_widget_grab_default(widget);

	group = stash_group_new("options");

	stash_group_add_toggle_button(group, &option_open_panel_on_load, "open_panel_on_load",
		TRUE, get_widget("option_open_panel_on_load"));
	stash_group_add_toggle_button(group, &option_open_panel_on_start, "open_panel_on_start",
		TRUE, get_widget("option_open_panel_on_start"));
	stash_group_add_toggle_button(group, &option_update_all_views, "update_all_views", FALSE,
		get_widget("option_update_all_views"));

	stash_group_add_radio_buttons(group, &option_high_bit_mode, "high_bit_mode", HB_7BIT,
		get_widget("option_high_bit_mode_7bit"), HB_7BIT,
		get_widget("option_high_bit_mode_locale"), HB_LOCALE,
		get_widget("option_high_bit_mode_utf8"), HB_UTF8, NULL);

	stash_group_add_toggle_button(group, &option_member_names, "member_names", TRUE,
		get_widget("option_member_names"));
	stash_group_add_toggle_button(group, &option_argument_names, "argument_names", TRUE,
		get_widget("option_argument_names"));
	long_mr_format = GTK_BUTTON(get_widget("option_mr_long_mr_format"));
	stash_group_add_boolean(group, &option_long_mr_format, "long_mr_format", TRUE);
	LONG_MR_FORMAT[FALSE] = _("as _Name=value");
	LONG_MR_FORMAT[TRUE] = _("as _Name = value");
	g_signal_connect(long_mr_format, "clicked", G_CALLBACK(on_long_mr_format_clicked), NULL);

	stash_group_add_toggle_button(group, &option_inspect_expand, "inspect_expand", TRUE,
		get_widget("option_inspect_expand"));
	stash_group_add_spin_button_integer(group, &option_inspect_count, "inspect_count", 100,
		get_widget("option_inspect_count"));

	stash_group_add_toggle_button(group, &option_library_messages, "library_messages", FALSE,
		get_widget("option_library_messages"));
	stash_group_add_toggle_button(group, &option_editor_tooltips, "editor_tooltips", TRUE,
		get_widget("option_editor_tooltips"));
	stash_group_add_boolean(group, &stack_show_address, "stack_show_address", TRUE);
	options_group = group;

	group = stash_group_new("terminal");
#ifdef G_OS_UNIX
	stash_group_add_boolean(group, &terminal_auto_show, "auto_show", FALSE);
	stash_group_add_boolean(group, &terminal_auto_hide, "auto_hide", FALSE);
	stash_group_add_boolean(group, &terminal_show_on_error, "show_on_error", FALSE);
#endif
	terminal_group = group;

	group = stash_group_new("thread");
	stash_group_add_boolean(group, &thread_select_on_running, "select_on_running", FALSE);
	stash_group_add_boolean(group, &thread_select_on_stopped, "select_on_stopped", TRUE);
	stash_group_add_boolean(group, &thread_select_on_exited, "select_on_exited", TRUE);
	stash_group_add_boolean(group, &thread_select_follow, "select_follow", TRUE);
	stash_group_add_boolean(group, &thread_show_group, "show_group", TRUE);
	stash_group_add_boolean(group, &thread_show_core, "show_core", TRUE);
	thread_group = group;

	recent_programs = SCP_TREE_STORE(get_object("recent_program_store"));
	recent_bitmap = 0;
	recent_menu = get_widget("program_recent_menu");
}

void program_finalize(void)
{
	char *configfile = prefs_file_name();
	GKeyFile *config = g_key_file_new();

	save_program_settings();
	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);
	store_save(recent_programs, config, "recent", recent_program_save);
	utils_key_file_write_to_file(config, configfile);
	g_key_file_free(config);
	g_free(configfile);

	gtk_widget_destroy(program_dialog);
	stash_foreach((GFunc) stash_group_destroy, NULL);
}
