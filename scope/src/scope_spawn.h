/*
 *      scope_spawn.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2013 Dimitar Toshkov Zhekov <dimitar(dot)zhekov(at)gmail(dot)com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef GEANY_SCOPE_SPAWN_H
#define GEANY_SCOPE_SPAWN_H 1

#include <glib.h>

#ifdef G_OS_WIN32
# define SPAWN_WIFEXITED(status) TRUE
# define SPAWN_WEXITSTATUS(status) (status)
# define SPAWN_WIFSIGNALED(status) FALSE
#else
# include <sys/types.h>
# include <sys/wait.h>
# define SPAWN_WIFEXITED(status) WIFEXITED(status)      /**< non-zero if the child exited normally */
# define SPAWN_WEXITSTATUS(status) WEXITSTATUS(status)  /**< exit status of a child if exited normally */
# define SPAWN_WIFSIGNALED(status) WIFSIGNALED(status)  /**< non-zero if the child exited due to signal */
#endif

G_BEGIN_DECLS

gboolean scope_spawn_check_command(const gchar *command_line, gboolean execute, GError **error);

gboolean scope_spawn_kill_process(GPid pid, GError **error);

gboolean scope_spawn_async(const gchar *working_directory, const gchar *command_line, gchar **argv,
	gchar **envp, GPid *child_pid, GError **error);

/** Flags passed to @c spawn_with_callbacks(), which see. */
typedef enum
{
	SCOPE_SPAWN_ASYNC                = 0x00,  /**< Asynchronous execution [default]. */
	SCOPE_SPAWN_SYNC                 = 0x01,  /**< Synchronous execution. */
	/* buffering modes */
	SCOPE_SPAWN_LINE_BUFFERED        = 0x00,  /**< stdout/stderr are line buffered [default]. */
	SCOPE_SPAWN_STDOUT_UNBUFFERED    = 0x02,  /**< stdout is not buffered. */
	SCOPE_SPAWN_STDERR_UNBUFFERED    = 0x04,  /**< stderr is not buffered. */
	SCOPE_SPAWN_UNBUFFERED           = 0x06,  /**< stdout/stderr are not buffered. */
	/* recursive modes */
	SCOPE_SPAWN_STDIN_RECURSIVE      = 0x08,  /**< The stdin callback is recursive. */
	SCOPE_SPAWN_STDOUT_RECURSIVE     = 0x10,  /**< The stdout callback is recursive. */
	SCOPE_SPAWN_STDERR_RECURSIVE     = 0x20,  /**< The stderr callback is recursive. */
	SCOPE_SPAWN_RECURSIVE            = 0x38   /**< All callbacks are recursive. */
} ScopeSpawnFlags;

/**
 *  Specifies the type of function passed to @c spawn_with_callbacks() as stdout or stderr
 *  callback.
 *
 *  In unbuffered mode, the @a string may contain nuls, while in line buffered mode, it may
 *  contain only a single nul as a line termination character at @a string->len - 1. In all
 *  cases, the @a string will be terminated with a nul character that is not part of the data
 *  at @a string->len.
 *
 *  If @c G_IO_IN or @c G_IO_PRI are set, the @a string will contain at least one character.
 *
 *  @param string contains the child data if @c G_IO_IN or @c G_IO_PRI are set.
 *  @param condition the I/O condition which has been satisfied.
 *  @param data the passed to @c spawn_with_callbacks() with the callback.
 */
typedef void (*SpawnReadFunc)(GString *string, GIOCondition condition, gpointer data);

gboolean scope_spawn_with_callbacks(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, ScopeSpawnFlags spawn_flags, GIOFunc stdin_cb, gpointer stdin_data,
	SpawnReadFunc stdout_cb, gpointer stdout_data, gsize stdout_max_length,
	SpawnReadFunc stderr_cb, gpointer stderr_data, gsize stderr_max_length,
	GChildWatchFunc exit_cb, gpointer exit_data, GPid *child_pid, GError **error);

/** 
 *  A simple structure used by @c spawn_write_data() to write data to a channel.
 *  See @c spawn_write_data() for more information.
 */
typedef struct _ScopeSpawnWriteData
{
	const gchar *ptr;   /**< Pointer to the data. May be NULL if the size is 0. */
	gsize size;         /**< Size of the data. */
} ScopeSpawnWriteData;

gboolean scope_spawn_write_data(GIOChannel *channel, GIOCondition condition, ScopeSpawnWriteData *data);

gboolean scope_spawn_sync(const gchar *working_directory, const gchar *command_line, gchar **argv,
	gchar **envp, ScopeSpawnWriteData *stdin_data, GString *stdout_data, GString *stderr_data,
	gint *exit_status, GError **error);

G_END_DECLS

#endif  /* GEANY_SCOPE_SPAWN_H */
