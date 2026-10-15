/*
 *      playdate.c - Geany Playdate plugin
 *
 *      Copyright (C) 2026 geany-playdate contributors
 *      SPDX-License-Identifier: GPL-3.0-or-later
 *
 *      Build & run Playdate games from Geany using the Playdate SDK
 *      (pdc + PlaydateSimulator).
 *
 *      Behaviour inspired by:
 *        - orta/vscode-playdate        (MIT — simple build+run command)
 *        - midouest/vscode-playdate-debug (MIT — pdc problem matchers, flags)
 *        - notpeter/playdate-luacats   (Apache-2.0 OR MIT — API tags)
 *
 *      See LICENSE and THIRD_PARTY.md.
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "playdate.h"

#include <string.h>

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

static PDData *pd = NULL;

enum
{
	KB_BUILD,
	KB_RUN,
	KB_BUILD_RUN,
	KB_CLEAN,
	KB_OPEN_SIM,
	COUNT_KB
};

static GtkWidget *menu_root = NULL;
static GtkWidget *menu_build = NULL;
static GtkWidget *menu_run = NULL;
static GtkWidget *menu_build_run = NULL;
static GtkWidget *menu_clean = NULL;
static GtkWidget *menu_open_sim = NULL;
static GtkWidget *menu_install_tags = NULL;


static void on_build(GtkMenuItem *item, gpointer user_data)
{
	(void) item;
	pd_build_project(pd, FALSE);
}


static void on_run(GtkMenuItem *item, gpointer user_data)
{
	(void) item;
	pd_run_simulator(pd);
}


static void on_build_run(GtkMenuItem *item, gpointer user_data)
{
	(void) item;
	pd_build_project(pd, TRUE);
}


static void on_clean(GtkMenuItem *item, gpointer user_data)
{
	(void) item;
	pd_clean_output(pd);
}


static void on_open_sim(GtkMenuItem *item, gpointer user_data)
{
	(void) item;
	pd_open_simulator_only(pd);
}


static void on_install_tags(GtkMenuItem *item, gpointer user_data)
{
	(void) item;
	pd_tags_install();
}


static void on_kb(G_GNUC_UNUSED guint key_id)
{
	switch (key_id)
	{
		case KB_BUILD:
			pd_build_project(pd, FALSE);
			break;
		case KB_RUN:
			pd_run_simulator(pd);
			break;
		case KB_BUILD_RUN:
			pd_build_project(pd, TRUE);
			break;
		case KB_CLEAN:
			pd_clean_output(pd);
			break;
		case KB_OPEN_SIM:
			pd_open_simulator_only(pd);
			break;
		default:
			break;
	}
}


static void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	PDData *data = user_data;

	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GtkEntry *e_sdk, *e_src, *e_out, *e_name;
		GtkCheckButton *c_strip, *c_nocompress, *c_verbose, *c_quiet, *c_skip;

		e_sdk = g_object_get_data(G_OBJECT(dialog), "pd_sdk");
		e_src = g_object_get_data(G_OBJECT(dialog), "pd_src");
		e_out = g_object_get_data(G_OBJECT(dialog), "pd_out");
		e_name = g_object_get_data(G_OBJECT(dialog), "pd_name");
		c_strip = g_object_get_data(G_OBJECT(dialog), "pd_strip");
		c_nocompress = g_object_get_data(G_OBJECT(dialog), "pd_nocompress");
		c_verbose = g_object_get_data(G_OBJECT(dialog), "pd_verbose");
		c_quiet = g_object_get_data(G_OBJECT(dialog), "pd_quiet");
		c_skip = g_object_get_data(G_OBJECT(dialog), "pd_skip");

		g_free(data->config.sdk_path);
		g_free(data->config.source);
		g_free(data->config.output);
		g_free(data->config.product_name);

		data->config.sdk_path = g_strdup(gtk_entry_get_text(e_sdk));
		data->config.source = g_strdup(gtk_entry_get_text(e_src));
		data->config.output = g_strdup(gtk_entry_get_text(e_out));
		data->config.product_name = g_strdup(gtk_entry_get_text(e_name));
		data->config.strip = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c_strip));
		data->config.no_compress = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c_nocompress));
		data->config.verbose = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c_verbose));
		data->config.quiet = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c_quiet));
		data->config.skip_unknown = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c_skip));

		/* empty source/output means auto-detect; do NOT force defaults back */
		pd_config_save(&data->config);
		ui_set_statusbar(TRUE, _("Playdate: preferences saved"));
	}
}


static GtkWidget *add_row(GtkGrid *grid, gint row, const gchar *label_text, GtkWidget *widget)
{
	GtkWidget *label = gtk_label_new_with_mnemonic(label_text);

	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(label, 6);
	gtk_widget_set_margin_end(label, 6);
	gtk_grid_attach(grid, label, 0, row, 1, 1);
	gtk_grid_attach(grid, widget, 1, row, 1, 1);
	gtk_widget_set_hexpand(widget, TRUE);
	return widget;
}


