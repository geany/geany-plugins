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

#include "tkui_markword.h"

void TweakUiMarkWord::initialize() {
  // connect the button-press event for all open documents
  if (enable && main_is_realized()) {
    guint i = 0;
    foreach_document(i) {
      connect_document_button_press_signal_handler(documents[i]);
    }
  }

  GEANY_PSC_AFTER("document-open", document_signal, this);
  GEANY_PSC_AFTER("document-new", document_signal, this);
  GEANY_PSC("document-close", document_close, this);
  GEANY_PSC("editor-notify", editor_notify, this);
}

void TweakUiMarkWord::clear_marker(GeanyDocument *doc) {
  if (!DOC_VALID(doc)) {
    doc = document_get_current();
  }
  // clear current search indicators
  if (DOC_VALID(doc)) {
    editor_indicator_clear(doc->editor, GEANY_INDICATOR_SEARCH);
  }
}

gboolean TweakUiMarkWord::mark_word(TweakUiMarkWord *self) {
  keybindings_send_command(GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_MARKALL);
  // unset and remove myself
  self->double_click_timer_id = 0;
  return false;
}

gboolean TweakUiMarkWord::on_editor_button_press_event(GtkWidget *widget,
                                                       GdkEventButton *event,
                                                       TweakUiMarkWord *self) {
  if (event->button == 1) {
    if (!self->enable) {
      return false;
    }
    if (event->type == GDK_BUTTON_PRESS) {
      if (self->single_click_deselect) {
        clear_marker();
      }
    } else if (event->type == GDK_2BUTTON_PRESS) {
      if (self->double_click_timer_id == 0) {
        self->double_click_timer_id =
            g_timeout_add(DOUBLE_CLICK_DELAY, G_SOURCE_FUNC(mark_word), self);
      }
    }
  }
  return false;
}

void TweakUiMarkWord::connect_document_button_press_signal_handler(
    GeanyDocument *doc) {
  g_return_if_fail(DOC_VALID(doc));

  plugin_signal_connect(geany_plugin, G_OBJECT(doc->editor->sci),
                        "button-press-event", false,
                        G_CALLBACK(on_editor_button_press_event), this);
}

void TweakUiMarkWord::document_signal(GObject *obj, GeanyDocument *doc,
                                      TweakUiMarkWord *self) {
  self->connect_document_button_press_signal_handler(doc);
}

void TweakUiMarkWord::document_close(GObject *obj, GeanyDocument *doc,
                                     TweakUiMarkWord *self) {
  g_return_if_fail(DOC_VALID(doc));

  g_signal_handlers_disconnect_by_func(
      doc->editor->sci, gpointer(on_editor_button_press_event), self);
}

bool TweakUiMarkWord::editor_notify(GObject *object, GeanyEditor *editor,
                                    SCNotification *nt, TweakUiMarkWord *self) {
  // If something is about to be deleted and
  // there is selected text clear the markers
  if (nt->nmhdr.code == SCN_MODIFIED &&
      ((nt->modificationType & SC_MOD_BEFOREDELETE) == SC_MOD_BEFOREDELETE) &&
      sci_has_selection(editor->sci)) {
    if (self->enable && self->single_click_deselect) {
      clear_marker(editor->document);
    }
  }
  // In single click deselect mode, clear the markers when the cursor moves
  else if (nt->nmhdr.code == SCN_UPDATEUI &&
           nt->updated == SC_UPDATE_SELECTION &&
           !sci_has_selection(editor->sci)) {
    if (self->enable && self->single_click_deselect) {
      clear_marker(editor->document);
    }
  }
  return false;
}
