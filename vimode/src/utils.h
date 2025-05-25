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

#ifndef __VIMODE_UTILS_H__
#define __VIMODE_UTILS_H__

#include "sci.h"

gchar *get_current_word(ScintillaObject *sci);

void clamp_cursor_pos(ScintillaObject *sci);
void goto_nonempty(ScintillaObject *sci, intptr_t line, gboolean scroll);

intptr_t perform_search(ScintillaObject *sci, const gchar *search_text,
	gint num, gboolean invert);
void perform_substitute(ScintillaObject *sci, const gchar *cmd, intptr_t from, intptr_t to,
	const gchar *flag_override);

intptr_t get_line_number_rel(ScintillaObject *sci, intptr_t shift);
void ensure_current_line_expanded(ScintillaObject *sci);

intptr_t jump_to_expended_parent(ScintillaObject *sci, intptr_t line);

#endif
