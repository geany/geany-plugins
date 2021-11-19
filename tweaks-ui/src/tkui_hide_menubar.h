/*
 * Hide Menubar - Tweaks-UI Plugin for Geany
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

class TweakUiHideMenubar {
 public:
  void initialize(GeanyKeyGroup *group, gsize key_id);
  bool hide();
  void show();
  void toggle();
  void update();
  bool get_state();

 public:
  std::string desc_hide_on_start =
      _("HideMenubar: Hide the menubar on start.  "
        "The menubar will not be hidden until after a keybinding is set.");
  std::string desc_restore_state =
      _("Restore the menubar state from when Geany was last shut down.");
  bool hide_on_start = false;
  bool restore_state = false;
  std::string desc_previous_state =
      _("The menubar state when Geany was last shut down.");
  bool previous_state = true;

 private:
  GtkWidget *geany_menubar = nullptr;
  GeanyKeyBinding *keybinding = nullptr;
};
