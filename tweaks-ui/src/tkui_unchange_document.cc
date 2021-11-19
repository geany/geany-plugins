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

#include "tkui_unchange_document.h"

void TweakUiUnchangeDocument::initialize() {
  GEANY_PSC("editor-notify", editor_notify, this);
}

bool TweakUiUnchangeDocument::editor_notify(GObject *object,
                                            GeanyEditor *editor,
                                            SCNotification *nt,
                                            TweakUiUnchangeDocument *self) {
  self->unchange(editor->document);
  return false;
}

void TweakUiUnchangeDocument::document_signal(GObject *obj, GeanyDocument *doc,
                                              TweakUiUnchangeDocument *self) {
  self->unchange(doc);
}

void TweakUiUnchangeDocument::unchange(GeanyDocument *doc) {
  if (!enable || !DOC_VALID(doc) || !doc->changed) {
    return;
  }
  if (sci_get_length(doc->editor->sci) == 0) {
    document_set_text_changed(doc, false);
  }
}
