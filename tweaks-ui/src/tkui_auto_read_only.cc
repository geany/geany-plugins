/*
 * Auto Read Only - Tweaks-UI Plugin for Geany
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

#include "tkui_auto_read_only.h"

void TweakUiAutoReadOnly::initialize() {
  if (geany_data && geany_data->main_widgets->window) {
    read_only_menu_item = GTK_CHECK_MENU_ITEM(ui_lookup_widget(
        GTK_WIDGET(geany_data->main_widgets->window), "set_file_readonly1"));
  }

  // Geany API currently allows changing read-only state only via the menu.
  // The wrong documents are set read only when other document signals are
  // used because of race conditions.
  GEANY_PSC_AFTER("document-activate", document_signal, this);
}

void TweakUiAutoReadOnly::document_signal(GObject* obj, GeanyDocument* doc,
                                          TweakUiAutoReadOnly* self) {
  self->check_read_only(doc);
}

void TweakUiAutoReadOnly::check_read_only(GeanyDocument* doc) {
  if (!enable) {
    return;
  }
  if (!DOC_VALID(doc)) {
    return;
  }

  if (doc->real_path && euidaccess(doc->real_path, W_OK) != 0) {
    set_read_only();
  }
}

void TweakUiAutoReadOnly::set_read_only() {
  if (read_only_menu_item) {
    gtk_check_menu_item_set_active(read_only_menu_item, true);
  }
}

void TweakUiAutoReadOnly::unset_read_only() {
  if (read_only_menu_item) {
    gtk_check_menu_item_set_active(read_only_menu_item, false);
  }
}

void TweakUiAutoReadOnly::toggle() {
  if (read_only_menu_item) {
    gtk_check_menu_item_set_active(
        read_only_menu_item,
        !gtk_check_menu_item_get_active(read_only_menu_item));
  }
}
