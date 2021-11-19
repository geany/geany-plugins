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

#pragma once

#include <geanyplugin.h>

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;

extern GtkWidget *find_focus_widget(GtkWidget *widget);
extern GeanyKeyGroup *keybindings_get_core_group(guint id);

enum TweakUiShortcuts {
  TKUI_KEY_TOGGLE_MENUBAR_VISIBILITY,
  TKUI_KEY_COPY_1,
  TKUI_KEY_COPY_2,
  TKUI_KEY_PASTE_1,
  TKUI_KEY_PASTE_2,
  TKUI_KEY_TOGGLE_READONLY,

  TKUI_KEY_REDETECT_FILETYPE,
  TKUI_KEY_REDETECT_FILETYPE_FORCE,

  TKUI_KEY_COUNT,
};
