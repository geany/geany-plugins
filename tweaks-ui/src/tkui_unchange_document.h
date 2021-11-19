/*
 * Unchange Document - Tweaks-UI Plugin for Geany
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

#define DOUBLE_CLICK_DELAY 50

class TweakUiUnchangeDocument {
 public:
  void initialize();
  void unchange(GeanyDocument *doc);

  static bool editor_notify(GObject *object, GeanyEditor *editor,
                            SCNotification *nt, TweakUiUnchangeDocument *self);
  static void document_signal(GObject *obj, GeanyDocument *doc,
                              TweakUiUnchangeDocument *self);

 public:
  std::string desc_enable =
      _("UnchangeDocument: Set change state to false for unsaved, "
        "empty documents.");
  bool enable = false;

 private:
};
