/*
 * Settings - Tweaks-UI Plugin for Geany
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

#include "auxiliary.h"
#include "tkui_settings.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void TweakUiSettings::initialize() {
  // AutoReadOnly
  add_setting((TkuiSetting *)&auto_read_only.enable, TKUI_PREF_TYPE_BOOLEAN,
              "auto_read_only", "\n " + auto_read_only.desc_enable, false);

  // HideMenubar
  add_setting((TkuiSetting *)&hide_menubar.hide_on_start,
              TKUI_PREF_TYPE_BOOLEAN, "menubar_hide_on_start",
              "\n " + hide_menubar.desc_hide_on_start, false);
  add_setting((TkuiSetting *)&hide_menubar.restore_state,
              TKUI_PREF_TYPE_BOOLEAN, "menubar_restore_state",
              "   " + hide_menubar.desc_restore_state, false);

  add_setting((TkuiSetting *)&hide_menubar.previous_state,
              TKUI_PREF_TYPE_BOOLEAN, "menubar_previous_state", {}, true);

  // DetectFileTypeOnReload
  add_setting((TkuiSetting *)&detect_filetype_on_reload.enable,
              TKUI_PREF_TYPE_BOOLEAN, "detect_filetype_on_reload",
              "\n " + detect_filetype_on_reload.desc_enable, false);

  // unchange document
  add_setting((TkuiSetting *)&unchange_document.enable, TKUI_PREF_TYPE_BOOLEAN,
              "unchange_document_enable", "\n " + unchange_document.desc_enable,
              false);

  // Window Geometry
  add_setting((TkuiSetting *)&window_geometry.enable, TKUI_PREF_TYPE_BOOLEAN,
              "geometry_enable", "\n " + window_geometry.description, false);
  add_setting((TkuiSetting *)&window_geometry.geometry_update,
              TKUI_PREF_TYPE_BOOLEAN, "geometry_update",
              "   " + window_geometry.desc_geometry_update, false);
  add_setting((TkuiSetting *)&window_geometry.sidebar_enable,
              TKUI_PREF_TYPE_BOOLEAN, "geometry_sidebar",
              "  " + window_geometry.desc_sidebar_enable, false);
  add_setting((TkuiSetting *)&window_geometry.msgwin_enable,
              TKUI_PREF_TYPE_BOOLEAN, "geometry_msgwin",
              "   " + window_geometry.desc_msgwin_enable, false);

  add_setting((TkuiSetting *)&window_geometry.xpos, TKUI_PREF_TYPE_INTEGER,
              "geometry_xpos", {}, true);
  add_setting((TkuiSetting *)&window_geometry.ypos, TKUI_PREF_TYPE_INTEGER,
              "geometry_ypos", {}, true);
  add_setting((TkuiSetting *)&window_geometry.width, TKUI_PREF_TYPE_INTEGER,
              "geometry_width", {}, true);
  add_setting((TkuiSetting *)&window_geometry.height, TKUI_PREF_TYPE_INTEGER,
              "geometry_height", {}, true);

  add_setting((TkuiSetting *)&window_geometry.xpos_maximized,
              TKUI_PREF_TYPE_INTEGER, "geometry_xpos_maximized", {}, true);
  add_setting((TkuiSetting *)&window_geometry.ypos_maximized,
              TKUI_PREF_TYPE_INTEGER, "geometry_ypos_maximized", {}, true);
  add_setting((TkuiSetting *)&window_geometry.width_maximized,
              TKUI_PREF_TYPE_INTEGER, "geometry_width_maximized", {}, true);
  add_setting((TkuiSetting *)&window_geometry.height_maximized,
              TKUI_PREF_TYPE_INTEGER, "geometry_height_maximized", {}, true);

  add_setting((TkuiSetting *)&window_geometry.sidebar_maximized,
              TKUI_PREF_TYPE_INTEGER, "geometry_sidebar_maximized", {}, true);
  add_setting((TkuiSetting *)&window_geometry.sidebar_normal,
              TKUI_PREF_TYPE_INTEGER, "geometry_sidebar_normal", {}, true);
  add_setting((TkuiSetting *)&window_geometry.msgwin_maximized,
              TKUI_PREF_TYPE_INTEGER, "geometry_msgwin_maximized", {}, true);
  add_setting((TkuiSetting *)&window_geometry.msgwin_normal,
              TKUI_PREF_TYPE_INTEGER, "geometry_msgwin_normal", {}, true);

  // sidebar auto size
  add_setting((TkuiSetting *)&sidebar_auto_position.enable,
              TKUI_PREF_TYPE_BOOLEAN, "sidebar_auto_size_enable",
              "\n " + sidebar_auto_position.description, false);

  add_setting((TkuiSetting *)&sidebar_auto_position.columns_normal,
              TKUI_PREF_TYPE_INTEGER, "sidebar_auto_size_normal", {}, true);
  add_setting((TkuiSetting *)&sidebar_auto_position.columns_maximized,
              TKUI_PREF_TYPE_INTEGER, "sidebar_auto_size_maximized", {}, true);

  // ColorTip
  add_setting((TkuiSetting *)&colortip.color_tooltip, TKUI_PREF_TYPE_BOOLEAN,
              "colortip_tooltip", "\n " + colortip.desc_color_tooltip, false);
  add_setting((TkuiSetting *)&colortip.color_tooltip_size,
              TKUI_PREF_TYPE_STRING, "colortip_tooltip_size",
              "   " + colortip.desc_color_tooltip_size, false);
  add_setting((TkuiSetting *)&colortip.color_chooser, TKUI_PREF_TYPE_BOOLEAN,
              "colortip_chooser", "   " + colortip.desc_color_chooser, false);

  // MarkWord
  add_setting((TkuiSetting *)&markword.enable, TKUI_PREF_TYPE_BOOLEAN,
              "markword_enable", "\n " + markword.desc_enable, false);
  add_setting((TkuiSetting *)&markword.single_click_deselect,
              TKUI_PREF_TYPE_BOOLEAN, "markword_single_click_deselect",
              "   " + markword.desc_single_click_deselect, false);

  // ColumnMarker
  add_setting((TkuiSetting *)&column_markers.enable, TKUI_PREF_TYPE_BOOLEAN,
              "column_marker_enable", "\n " + column_markers.desc_enable,
              false);

  add_setting((TkuiSetting *)&column_markers.str_columns, TKUI_PREF_TYPE_STRING,
              "column_marker_columns", {}, false);
  add_setting((TkuiSetting *)&column_markers.str_colors, TKUI_PREF_TYPE_STRING,
              "column_marker_colors", {}, false);
}

void TweakUiSettings::load() {
  if (!keyfile) {
    kf_open();
  }

  g_key_file_load_from_file(keyfile, config_file.c_str(),
                            GKeyFileFlags(G_KEY_FILE_KEEP_COMMENTS), nullptr);

  if (!g_key_file_has_group(keyfile, TKUI_KF_GROUP)) {
    return;
  }

  for (auto pref : pref_entries) {
    switch (pref.type) {
      case TKUI_PREF_TYPE_BOOLEAN:
        if (kf_has_key(pref.name)) {
          *(bool *)pref.setting = g_key_file_get_boolean(
              keyfile, TKUI_KF_GROUP, pref.name.c_str(), nullptr);
        }
        break;
      case TKUI_PREF_TYPE_INTEGER:
        if (kf_has_key(pref.name)) {
          *(int *)pref.setting = g_key_file_get_integer(
              keyfile, TKUI_KF_GROUP, pref.name.c_str(), nullptr);
        }
        break;
      case TKUI_PREF_TYPE_DOUBLE:
        if (kf_has_key(pref.name)) {
          *(double *)pref.setting = g_key_file_get_double(
              keyfile, TKUI_KF_GROUP, pref.name.c_str(), nullptr);
        }
        break;
      case TKUI_PREF_TYPE_STRING:
        if (kf_has_key(pref.name)) {
          char *val = g_key_file_get_string(keyfile, TKUI_KF_GROUP,
                                            pref.name.c_str(), nullptr);
          *(std::string *)pref.setting = cstr_assign(val);
        }
        break;
      default:
        break;
    }
  }

  // prevent auto size and window geometry conflict
  if (sidebar_auto_position.enable) {
    window_geometry.sidebar_enable = false;
    window_geometry.sidebar_maximized = -1;
    window_geometry.sidebar_normal = -1;
  }
}

void TweakUiSettings::save_session() { save(true); }

void TweakUiSettings::save(bool bSession) {
  // Load old contents in case user changed file outside of GUI
  g_key_file_load_from_file(keyfile, config_file.c_str(),
                            GKeyFileFlags(G_KEY_FILE_KEEP_COMMENTS), nullptr);

  for (auto pref : pref_entries) {
    if (bSession && !pref.session) {
      continue;
    }

    switch (pref.type) {
      case TKUI_PREF_TYPE_BOOLEAN:
        g_key_file_set_boolean(keyfile, TKUI_KF_GROUP, pref.name.c_str(),
                               *reinterpret_cast<bool *>(pref.setting));
        break;
      case TKUI_PREF_TYPE_INTEGER:
        g_key_file_set_integer(keyfile, TKUI_KF_GROUP, pref.name.c_str(),
                               *reinterpret_cast<int *>(pref.setting));
        break;
      case TKUI_PREF_TYPE_DOUBLE:
        g_key_file_set_double(keyfile, TKUI_KF_GROUP, pref.name.c_str(),
                              *reinterpret_cast<double *>(pref.setting));
        break;
      case TKUI_PREF_TYPE_STRING:
        g_key_file_set_string(
            keyfile, TKUI_KF_GROUP, pref.name.c_str(),
            reinterpret_cast<std::string *>(pref.setting)->c_str());
        break;
      default:
        break;
    }
    if (!pref.comment.empty()) {
      g_key_file_set_comment(keyfile, TKUI_KF_GROUP, pref.name.c_str(),
                             (" " + pref.comment).c_str(), nullptr);
    }
  }

  // Store back on disk
  std::string contents =
      cstr_assign(g_key_file_to_data(keyfile, nullptr, nullptr));
  if (!contents.empty()) {
    file_set_contents(config_file, contents);
  }
}

void TweakUiSettings::reset() {
  if (config_file.empty()) {
    kf_open();
  }

  {  // delete if file exists
    GFile *file = g_file_new_for_path(config_file.c_str());
    if (!g_file_trash(file, nullptr, nullptr)) {
      g_file_delete(file, nullptr, nullptr);
    }
    g_object_unref(file);
  }

  TweakUiSettings new_settings;
  new_settings.initialize();
  new_settings.kf_open();
  new_settings.save();
  new_settings.kf_close();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void TweakUiSettings::kf_open() {
  keyfile = g_key_file_new();

  config_file =
      cstr_assign(g_build_filename(geany_data->app->configdir, "plugins",
                                   "tweaks", "tweaks-ui.conf", nullptr));
  std::string conf_dn = g_path_get_dirname(config_file.c_str());
  g_mkdir_with_parents(conf_dn.c_str(), 0755);

  // if file does not exist, create it
  if (!g_file_test(config_file.c_str(), G_FILE_TEST_EXISTS)) {
    file_set_contents(config_file, "[" TKUI_KF_GROUP "]");
    save();
  }
}

void TweakUiSettings::kf_close() {
  if (keyfile) {
    g_key_file_free(keyfile);
    keyfile = nullptr;
  }
}

bool TweakUiSettings::kf_has_key(std::string const &key) {
  return g_key_file_has_key(keyfile, TKUI_KF_GROUP, key.c_str(), nullptr);
}

void TweakUiSettings::add_setting(TkuiSetting *setting,
                                  TkuiSettingType const &type,
                                  std::string const &name,
                                  std::string const &comment,
                                  bool const &session) {
  TkuiSettingPref pref{type, name, comment, session, setting};
  pref_entries.push_back(pref);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
