/*
 * ColorTip - Tweaks-UI Plugin for Geany
 * Copyright 2021 xiota
 *
 * AoColorTip - part of Addons, a Geany plugin
 * Copyright 2017 LarsGit223
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

#include "tkui_addons.h"

class TweakUiColorTip {
 public:
  void initialize();
  void setSize(std::string strSize = "");

 public:
  std::string desc_color_tooltip =
      _("ColorTip: Show hex colors in tooltips when the mouse hovers over them.");

  /// The first letter is significant.  (S)mall, (M)edium, (L)arge.
  std::string desc_color_tooltip_size =
      _("Tooltip size: small, medium, large.");

  std::string desc_color_chooser =
      _("Start the color chooser when double clicking a color value.");
  bool color_tooltip = false;
  bool color_chooser = false;
  std::string color_tooltip_size{"small"};

 private:
  static void document_signal(GObject *obj, GeanyDocument *doc,
                              TweakUiColorTip *self);

  static void document_close(GObject *obj, GeanyDocument *doc,
                             TweakUiColorTip *self);

  static bool editor_notify(GObject *object, GeanyEditor *editor,
                            SCNotification *nt, TweakUiColorTip *self);

  static int contains_color_value(char *string, int position, int maxdist);
  static int get_color_value_at_current_doc_position();
  static gboolean on_editor_button_press_event(GtkWidget *widget,
                                               GdkEventButton *event,
                                               TweakUiColorTip *self);

  void connect_document_button_press_signal_handler(GeanyDocument *doc);

 private:
  std::string colortip_template{"    "};
};
