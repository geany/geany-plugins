/*
 *      debug.h
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */
 
#include "debug_module.h"

/* debug states enumeration */
enum dbs {
	DBS_IDLE,
	DBS_STOPPED,
	DBS_STOP_REQUESTED,
	DBS_RUNNING,
	DBS_RUN_REQUESTED,
};

/* function type to execute on interrupt */
typedef void (*bs_callback)(breakpoint*, break_set_activity);

void		debug_init(GtkWidget* nb);
enum dbs	debug_get_state();
void		debug_run();
void		debug_stop();
void		debug_step_over();
void		debug_step_into();
void		debug_step_out();
void		debug_execute_until(gchar *file, int line);
gboolean	debug_set_break(breakpoint* bp, break_set_activity bsa);
gboolean	debug_remove_break(breakpoint* bp);
void		debug_request_interrupt(bs_callback cb, breakpoint* bp, break_set_activity flags);
gchar*		debug_error_message();
GList*		debug_get_modules();
int			debug_get_module_index(gchar *modulename);
gboolean	debug_supports_async_breaks();
void		debug_destroy();
gchar*		debug_evaluate_expression(gchar *expression);
gboolean	debug_current_instruction_have_sources();
void		debug_jump_to_current_instruction();
void		debug_on_file_open(GeanyDocument *doc);

