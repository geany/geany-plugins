/*
 *      debug_module.h
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

#ifndef DEBUG_MODULE_H
#define DEBUG_MODULE_H

/* debug states enumeration */
enum dbs {
	DBS_IDLE,
	DBS_STOPPED,
	DBS_STOP_REQUESTED,
	DBS_RUNNING,
	DBS_RUN_REQUESTED,
};

/* type to hold callbacks to call from debugger modules */
typedef struct _dbg_callbacks {
	void (*set_run) (void);
	void (*set_stopped) (int thread_id);
	void (*set_exited) (int code);
	void (*send_message) (const gchar* message, const gchar *color);
	void (*clear_messages) (void);
	void (*report_error) (const gchar* message);
	void (*add_thread) (int thread_id);
	void (*remove_thread) (int thread_id);
} dbg_callbacks;

typedef enum _variable_type {
	VT_ARGUMENT,
	VT_LOCAL,
	VT_WATCH,
	VT_GLOBAL,
	VT_CHILD,
	VT_NONE
} variable_type;

/* type to hold information about a variable */
typedef struct _variable {
	/* variable name */
	GString *name;
	/* internal name - GDB specific name to get information
	 * about a watch or local variable*/
	GString *internal;
	/* expression of a variable */
	GString *expression;
	/* variable type */
	GString *type;
	/* variable value */
	GString *value;
	/* flag indicating whether a variable has children */
	gboolean has_children;
	/* flag indicating whether getting variable value was successfull */
	gboolean evaluated;
	/* variable type */
	variable_type vt;
} variable;

/* type to hold information about a stack frame */
typedef struct _frame {
	gchar *address;
	gchar *function;
	gchar *file;
	gint line;
	gboolean have_source;
} frame;

/* enumeration for module features */
typedef enum _module_features
{
	MF_ASYNC_BREAKS = 1 << 0
} module_features;

/* enumeration for breakpoints activities */
typedef enum _break_set_activity {
	BSA_NEW_BREAK,
	BSA_UPDATE_ENABLE,
	BSA_UPDATE_CONDITION,
	BSA_UPDATE_HITS_COUNT,
	BSA_REMOVE
} break_set_activity;

/* type to hold pointers to describe a debug module */
typedef struct _dbg_module {
	
	gboolean (*run) (const gchar* target, const gchar* commandline, GList* env, GList *witer, GList *biter, const gchar* terminal_device, dbg_callbacks* callbacks);
	void (*restart) (void);
	void (*stop) (void);
	void (*resume) (void);
	void (*step_over) (void);
	void (*step_into) (void);
	void (*step_out) (void);
	void (*execute_until)(const gchar *file, int line);

	gboolean (*set_break) (breakpoint* bp, break_set_activity bsa);
	gboolean (*remove_break) (breakpoint* bp);

	GList* (*get_stack) (void);

	void (*set_active_frame)(int frame_number);
	int (*get_active_frame)(void);
		
	GList* (*get_autos) (void);
	GList* (*get_watches) (void);
	
	GList* (*get_files) (void);

	GList* (*get_children) (gchar* path);
	variable* (*add_watch)(gchar* expression);
	void (*remove_watch)(gchar* path);

	gchar* (*evaluate_expression)(gchar *expression);
	
	gboolean (*request_interrupt) (void);
	gchar* (*error_message) (void);
	module_features features;
	
} dbg_module;

/* helping macroes to declare and define degub module */
#define DBG_MODULE_DECLARE(name) extern dbg_module dbg_module##name
#define DBG_MODULE_DEFINE(name) dbg_module dbg_module_##name = { \
	run, \
	restart, \
	stop, \
	resume, \
	step_over, \
	step_into, \
	step_out, \
	execute_until, \
	set_break, \
	remove_break, \
	get_stack, \
	set_active_frame, \
	get_active_frame, \
	get_autos, \
	get_watches, \
	get_files, \
	get_children, \
	add_watch, \
	remove_watch, \
	evaluate_expression, \
	request_interrupt, \
	error_message, \
	MODULE_FEATURES }

void		variable_free(variable *var);
variable*	variable_new(gchar *name, variable_type vt);
variable*	variable_new2(gchar *name, gchar *internal, variable_type vt);
void		variable_reset(variable *var);

frame*	frame_new(void);
void		frame_free(frame* f);

#endif /* guard */
