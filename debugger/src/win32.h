/*
 *      win32.h
 *
 *      Copyright 2012 Vadim Kochan <vadim4j@gmail.com>
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


#ifndef _DBG_WIN32_HEADER_
#define _DBG_WIN32_HEADER_

#include <glib.h>

/* Kills windows process */
void win32_kill(GPid pid, gint exit_status);


/* Spawns windows process with I/O redirection */
/* through the pipe                            */
gboolean	win32_spawn_async_with_pipes( const gchar *working_directory, 
										gchar **argv, gchar **envp,
										GPid *child_pid,
										gint *standard_input,
										gint *standard_output,
										gint *standard_error,
										GError **error
										);
										

#endif /* _DBG_WIN32_HEADER_ */
