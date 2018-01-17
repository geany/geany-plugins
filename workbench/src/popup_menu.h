/*
 * Copyright 2017 LarsGit223
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

#ifndef __POPUP_MENU_H__
#define __POPUP_MENU_H__

#include <gdk/gdk.h>

typedef enum
{
	POPUP_CONTEXT_PROJECT,
	POPUP_CONTEXT_DIRECTORY,
	POPUP_CONTEXT_SUB_DIRECTORY,
	POPUP_CONTEXT_FILE,
	POPUP_CONTEXT_BACKGROUND,
	POPUP_CONTEXT_WB_BOOKMARK,
	POPUP_CONTEXT_PRJ_BOOKMARK,
}POPUP_CONTEXT;

void popup_menu_init(void);
void popup_menu_show(POPUP_CONTEXT context, GdkEventButton *event);

#endif
