/*
 * Copyright 2026 WrapEarnPass
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

#ifndef __AUTORUN_H__
#define __AUTORUN_H__

#include <geanyplugin.h>

typedef struct {
	GeanyPlugin* plugin;
	GeanyData* data;
	GSList* filedef_commands;
	GSList* project_commands;
	gboolean dirtybit;
	gint children;
} AUTORUN_GLOBALS;

typedef struct {
	GeanyFiletype* file_type;
	gchar* interceptor;
	gushort order;
	gchar* command;
	gchar* working_dir;
	gboolean invalid;
} AUTORUN_CMD;

extern AUTORUN_GLOBALS* autorun_globals;

void autorun_globals_init(GeanyPlugin* plugin);
void autorun_globals_free(void);
void autorun_cmd_list_free(GSList* command_list);
void autorun_cmd_free(AUTORUN_CMD* cmd);
AUTORUN_CMD* autorun_cmd_new(void);
void load_filedefs(void);
void load_projectdefs(GKeyFile* config);

#endif
