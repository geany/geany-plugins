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

#include "auxiliary.h"
#include "tkui_colortip.h"

void TweakUiColorTip::initialize() {
  // connect the button-press event for all open documents
  if (color_chooser && main_is_realized()) {
    uint i = 0;
    foreach_document(i) {
      connect_document_button_press_signal_handler(documents[i]);
    }
  }

  GEANY_PSC_AFTER("document-open", document_signal, this);
  GEANY_PSC_AFTER("document-new", document_signal, this);
  GEANY_PSC("document-close", document_close, this);
  GEANY_PSC("editor-notify", editor_notify, this);

  setSize();
}

/* Find a color value (short or long form, e.g. #fff or #ffffff) in the
 * given string. position must be inside the found color or at maximum
 * maxdist bytes in front of it (in front of the start of the color value)
 * or behind it (behind the end of the color value). */
int TweakUiColorTip::contains_color_value(char *string, int position,
                                          int maxdist) {
  int color = -1;

  char *start = strchr(string, '#');
  if (!start) {
    start = strstr(string, "0x");
    if (start) {
      start += 1;
    }
  }
  if (start == nullptr) {
    return color;
  }
  int end = start - string + 1;
  while (g_ascii_isxdigit(string[end])) {
    end++;
  }
  end--;
  uint length = &(string[end]) - start + 1;

  if (maxdist != -1) {
    int offset = start - string + 1;
    if (position < offset && (offset - position) > maxdist) {
      return color;
    }

    offset = end;
    if (position > offset && (position - offset) > maxdist) {
      return color;
    }
  }

  if (length == 4) {
    start++;
    color = (g_ascii_xdigit_value(*start) << 4) | g_ascii_xdigit_value(*start);

    start++;
    int value =
        (g_ascii_xdigit_value(*start) << 4) | g_ascii_xdigit_value(*start);
    color += value << 8;

    start++;
    value = (g_ascii_xdigit_value(*start) << 4) | g_ascii_xdigit_value(*start);
    color += value << 16;
  } else if (length == 7) {
    start++;
    color =
        (g_ascii_xdigit_value(start[0]) << 4) | g_ascii_xdigit_value(start[1]);

    start += 2;
    int value =
        (g_ascii_xdigit_value(start[0]) << 4) | g_ascii_xdigit_value(start[1]);
    color += value << 8;

    start += 2;
    value =
        (g_ascii_xdigit_value(start[0]) << 4) | g_ascii_xdigit_value(start[1]);
    color += value << 16;
  }

  return color;
}

int TweakUiColorTip::get_color_value_at_current_doc_position() {
  GeanyDocument *doc = document_get_current();
  g_return_val_if_fail(DOC_VALID(doc), -1);

  int color = -1;
  char *word =
      editor_get_word_at_pos(doc->editor, -1, "0123456789abcdefABCDEF");

  if (word) {
    switch (strlen(word)) {
      case 3:
        color = ((g_ascii_xdigit_value(word[0]) * 0x11) << 16 |
                 (g_ascii_xdigit_value(word[1]) * 0x11) << 8 |
                 (g_ascii_xdigit_value(word[2]) * 0x11) << 0);
        break;
      case 6:
        color = (g_ascii_xdigit_value(word[0]) << 20 |
                 g_ascii_xdigit_value(word[1]) << 16 |
                 g_ascii_xdigit_value(word[2]) << 12 |
                 g_ascii_xdigit_value(word[3]) << 8 |
                 g_ascii_xdigit_value(word[4]) << 4 |
                 g_ascii_xdigit_value(word[5]) << 0);
        break;
      default:
        // invalid color or other format
        break;
    }
  }

  return color;
}

gboolean TweakUiColorTip::on_editor_button_press_event(GtkWidget *widget,
                                                       GdkEventButton *event,
                                                       TweakUiColorTip *self) {
  if (event->button == 1) {
    if (event->type == GDK_2BUTTON_PRESS) {
      if (!self->color_chooser) {
        return false;
      }
      if (get_color_value_at_current_doc_position() != -1) {
        keybindings_send_command(GEANY_KEY_GROUP_TOOLS,
                                 GEANY_KEYS_TOOLS_OPENCOLORCHOOSER);
      }
    }
  }
  return false;
}

void TweakUiColorTip::setSize(std::string strSize) {
  to_lower_inplace(trim_inplace(strSize));
  if (!strSize.empty()) {
    color_tooltip_size = strSize;
  }

  switch (color_tooltip_size[0]) {
    case 'l':
      colortip_template = "          \n          \n          \n          ";
      break;
    case 'm':
      colortip_template = "        \n        ";
      break;
    case 's':
    default:
      colortip_template = "    ";
      break;
  }
}

void TweakUiColorTip::connect_document_button_press_signal_handler(
    GeanyDocument *doc) {
  g_return_if_fail(DOC_VALID(doc));

  plugin_signal_connect(geany_plugin, G_OBJECT(doc->editor->sci),
                        "button-press-event", false,
                        G_CALLBACK(on_editor_button_press_event), this);
}

void TweakUiColorTip::document_signal(GObject *obj, GeanyDocument *doc,
                                      TweakUiColorTip *self) {
  g_return_if_fail(DOC_VALID(doc));
  self->connect_document_button_press_signal_handler(doc);
  scintilla_send_message(doc->editor->sci, SCI_SETMOUSEDWELLTIME, 300, 0);
}

void TweakUiColorTip::document_close(GObject *obj, GeanyDocument *doc,
                                     TweakUiColorTip *self) {
  g_return_if_fail(DOC_VALID(doc));

  g_signal_handlers_disconnect_by_func(
      doc->editor->sci, gpointer(on_editor_button_press_event), self);
}

bool TweakUiColorTip::editor_notify(GObject *object, GeanyEditor *editor,
                                    SCNotification *nt, TweakUiColorTip *self) {
  ScintillaObject *sci = editor->sci;

  // Ignore all events if color tips are disabled in preferences
  if (!self->color_tooltip) {
    return false;
  }

  switch (nt->nmhdr.code) {
    case SCN_DWELLSTART: {
      char *subtext;
      int start, end, pos, max;

      // Is position valid?
      if (nt->position < 0) {
        break;
      }
      // Calculate range
      start = nt->position;
      if (start >= 7) {
        start -= 7;
      } else {
        start = 0;
      }
      pos = nt->position - start;
      end = nt->position + 7;
      max = scintilla_send_message(sci, SCI_GETTEXTLENGTH, 0, 0);
      if (end > max) {
        end = max;
      }

      // Get text in range and examine it
      subtext = sci_get_contents_range(sci, start, end);
      if (subtext != nullptr) {
        int color = contains_color_value(subtext, pos, 2);
        if (color != -1) {
          scintilla_send_message(sci, SCI_CALLTIPSETBACK, color, 0);
          scintilla_send_message(sci, SCI_CALLTIPSHOW, nt->position,
                                 (sptr_t)self->colortip_template.c_str());
        }
        g_free(subtext);
      }
    } break;

    case SCN_DWELLEND:
      scintilla_send_message(sci, SCI_CALLTIPCANCEL, 0, 0);
      break;
  }
  return false;
}
