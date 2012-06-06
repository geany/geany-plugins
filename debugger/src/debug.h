/*
 *      debug.h
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
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
#include "markers.h"

/* function type to execute on interrupt */
typedef void	(*bs_callback)(gpointer);

void			debug_init(void);
enum dbs		debug_get_state(void);
void			debug_run(void);
void			debug_stop(void);
void			debug_step_over(void);
void			debug_step_into(void);
void			debug_step_out(void);
void			debug_execute_until(const gchar *file, int line);
gboolean		debug_set_break(breakpoint* bp, break_set_activity bsa);
gboolean		debug_remove_break(breakpoint* bp);
void			debug_request_interrupt(bs_callback cb, gpointer data);
gchar*			debug_error_message(void);
GList*			debug_get_modules(void);
int				debug_get_module_index(const gchar *modulename);
gboolean		debug_supports_async_breaks(void);
void			debug_destroy(void);
gchar*			debug_evaluate_expression(gchar *expression);
gboolean		debug_current_instruction_have_sources(void);
void			debug_jump_to_current_instruction(void);
void			debug_on_file_open(GeanyDocument *doc);
gchar*			debug_get_calltip_for_expression(gchar* expression);
GList*			debug_get_stack(void);
void			debug_restart(void);
int				debug_get_active_frame(void);