static void pd_ui_configure(GeanyPlugin *plugin, GtkDialog *dialog)
{
	GtkWidget *vbox, *grid;
	GtkWidget *e_sdk, *e_src, *e_out, *e_name;
	GtkWidget *c_strip, *c_nocompress, *c_verbose, *c_quiet, *c_skip;
	gchar *hint;
	gint row = 0;

	(void) plugin;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_top(vbox, 6);
	gtk_widget_set_margin_bottom(vbox, 6);
	gtk_widget_set_margin_start(vbox, 6);
	gtk_widget_set_margin_end(vbox, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(dialog)), vbox, TRUE, TRUE, 0);

	hint = g_strdup_printf(
		_("SDK path is optional if PLAYDATE_SDK_PATH is set.\n"
		  "Source and output are relative to the project root.\n"
		  "Config file: %s"),
		"~/.config/geany/plugins/geany-playdate/playdate.conf");
	{
		GtkWidget *l = gtk_label_new(hint);
		gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
		gtk_widget_set_halign(l, GTK_ALIGN_START);
		gtk_box_pack_start(GTK_BOX(vbox), l, FALSE, FALSE, 0);
	}
	g_free(hint);

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

	e_sdk = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e_sdk), pd->config.sdk_path ? pd->config.sdk_path : "");
	gtk_widget_set_tooltip_text(e_sdk, _("Leave empty to use PLAYDATE_SDK_PATH"));
	add_row(GTK_GRID(grid), row++, _("_SDK path:"), e_sdk);

	e_src = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e_src), pd->config.source ? pd->config.source : PD_DEFAULT_SOURCE);
	add_row(GTK_GRID(grid), row++, _("_Source directory:"), e_src);

	e_out = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e_out), pd->config.output ? pd->config.output : PD_DEFAULT_OUTPUT);
	add_row(GTK_GRID(grid), row++, _("_Output .pdx:"), e_out);

	e_name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e_name), pd->config.product_name ? pd->config.product_name : "");
	gtk_widget_set_tooltip_text(e_name, _("Optional; matches name in pdxinfo"));
	add_row(GTK_GRID(grid), row++, _("_Product name:"), e_name);

	c_strip = gtk_check_button_new_with_mnemonic(_("_Strip debug symbols (-s)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c_strip), pd->config.strip);
	add_row(GTK_GRID(grid), row++, "", c_strip);

	c_nocompress = gtk_check_button_new_with_mnemonic(_("_No compression (--no-compress)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c_nocompress), pd->config.no_compress);
	add_row(GTK_GRID(grid), row++, "", c_nocompress);

	c_verbose = gtk_check_button_new_with_mnemonic(_("_Verbose (-v)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c_verbose), pd->config.verbose);
	add_row(GTK_GRID(grid), row++, "", c_verbose);

	c_quiet = gtk_check_button_new_with_mnemonic(_("_Quiet (-q)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c_quiet), pd->config.quiet);
	add_row(GTK_GRID(grid), row++, "", c_quiet);

	c_skip = gtk_check_button_new_with_mnemonic(_("_Skip unknown files (-k)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c_skip), pd->config.skip_unknown);
	add_row(GTK_GRID(grid), row++, "", c_skip);

	g_object_set_data(G_OBJECT(dialog), "pd_sdk", e_sdk);
	g_object_set_data(G_OBJECT(dialog), "pd_src", e_src);
	g_object_set_data(G_OBJECT(dialog), "pd_out", e_out);
	g_object_set_data(G_OBJECT(dialog), "pd_name", e_name);
	g_object_set_data(G_OBJECT(dialog), "pd_strip", c_strip);
	g_object_set_data(G_OBJECT(dialog), "pd_nocompress", c_nocompress);
	g_object_set_data(G_OBJECT(dialog), "pd_verbose", c_verbose);
	g_object_set_data(G_OBJECT(dialog), "pd_quiet", c_quiet);
	g_object_set_data(G_OBJECT(dialog), "pd_skip", c_skip);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), pd);

	gtk_widget_show_all(vbox);
}


static gboolean pd_cb_init(GeanyPlugin *plugin, gpointer pdata)
{
	GeanyKeyGroup *key_group;
	PDData *data = pdata;

	pd = data;
	pd->plugin = plugin;
	pd_config_load(&pd->config);

	/* submenu under Tools */
	menu_root = gtk_menu_item_new_with_mnemonic(_("_Playdate"));
	gtk_widget_show(menu_root);

	{
		GtkWidget *submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_root), submenu);

		menu_build_run = gtk_menu_item_new_with_mnemonic(_("Build and _Run"));
		menu_build = gtk_menu_item_new_with_mnemonic(_("_Build"));
		menu_run = gtk_menu_item_new_with_mnemonic(_("_Run in Simulator"));
		menu_clean = gtk_menu_item_new_with_mnemonic(_("C_lean"));
		menu_open_sim = gtk_menu_item_new_with_mnemonic(_("Open Simu_lator"));
		menu_install_tags = gtk_menu_item_new_with_mnemonic(_("Install API _Tags"));

		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menu_build_run);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menu_build);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menu_run);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menu_clean);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menu_open_sim);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menu_install_tags);

		g_signal_connect(menu_build_run, "activate", G_CALLBACK(on_build_run), NULL);
		g_signal_connect(menu_build, "activate", G_CALLBACK(on_build), NULL);
		g_signal_connect(menu_run, "activate", G_CALLBACK(on_run), NULL);
		g_signal_connect(menu_clean, "activate", G_CALLBACK(on_clean), NULL);
		g_signal_connect(menu_open_sim, "activate", G_CALLBACK(on_open_sim), NULL);
		g_signal_connect(menu_install_tags, "activate", G_CALLBACK(on_install_tags), NULL);

		gtk_widget_show_all(submenu);
	}

	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_root);

	/* keybindings */
	key_group = plugin_set_key_group(plugin, "playdate", COUNT_KB, NULL);
	keybindings_set_item(key_group, KB_BUILD, on_kb,
		GDK_KEY_b, GEANY_PRIMARY_MOD_MASK | GDK_SHIFT_MASK,
		"playdate_build", _("Playdate: Build"), menu_build);
	keybindings_set_item(key_group, KB_RUN, on_kb,
		GDK_KEY_r, GEANY_PRIMARY_MOD_MASK | GDK_SHIFT_MASK,
		"playdate_run", _("Playdate: Run in Simulator"), menu_run);
	keybindings_set_item(key_group, KB_BUILD_RUN, on_kb,
		GDK_KEY_F5, 0,
		"playdate_build_run", _("Playdate: Build and Run"), menu_build_run);
	keybindings_set_item(key_group, KB_CLEAN, on_kb,
		GDK_KEY_F5, GEANY_PRIMARY_MOD_MASK | GDK_SHIFT_MASK,
		"playdate_clean", _("Playdate: Clean"), menu_clean);
	keybindings_set_item(key_group, KB_OPEN_SIM, on_kb,
		0, 0,
		"playdate_open_sim", _("Playdate: Open Simulator"), menu_open_sim);

	/* SDK status on load */
	{
		gchar *sdk = pd_resolve_sdk_path(&pd->config);
		if (sdk != NULL)
		{
			gchar *pdc = g_build_filename(sdk, "bin", "pdc", NULL);
			if (g_file_test(pdc, G_FILE_TEST_EXISTS))
				ui_set_statusbar(FALSE, _("Playdate: SDK found at %s"), sdk);
			else
				ui_set_statusbar(FALSE, _("Playdate: pdc not found under %s"), sdk);
			g_free(pdc);
			g_free(sdk);
		}
		else
		{
			ui_set_statusbar(FALSE,
				_("Playdate: set PLAYDATE_SDK_PATH or configure the plugin"));
		}
	}

	return TRUE;
}


