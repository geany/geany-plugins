/*
 * Tweaks-UI Plugin for Geany
 * Copyright 2021 xiota
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "auxiliary.h"
#include "tkui_main.h"
#include "tkui_settings.h"

// global variables

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

namespace {  // useful variables

GtkWindow *geany_window = nullptr;
GtkNotebook *geany_sidebar = nullptr;
GtkNotebook *geany_msgwin = nullptr;
GtkNotebook *geany_editor = nullptr;
GtkWidget *geany_menubar = nullptr;

GtkWidget *g_tweaks_menu = nullptr;

GeanyKeyGroup *gKeyGroup = nullptr;

// There should be no need to access settings outside this file
// because each class has access to its own settings.
TweakUiSettings settings;

}  // namespace

namespace {

bool g_handle_reload_config = false;

gboolean reload_config(gpointer user_data) {
  settings.load();

  settings.column_markers.update_columns();
  settings.hide_menubar.update();

  settings.sidebar_auto_position.initialize();
  settings.window_geometry.initialize();

  g_handle_reload_config = false;
  return false;
}

void tkui_pref_reload_config(GtkWidget *self, GtkWidget *dialog) {
  if (!g_handle_reload_config) {
    g_handle_reload_config = true;
    g_idle_add(reload_config, nullptr);
  }
}

void tkui_pref_save_config(GtkWidget *self, GtkWidget *dialog) {
  settings.save();
}

void tkui_pref_reset_config(GtkWidget *self, GtkWidget *dialog) {
  settings.reset();
}

void tkui_pref_open_config_folder(GtkWidget *self, GtkWidget *dialog) {
  std::string conf_dn = cstr_assign(
      g_build_filename(geany_data->app->configdir, "plugins", nullptr));

  std::string command = R"(xdg-open ")" + conf_dn + R"(")";
  (void)!system(command.c_str());
}

void tkui_pref_edit_config(GtkWidget *self, GtkWidget *dialog) {
  const std::string &filename = settings.config_file;
  if (filename.empty()) {
    return;
  }

  GeanyDocument *doc =
      document_open_file(filename.c_str(), false, nullptr, nullptr);
  document_reload_force(doc, nullptr);

  if (dialog != nullptr) {
    gtk_widget_destroy(GTK_WIDGET(dialog));
  }
}

void tkui_menu_preferences(GtkWidget *self, GtkWidget *dialog) {
  plugin_show_configure(geany_plugin);
}

/* ********************
 * Pane Position Callbacks
 */

