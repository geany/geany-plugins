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

#ifndef __WB_MONITOR_H__
#define __WB_MONITOR_H__

#include <glib.h>
#include "wb_project.h"

typedef struct S_WB_MONITOR WB_MONITOR;

WB_MONITOR *wb_monitor_new(void);
void wb_monitor_add_dir(WB_MONITOR *monitor, WB_PROJECT *prj,
						WB_PROJECT_DIR *dir, const gchar *dirpath);
gboolean wb_monitor_remove_dir(WB_MONITOR *monitor, const gchar *dirpath);
void wb_monitor_free(WB_MONITOR *monitor);

#endif
