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

#ifndef __WB_MENU_H__
#define __WB_MENU_H__

typedef enum
{
	MENU_CONTEXT_WB_CREATED,
	MENU_CONTEXT_WB_OPENED,
	MENU_CONTEXT_WB_CLOSED,
	MENU_CONTEXT_SEARCH_PROJECTS_SCANING,
}MENU_CONTEXT;

void menu_set_context(MENU_CONTEXT context);
gboolean menu_init(void);
void menu_cleanup (void);

#endif
