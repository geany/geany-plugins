/*
 * Copyright 2019 LarsGit223
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

#ifndef __WB_IDLE_QUEUE_H__
#define __WB_IDLE_QUEUE_H__

typedef enum
{
	WB_IDLE_ACTION_ID_TM_SOURCE_FILE_ADD,
	WB_IDLE_ACTION_ID_TM_SOURCE_FILE_REMOVE,
	WB_IDLE_ACTION_ID_TM_SOURCE_FILE_FREE,
	WB_IDLE_ACTION_ID_TM_SOURCE_FILES_NEW,
	WB_IDLE_ACTION_ID_TM_SOURCE_FILES_REMOVE,
#if GEANY_API_VERSION >= 240
	WB_IDLE_ACTION_ID_OPEN_PROJECT_BY_DOC,
#endif
	WB_IDLE_ACTION_ID_OPEN_DOC,
}WB_IDLE_QUEUE_ACTION_ID;

void wb_idle_queue_add_action(WB_IDLE_QUEUE_ACTION_ID id, gpointer param_a);

#endif