bool tkui_key_binding(int key_id) {
  switch (key_id) {
    case TKUI_KEY_TOGGLE_MENUBAR_VISIBILITY:
      settings.hide_menubar.toggle();
      break;
    case TKUI_KEY_COPY_1:
    case TKUI_KEY_COPY_2:
      keybindings_send_command(GEANY_KEY_GROUP_CLIPBOARD,
                               GEANY_KEYS_CLIPBOARD_COPY);
      break;
    case TKUI_KEY_PASTE_1:
    case TKUI_KEY_PASTE_2:
      keybindings_send_command(GEANY_KEY_GROUP_CLIPBOARD,
                               GEANY_KEYS_CLIPBOARD_PASTE);
      break;
    case TKUI_KEY_TOGGLE_READONLY:
      settings.auto_read_only.toggle();
      break;
    case TKUI_KEY_REDETECT_FILETYPE:
      settings.detect_filetype_on_reload.redetect_filetype();
      break;
    case TKUI_KEY_REDETECT_FILETYPE_FORCE:
      settings.detect_filetype_on_reload.force_redetect_filetype();
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

namespace {

gboolean tkui_plugin_init(GeanyPlugin *plugin, gpointer data) {
  geany_plugin = plugin;
  geany_data = plugin->geany_data;

  // geany widgets for later use
  geany_window = GTK_WINDOW(geany->main_widgets->window);
  geany_sidebar = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
  geany_msgwin = GTK_NOTEBOOK(geany->main_widgets->message_window_notebook);
  geany_editor = GTK_NOTEBOOK(geany->main_widgets->notebook);
  geany_menubar = ui_lookup_widget(GTK_WIDGET(geany_window), "hbox_menubar");

  settings.initialize();
  settings.load();

  // setup menu
  GtkWidget *item;

  g_tweaks_menu = gtk_menu_item_new_with_label("Tweaks-UI");
  ui_add_document_sensitive(g_tweaks_menu);

  GtkWidget *submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(g_tweaks_menu), submenu);

  item = gtk_menu_item_new_with_label("Edit Config File");
  g_signal_connect(item, "activate", G_CALLBACK(tkui_pref_edit_config),
                   nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

  item = gtk_menu_item_new_with_label("Reload Config File");
  g_signal_connect(item, "activate", G_CALLBACK(tkui_pref_reload_config),
                   nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

  item = gtk_menu_item_new_with_label("Open Config Folder");
  g_signal_connect(item, "activate", G_CALLBACK(tkui_pref_open_config_folder),
                   nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

  item = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

  item = gtk_menu_item_new_with_label("Preferences");
  g_signal_connect(item, "activate", G_CALLBACK(tkui_menu_preferences),
                   nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

  gtk_widget_show_all(g_tweaks_menu);

  gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu),
                        g_tweaks_menu);

  // Set keyboard shortcuts
  gKeyGroup = plugin_set_key_group(geany_plugin, "Tweaks-UI", TKUI_KEY_COUNT,
                                   (GeanyKeyGroupCallback)tkui_key_binding);

  keybindings_set_item(gKeyGroup, TKUI_KEY_COPY_1, nullptr, 0,
                       GdkModifierType(0), "tweaks_ui_copy_1", _("Edit/Copy 1"),
                       nullptr);

  keybindings_set_item(gKeyGroup, TKUI_KEY_COPY_2, nullptr, 0,
                       GdkModifierType(0), "tweaks_ui_copy_2", _("Edit/Copy 2"),
                       nullptr);

  keybindings_set_item(gKeyGroup, TKUI_KEY_PASTE_1, nullptr, 0,
                       GdkModifierType(0), "tweaks_ui_paste_1",
                       _("Edit/Paste 1"), nullptr);

  keybindings_set_item(gKeyGroup, TKUI_KEY_PASTE_2, nullptr, 0,
                       GdkModifierType(0), "tweaks_ui_paste_2",
                       _("Edit/Paste 2"), nullptr);

  keybindings_set_item(gKeyGroup, TKUI_KEY_TOGGLE_MENUBAR_VISIBILITY, nullptr,
                       0, GdkModifierType(0),
                       "tweaks_ui_toggle_visibility_menubar_",
                       _("Toggle visibility of the menubar."), nullptr);

  keybindings_set_item(gKeyGroup, TKUI_KEY_TOGGLE_READONLY, nullptr, 0,
                       GdkModifierType(0), "tweaks_ui_toggle_readonly",
                       _("Toggle document read-only state"), nullptr);

  keybindings_set_item(gKeyGroup, TKUI_KEY_REDETECT_FILETYPE, nullptr, 0,
                       GdkModifierType(0), "tweaks_ui_redetect_filetype",
                       _("Redetect filetype.  Switch to detected type if the "
                         "current type is likely incorrect."),
                       nullptr);

  keybindings_set_item(
      gKeyGroup, TKUI_KEY_REDETECT_FILETYPE_FORCE, nullptr, 0,
      GdkModifierType(0), "tweaks_ui_redetect_filetype_force",
      _("Redetect fietype.  Switch to detected type unconditionally."),
      nullptr);

  // initialize hide menubar
  settings.hide_menubar.initialize(gKeyGroup,
                                   TKUI_KEY_TOGGLE_MENUBAR_VISIBILITY);

  settings.auto_read_only.initialize();
  settings.column_markers.initialize();
  settings.detect_filetype_on_reload.initialize();
  settings.unchange_document.initialize();

  settings.colortip.initialize();
  settings.markword.initialize();

  if (g_handle_reload_config == 0) {
    g_handle_reload_config = 1;
    g_idle_add(reload_config, nullptr);
  }

  return true;
}

void tkui_plugin_cleanup(GeanyPlugin *plugin, gpointer data) {
  gtk_widget_destroy(g_tweaks_menu);

  settings.sidebar_auto_position.disconnect();
  settings.window_geometry.disconnect();

  settings.save_session();
  settings.kf_close();
}

GtkWidget *tkui_plugin_configure(GeanyPlugin *plugin, GtkDialog *dialog,
                                 gpointer pdata) {
  GtkWidget *box, *btn;
  char *tooltip;

  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

  tooltip = g_strdup(_("Save the active settings to the config file."));
  btn = gtk_button_new_with_label(_("Save Config"));
  g_signal_connect(btn, "clicked", G_CALLBACK(tkui_pref_save_config), dialog);
  gtk_box_pack_start(GTK_BOX(box), btn, false, false, 3);
  gtk_widget_set_tooltip_text(btn, tooltip);
  GFREE(tooltip);

  tooltip = g_strdup(
      _("Reload settings from the config file.  May be used "
        "to apply preferences after editing without restarting Geany."));
  btn = gtk_button_new_with_label(_("Reload Config"));
  g_signal_connect(btn, "clicked", G_CALLBACK(tkui_pref_reload_config), dialog);
  gtk_box_pack_start(GTK_BOX(box), btn, false, false, 3);
  gtk_widget_set_tooltip_text(btn, tooltip);
  GFREE(tooltip);

  tooltip =
      g_strdup(_("Delete the current config file and restore the default "
                 "settings with explanatory comments."));
  btn = gtk_button_new_with_label(_("Reset Config"));
  g_signal_connect(btn, "clicked", G_CALLBACK(tkui_pref_reset_config), dialog);
  gtk_box_pack_start(GTK_BOX(box), btn, false, false, 3);
  gtk_widget_set_tooltip_text(btn, tooltip);
  GFREE(tooltip);

  tooltip = g_strdup(_("Open the config file in Geany for editing."));
  btn = gtk_button_new_with_label(_("Edit Config"));
  g_signal_connect(btn, "clicked", G_CALLBACK(tkui_pref_edit_config), dialog);
  gtk_box_pack_start(GTK_BOX(box), btn, false, false, 3);
  gtk_widget_set_tooltip_text(btn, tooltip);
  GFREE(tooltip);

  tooltip = g_strdup(
      _("Open the config folder in the default file manager.  The config "
        "folder contains stylesheets, which may be edited."));
  btn = gtk_button_new_with_label(_("Open Config Folder"));
  g_signal_connect(btn, "clicked", G_CALLBACK(tkui_pref_open_config_folder),
                   dialog);
  gtk_box_pack_start(GTK_BOX(box), btn, false, false, 3);
  gtk_widget_set_tooltip_text(btn, tooltip);
  GFREE(tooltip);

  return box;
}

}  // namespace

G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin) {
  // translation
  main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

  // plugin metadata
  plugin->info->name = "Tweaks-UI";
  plugin->info->description = _("User-interface tweaks for Geany.");
  plugin->info->version = VERSION;
  plugin->info->author = "xiota";

  // plugin functions
  plugin->funcs->init = tkui_plugin_init;
  plugin->funcs->cleanup = tkui_plugin_cleanup;
  plugin->funcs->help = nullptr;
  plugin->funcs->configure = tkui_plugin_configure;
  plugin->funcs->callbacks = nullptr;

  // register
  GEANY_PLUGIN_REGISTER(plugin, 226);
}
