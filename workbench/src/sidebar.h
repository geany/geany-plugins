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

#ifndef __SIDEBAR_H__
#define __SIDEBAR_H__

#include <gtk/gtk.h>
#include "wb_project.h"

enum
{
	DATA_ID_UNSET = 0,
	DATA_ID_WB_BOOKMARK,
	DATA_ID_PROJECT,
	DATA_ID_PRJ_BOOKMARK,
	DATA_ID_DIRECTORY,
	DATA_ID_NO_DIRS,
	DATA_ID_SUB_DIRECTORY,
	DATA_ID_FILE,
};

typedef struct
{
	WB_PROJECT     *project;
	WB_PROJECT_DIR *directory;
	gchar          *subdir;
	gchar          *file;
	gchar          *wb_bookmark;
	gchar          *prj_bookmark;
}SIDEBAR_CONTEXT;

typedef enum
{
	SIDEBAR_CONTEXT_WB_CREATED,
	SIDEBAR_CONTEXT_WB_OPENED,
	SIDEBAR_CONTEXT_WB_SAVED,
	SIDEBAR_CONTEXT_WB_SETTINGS_CHANGED,
	SIDEBAR_CONTEXT_WB_CLOSED,
	SIDEBAR_CONTEXT_PROJECT_ADDED,
	SIDEBAR_CONTEXT_PROJECT_SAVED,
	SIDEBAR_CONTEXT_PROJECT_REMOVED,
	SIDEBAR_CONTEXT_DIRECTORY_ADDED,
	SIDEBAR_CONTEXT_DIRECTORY_REMOVED,
	SIDEBAR_CONTEXT_DIRECTORY_RESCANNED,
	SIDEBAR_CONTEXT_DIRECTORY_SETTINGS_CHANGED,
	SIDEBAR_CONTEXT_WB_BOOKMARK_ADDED,
	SIDEBAR_CONTEXT_WB_BOOKMARK_REMOVED,
	SIDEBAR_CONTEXT_PRJ_BOOKMARK_ADDED,
	SIDEBAR_CONTEXT_PRJ_BOOKMARK_REMOVED,
	SIDEBAR_CONTEXT_FILE_ADDED,
	SIDEBAR_CONTEXT_FILE_REMOVED,
	SIDEBAR_CONTEXT_SELECTED_PROJECT_TOGGLED,
}SIDEBAR_EVENT;

void sidebar_init(void);
void sidebar_cleanup(void);

void sidebar_activate(void);
void sidebar_deactivate(void);

void sidebar_show_intro_message(const gchar *msg, gboolean activate);

void sidebar_update(SIDEBAR_EVENT event, SIDEBAR_CONTEXT *context);

void sidebar_expand_all(void);
void sidebar_collapse_all(void);
void sidebar_expand_selected_project (void);
void sidebar_collapse_selected_project (void);
void sidebar_toggle_selected_project_expansion (void);
void sidebar_toggle_selected_project_dir_expansion (void);

WB_PROJECT *sidebar_file_view_get_selected_project(GtkTreePath **path);
gboolean sidebar_file_view_get_selected_context(SIDEBAR_CONTEXT *context);

GPtrArray *sidebar_get_selected_project_filelist (gboolean dirnames);
GPtrArray *sidebar_get_selected_directory_filelist (gboolean dirnames);
GPtrArray *sidebar_get_selected_subdir_filelist (gboolean dirnames);

void sidebar_call_foreach(guint dataid,
	void (func)(SIDEBAR_CONTEXT *, gpointer userdata), gpointer userdata);

#endif
