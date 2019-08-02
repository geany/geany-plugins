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

#ifndef __WB_PROJECT_H__
#define __WB_PROJECT_H__

#include <glib.h>

typedef struct S_WB_PROJECT WB_PROJECT;
typedef struct S_WB_PROJECT_DIR WB_PROJECT_DIR;

typedef enum
{
	WB_PROJECT_SCAN_MODE_INVALID,
	WB_PROJECT_SCAN_MODE_WORKBENCH,
	WB_PROJECT_SCAN_MODE_GIT,
}WB_PROJECT_SCAN_MODE;

WB_PROJECT *wb_project_new(const gchar *filename);
void wb_project_free(WB_PROJECT *prj);

void wb_project_set_modified(WB_PROJECT *prj, gboolean value);
gboolean wb_project_is_modified(WB_PROJECT *prj);
void wb_project_set_active(WB_PROJECT *prj, gboolean value);
gboolean wb_project_is_active(WB_PROJECT *prj);

void wb_project_set_filename(WB_PROJECT *prj, const gchar *filename);
const gchar *wb_project_get_filename(WB_PROJECT *prj);
const gchar *wb_project_get_name(WB_PROJECT *prj);
GSList *wb_project_get_directories(WB_PROJECT *prj);
gboolean wb_project_add_directory(WB_PROJECT *prj, const gchar *dirname);
gboolean wb_project_remove_directory (WB_PROJECT *prj, WB_PROJECT_DIR *dir);
void wb_project_rescan(WB_PROJECT *prj);
gboolean wb_project_file_is_included(WB_PROJECT *prj, const gchar *filename);
gboolean wb_project_is_valid_dir_reference(WB_PROJECT *prj, WB_PROJECT_DIR *dir);

void wb_project_dir_set_is_prj_base_dir (WB_PROJECT_DIR *directory, gboolean value);
gboolean wb_project_dir_get_is_prj_base_dir (WB_PROJECT_DIR *directory);
const gchar *wb_project_dir_get_name (WB_PROJECT_DIR *directory);
GHashTable *wb_project_dir_get_file_table (WB_PROJECT_DIR *directory);
gchar *wb_project_dir_get_base_dir (WB_PROJECT_DIR *directory);
gchar **wb_project_dir_get_file_patterns (WB_PROJECT_DIR *directory);
gboolean wb_project_dir_set_file_patterns (WB_PROJECT_DIR *directory, gchar **new);
gchar **wb_project_dir_get_ignored_dirs_patterns (WB_PROJECT_DIR *directory);
gboolean wb_project_dir_set_ignored_dirs_patterns (WB_PROJECT_DIR *directory, gchar **new);
gchar **wb_project_dir_get_ignored_file_patterns (WB_PROJECT_DIR *directory);
gboolean wb_project_dir_set_ignored_file_patterns (WB_PROJECT_DIR *directory, gchar **new);
WB_PROJECT_SCAN_MODE wb_project_dir_get_scan_mode (WB_PROJECT_DIR *directory);
gboolean wb_project_dir_set_scan_mode (WB_PROJECT *project, WB_PROJECT_DIR *directory, WB_PROJECT_SCAN_MODE mode);
guint wb_project_dir_rescan(WB_PROJECT *prj, WB_PROJECT_DIR *root);
gchar *wb_project_dir_get_info (WB_PROJECT_DIR *dir);
gboolean wb_project_dir_file_is_included(WB_PROJECT_DIR *dir, const gchar *filename);
void wb_project_dir_add_file(WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath);
void wb_project_dir_remove_file(WB_PROJECT *prj, WB_PROJECT_DIR *root, const gchar *filepath);

gboolean wb_project_add_bookmark(WB_PROJECT *prj, const gchar *filename);
gboolean wb_project_remove_bookmark(WB_PROJECT *prj, const gchar *filename);
GPtrArray *wb_project_get_bookmarks(WB_PROJECT *prj);
gchar *wb_project_get_bookmark_at_index (WB_PROJECT *prj, guint index);
guint wb_project_get_bookmarks_count(WB_PROJECT *prj);

gboolean wb_project_save(WB_PROJECT *prj, GError **error);
gboolean wb_project_load(WB_PROJECT *prj, const gchar *filename, GError **error);

gchar *wb_project_get_info (WB_PROJECT *prj);

#endif
