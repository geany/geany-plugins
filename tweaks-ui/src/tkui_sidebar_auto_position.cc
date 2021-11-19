/*
 * Sidebar Auto Position - Tweaks-UI Plugin for Geany
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

#include <string>

#include "tkui_sidebar_auto_position.h"

void TweakUiSidebarAutoPosition::initialize() {
  if (geany_data && geany_data->main_widgets) {
    geany_window = geany_data->main_widgets->window;
    geany_sidebar = geany_data->main_widgets->sidebar_notebook;
    geany_hpane = ui_lookup_widget(GTK_WIDGET(geany_window), "hpaned1");
    connect(enable);

    bPanedLeft = geany_sidebar == gtk_paned_get_child1(GTK_PANED(geany_hpane));
  }

  update_position();
}

void TweakUiSidebarAutoPosition::update_position() {
  GeanyDocument *doc = document_get_current();
  if (DOC_VALID(doc)) {
    std::string str_auto_maximized(columns_maximized, '0');
    std::string str_auto_normal(columns_normal, '0');

    int pos_origin = (int)scintilla_send_message(doc->editor->sci,
                                                 SCI_POINTXFROMPOSITION, 0, 1);
    int pos_maximized = (int)scintilla_send_message(
        doc->editor->sci, SCI_TEXTWIDTH, 0, (gulong)str_auto_maximized.c_str());
    int pos_normal = (int)scintilla_send_message(
        doc->editor->sci, SCI_TEXTWIDTH, 0, (gulong)str_auto_normal.c_str());

    position_maximized = pos_origin + pos_maximized;
    position_normal = pos_origin + pos_normal;
  }
}

void TweakUiSidebarAutoPosition::setEnabled(bool const val) {
  enable = val;
  connect(enable);
}

void TweakUiSidebarAutoPosition::disconnect() { connect(false); }

void TweakUiSidebarAutoPosition::connect(bool val) {
  if (val && geany_hpane && !ulHandlePanePosition) {
    ulHandlePanePosition = g_signal_connect_after(
        GTK_WIDGET(geany_hpane), "draw", G_CALLBACK(hpane_callback), this);
  }

  if (!val && geany_hpane && ulHandlePanePosition) {
    g_clear_signal_handler(&ulHandlePanePosition, GTK_WIDGET(geany_hpane));
  }
}

gboolean TweakUiSidebarAutoPosition::hpane_callback(GtkWidget *hpane,
                                                    cairo_t *cr,
                                                    gpointer user_data) {
  TweakUiSidebarAutoPosition *self = (TweakUiSidebarAutoPosition *)user_data;
  if (!self->enable) {
    self->connect(self->enable);
    return false;
  }

  self->update_hpane();
  return false;
}

void TweakUiSidebarAutoPosition::update_hpane() {
  if (!geany_hpane || !geany_window) {
    enable = false;
    connect(enable);
    return;
  }

  if (position_maximized < 0 || position_normal < 0) {
    update_position();
  }

  GdkWindowState wstate =
      gdk_window_get_state(gtk_widget_get_window(geany_window));

  bool fullscreen = wstate & GDK_WINDOW_STATE_FULLSCREEN;
  bool maximized_current = wstate & GDK_WINDOW_STATE_MAXIMIZED;

  static bool maximized_previous = !maximized_current;

  // doesn't work well with full screen
  if (fullscreen) {
    return;
  }

  if (maximized_current != maximized_previous) {
    int width, height;
    gtk_window_get_size(GTK_WINDOW(geany_window), &width, &height);
    bPanedLeft = geany_sidebar == gtk_paned_get_child1(GTK_PANED(geany_hpane));

    if (maximized_current) {
      if (0 <= position_maximized) {
        if (!bPanedLeft) {
          gtk_paned_set_position(GTK_PANED(geany_hpane), position_maximized);
        } else if (position_maximized <= width) {
          gtk_paned_set_position(GTK_PANED(geany_hpane),
                                 width - position_maximized);
        }
      }
    } else {
      if (0 <= position_normal) {
        if (!bPanedLeft) {
          gtk_paned_set_position(GTK_PANED(geany_hpane), position_normal);
        } else if (position_normal <= width) {
          gtk_paned_set_position(GTK_PANED(geany_hpane),
                                 width - position_normal);
        }
      }
    }
    maximized_previous = maximized_current;
  }
}
