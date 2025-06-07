/*
 * Copyright 2018 LarsGit223
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
 * Code for file monitoring.
 */
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>
#include "wb_globals.h"
#include "workbench.h"
#include "wb_monitor.h"
#include "utils.h"


struct S_WB_MONITOR
{
	GHashTable *monitors;
};

typedef struct
{
	GFileMonitor *monitor;
	WB_PROJECT *prj;
	WB_PROJECT_DIR *dir;
}WB_MONITOR_ENTRY;


/** Create a new, empty WB_MONITOR.
 *
 * @return Address of the new WB_MONITOR
 * 
 **/
WB_MONITOR *wb_monitor_new(void)
{
	WB_MONITOR *monitor;

	monitor = g_new0(WB_MONITOR, 1);

	return monitor;
}


/* Create a new monitor entry */
static WB_MONITOR_ENTRY *wb_monitor_entry_new (GFileMonitor *monitor,
							WB_PROJECT *prj, WB_PROJECT_DIR *dir)
{
	WB_MONITOR_ENTRY *new;

	new =  g_new0(WB_MONITOR_ENTRY, 1);
	new->monitor = monitor;
	new->prj = prj;
	new->dir = dir;

	return new;
}


/* Free a monitor entry */
static void wb_monitor_entry_free (gpointer data)
{
	WB_MONITOR_ENTRY *entry = data;

	if (data != NULL)
	{
		g_object_unref(entry->monitor);
		g_free(entry);
	}
}


/* Callback function for file monitoring. */
static void wb_monitor_file_changed_cb(G_GNUC_UNUSED GFileMonitor *monitor,
									   GFile *file,
									   GFile *other_file,
									   GFileMonitorEvent event,
									   WB_MONITOR_ENTRY *entry)
{
	const gchar *event_string = NULL;
	gchar *file_path, *other_file_path = NULL;
	void (*handler) (WORKBENCH *, WB_PROJECT *, WB_PROJECT_DIR *, const gchar *) = NULL;

	g_return_if_fail(entry != NULL);

	g_message("%s: event: %d", G_STRFUNC, event);

	file_path = g_file_get_path (file);
	if (other_file != NULL)
	{
		other_file_path = g_file_get_path (other_file);
	}
	switch (event)
	{
		case G_FILE_MONITOR_EVENT_CREATED:
			event_string = "FILE_CREATED";
			handler = workbench_process_add_file_event;
			break;

		case G_FILE_MONITOR_EVENT_DELETED:
			event_string = "FILE_DELETED";
			handler = workbench_process_remove_file_event;
			break;

		default:
			break;
	}

	if (event_string != NULL)
	{
		g_message("%s: Prj: \"%s\" Dir: \"%s\" %s: \"%s\"", G_STRFUNC, wb_project_get_name(entry->prj),
			wb_project_dir_get_name(entry->dir), event_string, file_path);
	}

	if (handler != NULL)
	{
		/* The handler might destroy the entry, so we mustn't reference it any
		 * further after calling it */
		handler (wb_globals.opened_wb, entry->prj, entry->dir, file_path);
	}

	g_free(file_path);
	g_free(other_file_path);
}


/** Add a new file monitor
 *
 * Add a new file monitor for dirpath. The monitor will only be created
 * if the settings option "Enable live monitor" is set to on and if
 * no file monitor exists for dirpath already. If monitor creation fails,
 * that means g_file_monitor_directory returns NULL, then a message is
 * output on the statusbar.
 * 
 * @param monitor The global monitor management
 * @param prj     The project to which dirpath belongs
 * @param dir     The directory (WB_PROJECT_DIR) to which dirpath belongs
 * @param dirpath The path of the directory
 * 
 **/
void wb_monitor_add_dir(WB_MONITOR *monitor, WB_PROJECT *prj,
						WB_PROJECT_DIR *dir, const gchar *dirpath)
{
	GFileMonitor *newmon;
	GFile *file;
	GError *error = NULL;
	WB_MONITOR_ENTRY *entry;

	g_return_if_fail(monitor != NULL);
	g_return_if_fail(dir != NULL);
	g_return_if_fail(dirpath != NULL);

	if (workbench_get_enable_live_update(wb_globals.opened_wb) == FALSE)
	{
		/* Return if the feature is disabled. */
		return;
	}

	if (monitor->monitors == NULL)
	{
		monitor->monitors = g_hash_table_new_full
			(g_str_hash, g_str_equal, g_free, wb_monitor_entry_free);
	}
	if (g_hash_table_contains(monitor->monitors, dirpath))
	{
		/* A monitor for that path already exists,
		   do not create another one. */
		return;
	}

	/* Setup file monitor for directory */
	file = g_file_new_for_path(dirpath);
	newmon = g_file_monitor_directory
		(file, G_FILE_MONITOR_NONE, NULL, &error);
	if (newmon == NULL)
	{
		/* Create monitor failed. Report error. */
		ui_set_statusbar(TRUE,
			_("Could not setup file monitoring for directory: \"%s\". Error: %s"),
			dirpath, error->message);
		g_error_free (error);
		return;
	}
	else
	{
		/* Add file monitor to hash table. */
		entry = wb_monitor_entry_new(newmon, prj, dir);
		g_hash_table_insert(monitor->monitors, (gpointer)g_strdup(dirpath), entry);

		g_signal_connect(newmon, "changed",
			G_CALLBACK(wb_monitor_file_changed_cb), entry);

		/* ToDo: make rate limit configurable */
		g_file_monitor_set_rate_limit(newmon, 5 * 1000);
	}
	g_object_unref(file);
}


/** Remove a file monitor
 *
 * Remove the file monitor for dirpath.
 * 
 * @param monitor The global monitor management
 * @param dirpath The path of the directory
 * 
 **/
gboolean wb_monitor_remove_dir(WB_MONITOR *monitor, const gchar *dirpath)
{
	if (monitor == NULL || dirpath == NULL)
	{
		return FALSE;
	}

	/* Free the entry. The hash table will call the destroy function
	   wb_monitor_entry_free which is doing the work for us. */
	return g_hash_table_remove(monitor->monitors, dirpath);
}


/** Free monitor management/all file monitors.
 * 
 * @param monitor The global monitor management
 * 
 **/
void wb_monitor_free(WB_MONITOR *monitor)
{
	if (monitor != NULL)
	{
		if (monitor->monitors != NULL)
		{
			g_hash_table_unref(monitor->monitors);
			monitor->monitors = NULL;
		}
	}
}
