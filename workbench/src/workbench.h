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

#ifndef __WB_WORKBENCH_H__
#define __WB_WORKBENCH_H__

#include <glib.h>
#include "wb_project.h"
#include "wb_monitor.h"

typedef enum
{
	PROJECT_ENTRY_STATUS_UNKNOWN,
	PROJECT_ENTRY_STATUS_OK,
	PROJECT_ENTRY_STATUS_NOT_FOUND,
}PROJECT_ENTRY_STATUS;

typedef struct S_WORKBENCH WORKBENCH;

WORKBENCH *workbench_new(void);
void workbench_free(WORKBENCH *wb);

gboolean workbench_is_empty(WORKBENCH *wb);
guint workbench_get_project_count(WORKBENCH *wb);
gboolean workbench_is_modified(WORKBENCH *wb);

void workbench_set_rescan_projects_on_open(WORKBENCH *wb, gboolean value);
gboolean workbench_get_rescan_projects_on_open(WORKBENCH *wb);
void workbench_set_enable_live_update(WORKBENCH *wb, gboolean value);
gboolean workbench_get_enable_live_update(WORKBENCH *wb);
void workbench_set_expand_on_hover(WORKBENCH *wb, gboolean value);
gboolean workbench_get_expand_on_hover(WORKBENCH *wb);
void workbench_set_enable_tree_lines(WORKBENCH *wb, gboolean value);
gboolean workbench_get_enable_tree_lines(WORKBENCH *wb);
void workbench_set_enable_auto_open_by_doc(WORKBENCH *wb, gboolean value);
gboolean workbench_get_enable_auto_open_by_doc(WORKBENCH *wb);

WB_PROJECT *workbench_get_project_at_index(WORKBENCH *wb, guint index);
PROJECT_ENTRY_STATUS workbench_get_project_status_at_index(WORKBENCH *wb, guint index);
PROJECT_ENTRY_STATUS workbench_get_project_status_by_address(WORKBENCH *wb, WB_PROJECT *address);
gboolean workbench_add_project(WORKBENCH *wb, const gchar *filename);
gboolean workbench_remove_project_with_address(WORKBENCH *wb, WB_PROJECT *project);
WB_PROJECT *workbench_file_is_included (WORKBENCH *wb, const gchar *filename);

void workbench_set_filename(WORKBENCH *wb, const gchar *filename);
const gchar *workbench_get_filename(WORKBENCH *wb);
gchar *workbench_get_name(WORKBENCH *wb);
WB_MONITOR *workbench_get_monitor(WORKBENCH *wb);

gboolean workbench_save(WORKBENCH *wb, GError **error);
gboolean workbench_load(WORKBENCH *wb, const gchar *filename, GError **error);

gboolean workbench_add_bookmark(WORKBENCH *wb, const gchar *filename);
gboolean workbench_remove_bookmark(WORKBENCH *wb, const gchar *filename);
gchar *workbench_get_bookmark_at_index (WORKBENCH *wb, guint index);
guint workbench_get_bookmarks_count(WORKBENCH *wb);

void workbench_process_add_file_event(WORKBENCH *wb, WB_PROJECT *prj, WB_PROJECT_DIR *dir, const gchar *file);
void workbench_process_remove_file_event(WORKBENCH *wb, WB_PROJECT *prj, WB_PROJECT_DIR *dir, const gchar *file);
void workbench_enable_live_update(WORKBENCH *wb);
void workbench_disable_live_update(WORKBENCH *wb);

void workbench_open_project(WORKBENCH *wb, WB_PROJECT *project, gboolean select);
WB_PROJECT *workbench_get_selected_project(WORKBENCH *wb);
void workbench_open_project_by_filename(WORKBENCH *wb, const gchar *filename);

#endif
