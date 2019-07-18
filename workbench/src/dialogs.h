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

#ifndef __WB_DIALOGS_H__
#define __WB_DIALOGS_H__

gchar *dialogs_create_new_file(const gchar *path);
gchar *dialogs_create_new_directory(const gchar *path);
gchar *dialogs_create_new_workbench(void);
gchar *dialogs_open_workbench(void);
gchar *dialogs_add_project(void);
gchar *dialogs_add_directory(WB_PROJECT *project);
gboolean dialogs_directory_settings(WB_PROJECT *project, WB_PROJECT_DIR *directory);
gboolean dialogs_workbench_settings(WORKBENCH *workbench);

#endif
