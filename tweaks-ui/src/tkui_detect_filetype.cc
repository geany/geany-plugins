/*
 * Detect File Type on Reload - Tweaks-UI Plugin for Geany
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

#include "tkui_detect_filetype.h"

void TweakUiDetectFileType::initialize() {
  GEANY_PSC_AFTER("document-reload", document_signal, this);
}

void TweakUiDetectFileType::document_signal(
    GObject *obj, GeanyDocument *doc, TweakUiDetectFileType *self) {
  if (!self->enable || !DOC_VALID(doc) || !doc->file_name) {
    return;
  }

  self->redetect_filetype(doc);
}

void TweakUiDetectFileType::redetect_filetype(GeanyDocument *doc) {
  if (!DOC_VALID(doc)) {
    doc = document_get_current();
  }
  if (!DOC_VALID(doc)) {
    return;
  }

  bool keep_type = false;
  if (doc->file_type->id != GEANY_FILETYPES_NONE) {
    // Don't change filetype if filename matches pattern.
    // Avoids changing *.h files to C after they have been changed to C++.
    char **pattern = doc->file_type->pattern;
    for (int i = 0; pattern[i] != nullptr; ++i) {
      if (g_pattern_match_simple(pattern[i], doc->file_name)) {
        keep_type = true;
        break;
      }
    }
  }

  if (!keep_type) {
    document_set_filetype(doc, filetypes_detect_from_file(doc->file_name));
  }
}

void TweakUiDetectFileType::force_redetect_filetype(
    GeanyDocument *doc) {
  if (!DOC_VALID(doc)) {
    doc = document_get_current();
  }
  if (!DOC_VALID(doc)) {
    return;
  }

  document_set_filetype(doc, filetypes_detect_from_file(doc->file_name));
}
