/*
 *  debug.h
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

#ifndef DEBUG_H

typedef enum _DebugState
{
	DS_INACTIVE = 0x01,  /* gdb not loaded */
	DS_BUSY     = 0x02,
	DS_READY    = 0x04,  /* at prompt, has threads, none/running thread selected */
	DS_DEBUG    = 0x08,  /* at prompt, has threads, stopped thread selected */
	DS_HANGING  = 0x10,  /* at prompt, no threads */
	DS_VARIABLE = 0x18,
	DS_SENDABLE = 0x1C,
	DS_NOT_BUSY = 0x1D,
	DS_ACTIVE   = 0x1E,
	DS_BASICS   = 0x1F,
	DS_EXTRA_1  = 0x20,  /* also Assem */
	DS_EXTRA_2  = 0x40,  /* also Load */
	DS_EXTRA_3  = 0x80,
	DS_EXTRA_4  = 0x100,
	DS_EXTRAS   = 0x1E0
} DebugState;

enum
{
	DS_INDEX_1 = 5,
	DS_INDEX_2,
	DS_INDEX_3,
	DS_INDEX_4
};

DebugState debug_state(void);  /* single basic bit only */

void on_debug_list_source(GArray *nodes);
void on_debug_error(GArray *nodes);
void on_debug_loaded(GArray *nodes);
void on_debug_load_error(GArray *nodes);
void on_debug_exit(GArray *nodes);
void on_debug_auto_run(GArray *nodes);
void on_debug_auto_exit(void);

void on_debug_run_continue(const MenuItem *menu_item);
void on_debug_goto_cursor(const MenuItem *menu_item);
void on_debug_goto_source(const MenuItem *menu_item);
void on_debug_step_into(const MenuItem *menu_item);
void on_debug_step_over(const MenuItem *menu_item);
void on_debug_step_out(const MenuItem *menu_item);
void on_debug_terminate(const MenuItem *menu_item);

enum { N, T, F };
void debug_send_command(gint tf, const char *command);
#define debug_send_thread(command) debug_send_command(T, (command))
void debug_send_format(gint tf, const char *format, ...) G_GNUC_PRINTF(2, 3);
char *debug_send_evaluate(char token, gint scid, const gchar *expr);  /* == locale(expr) */

void debug_init(void);
void debug_finalize(void);

#define DEBUG_H 1
#endif
