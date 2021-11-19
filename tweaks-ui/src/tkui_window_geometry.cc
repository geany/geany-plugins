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

#include "tkui_window_geometry.h"

void TweakUiWindowGeometry::initialize() {
  if (geany_data && geany_data->main_widgets) {
    geany_window = geany_data->main_widgets->window;
    geany_hpane = ui_lookup_widget(GTK_WIDGET(geany_window), "hpaned1");
    geany_vpane = ui_lookup_widget(GTK_WIDGET(geany_window), "vpaned1");

    geany_sidebar = geany_data->main_widgets->sidebar_notebook;
    geany_msgwin = geany_data->main_widgets->message_window_notebook;

    // hack to work around GTK moving pane positions around
    maximized = gtk_window_is_maximized(GTK_WINDOW(geany_window));
    if (maximized) {
      // unmaximize, set geometry/positions
      gtk_window_unmaximize(GTK_WINDOW(geany_window));
      GdkWindow *gdk_window = gtk_widget_get_window(geany_window);
      gdk_window_move_resize(gdk_window, xpos, ypos, width, height);

      if (msgwin_normal >= 0) {
        gtk_paned_set_position(GTK_PANED(geany_vpane), msgwin_normal);
      }
      if (sidebar_normal >= 0) {
        gtk_paned_set_position(GTK_PANED(geany_hpane), sidebar_normal);
      }

      // remaximize, set geometry/positions
      gtk_window_maximize(GTK_WINDOW(geany_window));
      if (msgwin_maximized >= 0) {
        gtk_paned_set_position(GTK_PANED(geany_vpane), msgwin_maximized);
      }
      if (sidebar_maximized >= 0) {
        gtk_paned_set_position(GTK_PANED(geany_hpane), sidebar_maximized);
      }
    } else {
      // normal window, adjust positions
      int cur_width, cur_height;
      gtk_window_get_size(GTK_WINDOW(geany_window), &cur_width, &cur_height);

      if (msgwin_normal >= 0 &&
          gtk_orientable_get_orientation(GTK_ORIENTABLE(geany_hpane)) ==
              GTK_ORIENTATION_HORIZONTAL) {
        gtk_paned_set_position(GTK_PANED(geany_vpane),
                               cur_width - width + msgwin_normal);
      } else {
        gtk_paned_set_position(GTK_PANED(geany_vpane),
                               cur_height - height + msgwin_normal);
      }

      if (sidebar_normal >= 0 &&
          geany_sidebar == gtk_paned_get_child2(GTK_PANED(geany_hpane))) {
        gtk_paned_set_position(GTK_PANED(geany_hpane),
                               cur_width - width + sidebar_normal);
      } else {
        gtk_paned_set_position(GTK_PANED(geany_hpane), sidebar_normal);
      }
      GdkWindow *gdk_window = gtk_widget_get_window(geany_window);
      gdk_window_move_resize(gdk_window, xpos, ypos, width, height);
    }

    connect(enable);
  }
}

void TweakUiWindowGeometry::setEnabled(bool const val) {
  enable = val;
  connect(enable);
}

void TweakUiWindowGeometry::disconnect() { connect(false); }

void TweakUiWindowGeometry::connect(bool val) {
  if (val && geany_hpane && !ulHandleGeometry) {
    ulHandleGeometry = g_signal_connect_after(
        GTK_WIDGET(geany_hpane), "draw", G_CALLBACK(geometry_callback), this);
  }

  if (!val && geany_hpane && ulHandleGeometry) {
    g_clear_signal_handler(&ulHandleGeometry, GTK_WIDGET(geany_hpane));
  }
}

gboolean TweakUiWindowGeometry::geometry_callback(GtkWidget *window,
                                                  cairo_t *cr,
                                                  gpointer user_data) {
  TweakUiWindowGeometry *self = (TweakUiWindowGeometry *)user_data;
  if (!self->enable) {
    self->connect(self->enable);
    return false;
  }

  self->update_geometry();
  return false;
}

void TweakUiWindowGeometry::restore_geometry() {
  if (!enable) {
    connect(enable);
  }

  GdkWindow *gdk_window = gtk_widget_get_window(geany_window);
  GdkWindowState wstate = gdk_window_get_state(gdk_window);

  bool window_maximized_current =
      wstate & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN);

  if (window_maximized_current) {
    if (msgwin_maximized >= 0) {
      gtk_paned_set_position(GTK_PANED(geany_vpane), msgwin_maximized);
    }
    if (sidebar_maximized >= 0) {
      gtk_paned_set_position(GTK_PANED(geany_hpane), sidebar_maximized);
    }
  } else {
    // gtk_window_unmaximize(GTK_WINDOW(geany_window));
    gdk_window_move_resize(gdk_window, xpos, ypos, width, height);

    if (msgwin_normal >= 0) {
      gtk_paned_set_position(GTK_PANED(geany_vpane), msgwin_normal);
    }
    if (sidebar_normal >= 0) {
      gtk_paned_set_position(GTK_PANED(geany_hpane), sidebar_normal);
    }
  }
}

void TweakUiWindowGeometry::update_geometry() {
  if (!enable) {
    connect(enable);
  }

  GdkWindow *gdk_window = gtk_widget_get_window(geany_window);
  GdkWindowState wstate = gdk_window_get_state(gdk_window);
  bool window_maximized_current =
      wstate & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN);

  if (window_maximized_current == maximized) {
    if (geometry_update) {
      if (window_maximized_current) {
        sidebar_maximized = gtk_paned_get_position(GTK_PANED(geany_hpane));
        msgwin_maximized = gtk_paned_get_position(GTK_PANED(geany_vpane));

        gtk_window_get_position(GTK_WINDOW(geany_window), &xpos_maximized,
                                &ypos_maximized);
        gtk_window_get_size(GTK_WINDOW(geany_window), &width_maximized,
                            &height_maximized);
      } else {
        sidebar_normal = gtk_paned_get_position(GTK_PANED(geany_hpane));
        msgwin_normal = gtk_paned_get_position(GTK_PANED(geany_vpane));

        gtk_window_get_position(GTK_WINDOW(geany_window), &xpos, &ypos);
        gtk_window_get_size(GTK_WINDOW(geany_window), &width, &height);
      }
    }
  } else {
    restore_geometry();

    maximized = window_maximized_current;
  }
}
