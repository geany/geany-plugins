/*
 * gdb-io.h - A GLib-based library wrapper for the GNU debugger.
 * Copyright 2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


extern ssize_t getline(char **lineptr, size_t * n, FILE * stream);
extern const gchar *basename(const gchar * path);

gint gdbio_atoi(gchar * str);
gint gdbio_wait(gint ms);


typedef enum
{
	GdbDead,
	GdbLoaded,
	GdbStartup,
	GdbRunning,
	GdbStopped,
	GdbFinished
} GdbStatus;


typedef struct
{
	gchar *type;
	gchar *name;
	gchar *value;
	gchar *numchild;
} GdbVar;


typedef struct
{
	gchar level[12];
	gchar addr[12];
	gchar line[12];
	gchar *func;
	gchar *filename;
	GSList *args;
} GdbFrameInfo;


typedef struct
{
	gchar *signal_name;
	gchar *signal_meaning;
	gchar *addr;
	gchar *func;
	gchar *file;
	gchar *fullname;
	gchar *line;
	gchar *from;
} GdbSignalInfo;


typedef struct
{
	gchar *addr;
	gchar *disp;
	gchar *enabled;
	gchar *file;
	gchar *fullname;
	gchar *func;
	gchar *line;
	gchar *number;
	gchar *times;
	gchar *type;
	gchar *what;
	gchar *cond;
	gchar *ignore;
} GdbBreakPointInfo;


typedef struct
{
	gchar *cwd;
	gchar *path;
	gchar *args;
	gchar *dirs;
} GdbEnvironInfo;


typedef void (*GdbMsgFunc) (const gchar * msg);
typedef void (*GdbListFunc) (const GSList * list);
typedef void (*GdbFrameFunc) (const GdbFrameInfo * frame, const GSList * locals);
typedef void (*GdbSignalFunc) (const GdbSignalInfo * si);
typedef void (*GdbStatusFunc) (GdbStatus status);
typedef void (*GdbStepFunc) (const gchar * filename, const gchar * line, const gchar * reason);
typedef void (*GdbObjectFunc) (const GdbVar * obj, const GSList * list);
typedef void (*GdbEnvironFunc) (const GdbEnvironInfo * env);



/* Load a program into the debugger */
void gdbio_load(const gchar * exe_name);

/* Terminate the debugger ( and the target program, if running ) */
void gdbio_exit();

/* Resume execution after a breakpoint or SIGINT, etc... */
void gdbio_continue();

/* Complete the current function */
void gdbio_finish();

/* Return immediately from the current function */
void gdbio_return();

/*
  Execute the previously loaded program in the debugger.
  If the terminal_command argument is non-null, it will
  be started first, and the target program will be run
  in the resulting console.
*/
void gdbio_exec_target(gchar * terminal_command);


/* Send SIGINT to target */
void gdbio_pause_target();


/* Send SIGKILL to target */
void gdbio_kill_target();


/* Send a command to GDB */
void gdbio_send_cmd(const gchar * fmt, ...);


/*
  Pass a GSList of GdbBreakPointInfo pointers to the func callback
  Showing the current list of breakpoints and watchpoints
*/
void gdbio_show_breaks(GdbListFunc func);


/*
  These 3 functions pass a GSList of GdbBreakPointInfo pointers to
  the func callback showing the current list of breakpoints and
  watchpoints after the requested change has been completed.
*/
void gdbio_add_watch(GdbListFunc func, const gchar * option, const gchar * varname);
void gdbio_add_break(GdbListFunc func, const gchar * filename, const gchar * locn);
void gdbio_delete_break(GdbListFunc func, const gchar * number);


void gdbio_enable_break(const gchar * number, gboolean enabled);
void gdbio_ignore_break(const gchar * number, const gchar * times);
void gdbio_break_cond(const gchar * number, const gchar * expr);



/*
  Pass a GdbEnvironInfo pointer to the callback to show some
  information about current GDB settings.
*/
void gdbio_get_env(GdbEnvironFunc func);


/*
  Pass a GSList of GdbFrameInfo pointers to the func callback
  representing a backtrace of current call stack.
*/
void gdbio_show_stack(GdbListFunc func);


/*
  Passes a GdbFrameInfo pointer and a GSList of GdbVar pointers to func
  representing the state of the local variables at the specified level.
*/
void gdbio_show_locals(GdbFrameFunc func, gchar * level);


/*
  Passes GdbVar pointer containing information about the variable to
  the callback along with a GSList of GdbVar pointers containing
  information about each array element or struct field.
*/
void gdbio_show_object(GdbObjectFunc func, const gchar * varname);



/*
  The fields of the gdbio_setup struct should be initialized by the
  client application before loading the first target program.

  The tty_helper field must point to the full path and filename
  of the included "ttyhelper" program (see ttyhelper.c)

  The temp_dir must point to a readable and writeable directory.

  The client is also responsible for allocating/freeing the
  string fields.
*/

typedef struct _GdbIoSetup
{
	GdbMsgFunc info_func;
	GdbMsgFunc error_func;
	GdbSignalFunc signal_func;
	GdbStatusFunc status_func;
	GdbStepFunc step_func;
	gchar *tty_helper;
	gchar *temp_dir;
} GdbIoSetup;

extern GdbIoSetup gdbio_setup;