static GtkWidget *pd_cb_configure(GeanyPlugin *plugin, GtkDialog *dialog, gpointer pdata)
{
	(void) pdata;
	pd_ui_configure(plugin, dialog);
	/* The dialog already has content; return NULL to signal we added widgets
	 * to the dialog ourselves (Geany still runs response handling). */
	return NULL;
}


static void pd_cb_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	PDData *data = pdata;

	(void) plugin;
	(void) data;

	if (menu_root != NULL)
	{
		gtk_widget_destroy(menu_root);
		menu_root = NULL;
	}

	/* pdata is freed via the destroy notify registered in geany_load_module(). */
	pd = NULL;
}


static void pd_free_data(gpointer p)
{
	PDData *data = p;
	if (data == NULL)
		return;
	pd_config_free(&data->config);
	g_free(data);
}


void geany_load_module(GeanyPlugin *plugin)
{
	PDData *data;

	plugin->info->name = PD_PLUGIN_NAME;
	plugin->info->description = PD_PLUGIN_DESCRIPTION;
	plugin->info->version = PD_PLUGIN_VERSION;
	plugin->info->author = PD_PLUGIN_AUTHOR;

	plugin->funcs->init = pd_cb_init;
	plugin->funcs->configure = pd_cb_configure;
	plugin->funcs->cleanup = pd_cb_cleanup;

	data = g_new0(PDData, 1);
	pd_config_init_defaults(&data->config);

	geany_plugin = plugin;
	geany_data = plugin->geany_data;

	GEANY_PLUGIN_REGISTER_FULL(plugin, 224, data, pd_free_data);
}
