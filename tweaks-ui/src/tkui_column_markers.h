/*
 * Column Markers - Tweaks-UI Plugin for Geany
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

#include <string>
#include <vector>

#include "tkui_addons.h"

class TweakUiColumnMarkers {
 public:
  TweakUiColumnMarkers() = default;

  void initialize();
  void show(GeanyDocument *doc = nullptr);
  void show_idle();
  void clear_columns();
  void add_column(int nColumn, int nColor);
  void add_column(std::string strColumn, std::string strColor);
  void get_columns(std::string &strColumns, std::string &strColors);
  void update_columns();

  std::pair<std::string, std::string> get_columns();

 public:
  std::string desc_enable =
      _("ColumnMarkers: Show column markers at the specified columns.  "
        "Colors are in #RRGGBB or #RGB formats.");
  bool enable = false;
  std::string str_columns{"60;72;80;88;96;104;112;120;128;136;144;152;160;"};
  std::string str_colors{
      "#e5e5e5;#b0d0ff;#ffc0ff;#e5e5e5;#ffb0a0;#e5e5e5;#e5e5e5;"
      "#e5e5e5;#e5e5e5;#e5e5e5;#e5e5e5;#e5e5e5;#e5e5e5;"};

 private:
  static gboolean show_idle_callback(TweakUiColumnMarkers *self);

  static void document_signal(GObject *obj, GeanyDocument *doc,
                              TweakUiColumnMarkers *self);

 private:
  bool bHandleShowIdleInProgress = false;
  std::vector<int> vn_columns;
  std::vector<int> vn_colors;
};
