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

/*
 * Code for the Workbench idle queue.
 */
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>
#include "wb_globals.h"
#include "idle_queue.h"
#include "tm_control.h"

extern GeanyData *geany_data;

typedef struct
{
	WB_IDLE_QUEUE_ACTION_ID id;
	gpointer param_a;
}WB_IDLE_QUEUE_ACTION;

static GSList *s_idle_actions = NULL;


/* Clear idle queue */
static void wb_idle_queue_clear(void)
{
	if (s_idle_actions == NULL)
	{
		return;
	}

	g_slist_free_full(s_idle_actions, g_free);
	s_idle_actions = NULL;
}


/* On-idle callback function. */
static gboolean wb_idle_queue_callback(gpointer foo)
{
	static gboolean first = TRUE;
	GSList *elem = NULL;
	WB_IDLE_QUEUE_ACTION *action;
	static GMutex mutex;

	if (first == TRUE)
	{
		first = FALSE;
		g_mutex_init (&mutex);
	}

	g_mutex_lock(&mutex);

	foreach_slist (elem, s_idle_actions)
	{
		action = elem->data;
		switch (action->id)
		{
			case WB_IDLE_ACTION_ID_TM_SOURCE_FILES_NEW:
				wb_tm_control_source_files_new(action->param_a);
			break;

			case WB_IDLE_ACTION_ID_TM_SOURCE_FILE_ADD:
				wb_tm_control_source_file_add(action->param_a);
			break;

			case WB_IDLE_ACTION_ID_TM_SOURCE_FILE_REMOVE:
				wb_tm_control_source_file_remove(action->param_a);
			break;

			case WB_IDLE_ACTION_ID_TM_SOURCE_FILE_FREE:
				wb_tm_control_source_file_free(action->param_a);
			break;

			case WB_IDLE_ACTION_ID_TM_SOURCE_FILES_REMOVE:
				wb_tm_control_source_files_remove(action->param_a);
			break;
#if GEANY_API_VERSION >= 240
			case WB_IDLE_ACTION_ID_OPEN_PROJECT_BY_DOC:
				workbench_open_project_by_filename(wb_globals.opened_wb, action->param_a);
				g_free(action->param_a);
			break;
#endif
			case WB_IDLE_ACTION_ID_OPEN_DOC:
				document_open_file(action->param_a, FALSE, NULL, NULL);
				g_free(action->param_a);
			break;
		}
	}

	wb_idle_queue_clear();

	g_mutex_unlock(&mutex);

	return FALSE;
}


/** Add a new idle action to the list.
 *
 * The function allocates a new WB_IDLE_QUEUE_ACTION structure and fills
 * in the values passed. On-idle Geany will then call wb_idle_queue_callback
 * and that function will call the function related to the action ID
 * and pass the relevant parameters to it.
 * 
 * @param id The action to execute on-idle
 * @param param_a Parameter A
 * @param param_a Parameter B
 *
 **/
void wb_idle_queue_add_action(WB_IDLE_QUEUE_ACTION_ID id, gpointer param_a)
{
	WB_IDLE_QUEUE_ACTION *action;

	action = g_new0(WB_IDLE_QUEUE_ACTION, 1);
	action->id = id;
	action->param_a = param_a;

	if (s_idle_actions == NULL)
	{
		plugin_idle_add(wb_globals.geany_plugin, (GSourceFunc)wb_idle_queue_callback, NULL);
	}

	s_idle_actions = g_slist_append(s_idle_actions, action);
}
