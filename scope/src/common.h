/*
 *  common.h
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "geanyplugin.h"

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;

typedef struct _MenuItem MenuItem;
#define FRAME_ARGS '0' + (int) strlen(thread_id) - 1, thread_id, frame_id

#include "break.h"
#include "conterm.h"
#include "debug.h"
#include "inspect.h"
#include "local.h"
#include "memory.h"
#include "menu.h"
#include "parse.h"
#include "plugme.h"
#include "prefs.h"
#include "program.h"
#include "register.h"
#include "stack.h"
#include "scope.h"
#include "store.h"
#include "thread.h"
#include "tooltip.h"
#include "utils.h"
#include "views.h"
#include "watch.h"
