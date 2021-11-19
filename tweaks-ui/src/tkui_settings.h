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

#pragma once

#include <vector>

#include "tkui_auto_read_only.h"
#include "tkui_colortip.h"
#include "tkui_column_markers.h"
#include "tkui_detect_filetype.h"
#include "tkui_hide_menubar.h"
#include "tkui_main.h"
#include "tkui_markword.h"
#include "tkui_sidebar_auto_position.h"
#include "tkui_unchange_document.h"
#include "tkui_window_geometry.h"

#define TKUI_KF_GROUP "tweaks"

union TkuiSetting {
  bool tkuiBoolean;
  int tkuiInteger;
  double tkuiDouble;
  std::string tkuiString;
};

enum TkuiSettingType {
  TKUI_PREF_TYPE_BOOLEAN,
  TKUI_PREF_TYPE_INTEGER,
  TKUI_PREF_TYPE_DOUBLE,
  TKUI_PREF_TYPE_STRING,
};

class TkuiSettingPref {
 public:
  TkuiSettingType type;
  std::string name;
  std::string comment;
  bool session;
  TkuiSetting *setting;
};

class TweakUiSettings {
 public:
  void initialize();

  void load();
  void save(bool bSession = false);
  void save_session();
  void reset();
  void kf_open();
  void kf_close();

 public:
  std::string config_file;

  TweakUiAutoReadOnly auto_read_only;
  TweakUiColumnMarkers column_markers;
  TweakUiHideMenubar hide_menubar;
  TweakUiSidebarAutoPosition sidebar_auto_position;
  TweakUiUnchangeDocument unchange_document;
  TweakUiWindowGeometry window_geometry;

  TweakUiDetectFileType detect_filetype_on_reload;

  TweakUiColorTip colortip;
  TweakUiMarkWord markword;

 private:
  bool kf_has_key(std::string const &key);

  void add_setting(TkuiSetting *setting, TkuiSettingType const &type,
                   std::string const &name, std::string const &comment,
                   bool const &session);

 private:
  GKeyFile *keyfile = nullptr;
  std::vector<TkuiSettingPref> pref_entries;
};
