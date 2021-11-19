/*
 * Window Geometry - Tweaks-UI Plugin for Geany
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

#include "tkui_addons.h"

class TweakUiWindowGeometry {
 public:
  void initialize();
  void setEnabled(bool const val);
  void disconnect();
  void restore_geometry();

 public:
  std::string description =
      _("WindowGeometry: Save and restore sidebar and message window "
        "positions.  This feature is intended to be used with "
        "\"Save Window Size\" and \"Save Window Position\" enabled.");
  bool enable = false;
  std::string desc_geometry_update =
      _("Update positions when they are changed.  "
        "When false, the previously saved settings will be used.");
  bool geometry_update = false;
  std::string desc_sidebar_enable = _("Save sidebar position.");
  bool sidebar_enable = false;
  std::string desc_msgwin_enable = _("Save message window position.");
  bool msgwin_enable = false;

  bool maximized = false;

  int xpos = -1;
  int ypos = -1;
  int width = -1;
  int height = -1;

  int xpos_maximized = -1;
  int ypos_maximized = -1;
  int width_maximized = -1;
  int height_maximized = -1;

  int sidebar_maximized = -1;
  int sidebar_normal = -1;
  int msgwin_maximized = -1;
  int msgwin_normal = -1;

 private:
  void connect(bool enable);
  void update_geometry();
  static gboolean geometry_callback(GtkWidget *hpane, cairo_t *cr,
                                    gpointer user_data);

 private:
  bool bRestoreInProgress = false;
  ulong ulHandleGeometry = false;
  GtkWidget *geany_window = nullptr;
  GtkWidget *geany_hpane = nullptr;
  GtkWidget *geany_vpane = nullptr;
  GtkWidget *geany_sidebar = nullptr;
  GtkWidget *geany_msgwin = nullptr;
};
