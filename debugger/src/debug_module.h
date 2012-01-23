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
	void (*set_run) ();
	void (*set_stopped) ();
	void (*set_exited) (int code);
	void (*send_message) (const gchar* message, const gchar *color);
	void (*clear_messages) ();
	void (*report_error) (const gchar* message);
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
	
	gboolean (*init) (dbg_callbacks* callbacks);
	gboolean (*load) (char* file, char* commandline, GList* env, GList *witer);
	void (*run) (char* terminal_device);
	void (*restart) ();
	void (*stop) ();
	void (*resume) ();
	void (*step_over) ();
	void (*step_into) ();
	void (*step_out) ();
	void (*execute_until)(const gchar *file, int line);

	gboolean (*set_break) (breakpoint* bp, break_set_activity bsa);
	gboolean (*remove_break) (breakpoint* bp);
	GList* (*get_stack) ();
	
	GList* (*get_autos) ();
	GList* (*get_watches) ();
	
	GList* (*get_files) ();

	GList* (*get_children) (gchar* path);
	variable* (*add_watch)(gchar* expression);
	void (*remove_watch)(gchar* path);

	gchar* (*evaluate_expression)(gchar *expression);
	
	gboolean (*request_interrupt) ();
	gchar* (*error_message) ();
	module_features features;
	
} dbg_module;

/* helping macroes to declare and define degub module */
#define DBG_MODULE_DECLARE(name) extern dbg_module dbg_module##name
#define DBG_MODULE_DEFINE(name) dbg_module dbg_module_##name = { \
	init, \
	load, \
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

frame*	frame_new();
void		frame_free(frame* f);
