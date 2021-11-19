/*
 * MarkWord - Tweaks-UI Plugin for Geany
 * Copyright 2021 xiota
 *
 * AoMarkWord - part of Addons, a Geany plugin
 * Copyright 2009-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

class TweakUiMarkWord {
 public:
  void initialize();

 public:
  std::string desc_enable =
      _("MarkWord: Mark all occurrences of a word when double-clicking it.");
  std::string desc_single_click_deselect =
      _("Deselect the previous highlight by single click");
  bool enable = false;
  bool single_click_deselect = true;

 private:
  static void document_signal(GObject *obj, GeanyDocument *doc,
                              TweakUiMarkWord *self);

  static void document_close(GObject *obj, GeanyDocument *doc,
                             TweakUiMarkWord *self);

  static bool editor_notify(GObject *object, GeanyEditor *editor,
                            SCNotification *nt, TweakUiMarkWord *self);

  static void clear_marker(GeanyDocument *doc = nullptr);
  static gboolean mark_word(TweakUiMarkWord *self);
  static gboolean on_editor_button_press_event(GtkWidget *widget,
                                               GdkEventButton *event,
                                               TweakUiMarkWord *self);

  void connect_document_button_press_signal_handler(GeanyDocument *doc);

 private:
  gulong double_click_timer_id = 0;
};
