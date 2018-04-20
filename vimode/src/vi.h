/*
 * Copyright 2018 Jiri Techet <techet@gmail.com>
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

#ifndef __VIMODE_VI_H__
#define __VIMODE_VI_H__

#include "sci.h"

#define VI_IS_VISUAL(m) ((m) == VI_MODE_VISUAL || (m) == VI_MODE_VISUAL_LINE || (m) == VI_MODE_VISUAL_BLOCK)
#define VI_IS_COMMAND(m) ((m) == VI_MODE_COMMAND || (m) == VI_MODE_COMMAND_SINGLE)
#define VI_IS_INSERT(m) ((m) == VI_MODE_INSERT || (m) == VI_MODE_REPLACE)

typedef enum {
	VI_MODE_COMMAND,
	VI_MODE_COMMAND_SINGLE, //performing single command from insert mode using Ctrl+O
	VI_MODE_VISUAL,
	VI_MODE_VISUAL_LINE,
	VI_MODE_VISUAL_BLOCK, //not implemented
	VI_MODE_INSERT,
	VI_MODE_REPLACE,
} ViMode;

typedef struct
{
	void (*on_mode_change)(ViMode mode);
	gboolean (*on_save)(gboolean force);
	gboolean (*on_save_all)(gboolean force);
	void (*on_quit)(gboolean force);
} ViCallback;


void vi_enter_ex_mode(void);
void vi_set_mode(ViMode mode);
ViMode vi_get_mode(void);

gboolean vi_notify_sci(SCNotification *nt);
gboolean vi_notify_key_press(GdkEventKey *event);

void vi_init(GtkWidget *parent_window, ViCallback *cb);
void vi_cleanup(void);

void vi_set_active_sci(ScintillaObject *sci);

void vi_set_enabled(gboolean enabled);
void vi_set_insert_for_dummies(gboolean enabled);
gboolean vi_get_enabled(void);
gboolean vi_get_insert_for_dummies(void);

#endif
