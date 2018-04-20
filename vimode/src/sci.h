/*
 * Copyright 2018 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __VIMODE_SCI_H__
#define __VIMODE_SCI_H__

#include <gtk/gtk.h>
#include "Scintilla.h"
#include "ScintillaWidget.h"

#define SSM(s, m, w, l) scintilla_send_message((s), (m), (w), (l))

#define NEXT(s, pos) scintilla_send_message((s), SCI_POSITIONAFTER, (pos), 0)
#define PREV(s, pos) scintilla_send_message((s), SCI_POSITIONBEFORE, (pos), 0)
#define NTH(s, pos, rel) scintilla_send_message((s), SCI_POSITIONRELATIVE, (pos), (rel))
#define DIFF(s, start, end) scintilla_send_message((s), SCI_COUNTCHARACTERS, (start), (end))

#define SET_POS(s, pos, scr) _set_current_position((s), (pos), (scr), TRUE)
#define SET_POS_NOX(s, pos, scr) _set_current_position((s), (pos), (scr), FALSE)
#define GET_CUR_LINE(s) scintilla_send_message((s), SCI_LINEFROMPOSITION, \
	SSM((s), SCI_GETCURRENTPOS, 0, 0), 0)

#define MAX_CHAR_SIZE 16

void _set_current_position(ScintillaObject *sci, gint position, gboolean scroll_to_caret,
	gboolean caretx);

#endif
