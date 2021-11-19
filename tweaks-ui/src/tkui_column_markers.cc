/*
 * Column Markers - Tweaks-UI Plugin for Geany
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

#include "auxiliary.h"
#include "tkui_column_markers.h"

void TweakUiColumnMarkers::initialize() {
  if (enable && main_is_realized()) {
    guint i = 0;
    foreach_document(i) { show(documents[i]); }
  }

  GEANY_PSC_AFTER("document-activate", document_signal, this);
  GEANY_PSC_AFTER("document-new", document_signal, this);
  GEANY_PSC_AFTER("document-open", document_signal, this);
  GEANY_PSC_AFTER("document-reload", document_signal, this);
}

void TweakUiColumnMarkers::document_signal(GObject *obj, GeanyDocument *doc,
                                           TweakUiColumnMarkers *self) {
  self->show_idle();
}

gboolean TweakUiColumnMarkers::show_idle_callback(TweakUiColumnMarkers *self) {
  self->show();
  self->bHandleShowIdleInProgress = false;
  return false;
}

void TweakUiColumnMarkers::show_idle() {
  if (!bHandleShowIdleInProgress) {
    bHandleShowIdleInProgress = true;
    g_idle_add(G_SOURCE_FUNC(show_idle_callback), this);
  }
}

void TweakUiColumnMarkers::clear_columns() {
  vn_columns.clear();
  vn_colors.clear();
}

void TweakUiColumnMarkers::add_column(int nColumn, int nColor) {
  vn_columns.push_back(nColumn);
  vn_colors.push_back(nColor);
}

void TweakUiColumnMarkers::show(GeanyDocument *doc) {
  if (!enable) {
    return;
  }

  if (!DOC_VALID(doc)) {
    doc = document_get_current();
    g_return_if_fail(DOC_VALID(doc));
  }

  scintilla_send_message(doc->editor->sci, SCI_SETEDGEMODE, 3, 3);
  scintilla_send_message(doc->editor->sci, SCI_MULTIEDGECLEARALL, 0, 0);

  if (!vn_columns.empty() && !vn_colors.empty()) {
    const int cnt_col = vn_columns.size();
    const int cnt_clr = vn_colors.size();
    const int count = cnt_col > cnt_clr ? cnt_clr : cnt_col;

    for (int i = 0; i < count; i++) {
      scintilla_send_message(doc->editor->sci, SCI_MULTIEDGEADDLINE,
                             vn_columns[i], vn_colors[i]);
    }
  }
}

void TweakUiColumnMarkers::get_columns(std::string &strColumns,
                                       std::string &strColors) {
  std::vector<std::string> vs_columns;
  for (auto x : vn_columns) {
    vs_columns.push_back(std::to_string(x));
  }

  std::vector<std::string> vs_colors;
  for (auto x : vn_colors) {
    char cstr[16];
    std::string strtmp;
    strtmp = cstr_assign(g_strdup_printf("%x", x));
    cstr[0] = '#';
    cstr[1] = strtmp[4];
    cstr[2] = strtmp[5];
    cstr[3] = strtmp[2];
    cstr[4] = strtmp[3];
    cstr[5] = strtmp[0];
    cstr[6] = strtmp[1];
    cstr[7] = '\0';
    vs_colors.push_back(cstr);
  }

  for (auto x : vs_columns) {
    strColumns += x + ";";
  }

  for (auto x : vs_colors) {
    strColors += x + ";";
  }
}

void TweakUiColumnMarkers::add_column(std::string strColumn,
                                      std::string strColor) {
  char *ptr = nullptr;
  char strTmp[16];
  ulong color_val = 0;

  if (strColor[0] == '0' && strColor[1] == 'x') {
    color_val = strtoul(strColor.c_str(), &ptr, 0);
  } else if (strColor[0] == '#' && strColor.length() == 7) {
    // convert from RGB to BGR format
    strTmp[0] = '0';
    strTmp[1] = 'x';
    strTmp[2] = strColor[5];
    strTmp[3] = strColor[6];
    strTmp[4] = strColor[3];
    strTmp[5] = strColor[4];
    strTmp[6] = strColor[1];
    strTmp[7] = strColor[2];
    strTmp[8] = '\0';
    color_val = strtoul(strTmp, &ptr, 0);
  } else if (strColor[0] == '#' && strColor.length() == 4) {
    strTmp[0] = '0';
    strTmp[1] = 'x';
    strTmp[2] = strColor[3];
    strTmp[3] = strColor[3];
    strTmp[4] = strColor[2];
    strTmp[5] = strColor[2];
    strTmp[6] = strColor[1];
    strTmp[7] = strColor[1];
    strTmp[8] = '\0';
  } else {
    color_val = atoi(strColor.c_str());
  }
  const int column_val = atoi(strColumn.c_str());

  add_column(column_val, color_val);
}

void TweakUiColumnMarkers::update_columns() {
  clear_columns();

  auto vs_columns = split_string(str_columns, ";");
  auto vs_colors = split_string(str_colors, ";");

  const int len_a = vs_columns.size();
  const int len_b = vs_colors.size();
  const int len_c = len_a < len_b ? len_a : len_b;

  for (int i = 0; i < len_c; ++i) {
    if (!vs_columns[i].empty() && !vs_colors[i].empty()) {
      add_column(vs_columns[i], vs_colors[i]);
    }
  }

  show();
}
