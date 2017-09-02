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

#ifndef __WB_GLOBALS_H__
#define __WB_GLOBALS_H__

#include <geanyplugin.h>
#include "workbench.h"

typedef struct
{
    GeanyPlugin *geany_plugin;
    WORKBENCH   *opened_wb;
}WB_GLOBALS;

extern WB_GLOBALS wb_globals;

void workbench_globals_init(void);

#endif
