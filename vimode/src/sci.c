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

#include "sci.h"

void _set_current_position(ScintillaObject *sci, gint position, gboolean scroll_to_caret,
	gboolean caretx)
{
	if (scroll_to_caret)
		SSM(sci, SCI_GOTOPOS, (uptr_t) position, 0);
	else
	{
		SSM(sci, SCI_SETCURRENTPOS, (uptr_t) position, 0);
		SSM(sci, SCI_SETANCHOR, (uptr_t) position, 0); /* to avoid creation of a selection */
	}
	if (caretx)
		SSM(sci, SCI_CHOOSECARETX, 0, 0);
}
