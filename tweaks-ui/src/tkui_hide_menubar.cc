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

#include <string>

#include "auxiliary.h"
#include "tkui_hide_menubar.h"

void TweakUiHideMenubar::initialize(GeanyKeyGroup *group, gsize key_id) {
  keybinding = keybindings_get_item(group, key_id);
  if (geany_data && geany_data->main_widgets->window) {
    geany_menubar = ui_lookup_widget(
        GTK_WIDGET(geany_data->main_widgets->window), "hbox_menubar");
    update();
  }
}

void TweakUiHideMenubar::update() {
  if (!geany_menubar) {
    return;
  }

  if (hide_on_start) {
    hide();
  } else if (restore_state && !previous_state) {
    hide();
  } else {
    show();
  }
}

void TweakUiHideMenubar::toggle() {
  if (!hide()) {
    show();
  }
}

bool TweakUiHideMenubar::get_state() {
  previous_state = gtk_widget_is_visible(geany_menubar);
  return previous_state;
}

void TweakUiHideMenubar::show() {
  previous_state = true;
  gtk_widget_show(geany_menubar);
}

bool TweakUiHideMenubar::hide() {
  if (gtk_widget_is_visible(geany_menubar)) {
    if (keybinding && keybinding->key != 0) {
      gtk_widget_hide(geany_menubar);
      std::string val =
          cstr_assign(gtk_accelerator_name(keybinding->key, keybinding->mods));
      msgwin_status_add(_("Menubar has been hidden.  To reshow it, use: %s"),
                        val.c_str());
      previous_state = false;
      return true;
    } else {
      msgwin_status_add(
          _("Menubar will not be hidden until after a keybinding "
            "to reshow it has been set."));
      return false;
    }
  }
  return false;
}
