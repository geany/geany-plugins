/*
 * Copyright 2013 The Geany contributors
 * Copyright 2023 Jiri Techet <techet@gmail.com>
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

// stolen from Geany, made lsp_spawn_async_with_pipes() public, removed unneeded stuff

#ifndef LSP_SPAWN_H
#define LSP_SPAWN_H 1

#include <glib.h>
#include <geanyplugin.h>

G_BEGIN_DECLS

gboolean lsp_spawn_with_pipes_and_stderr_callback(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, gint *stdin_fd, gint *stdout_fd,
	SpawnReadFunc stderr_cb, gpointer stderr_data, gsize stderr_max_length,
	GChildWatchFunc exit_cb, gpointer exit_data, GPid *child_pid, GError **error);

G_END_DECLS

#endif  /* LSP_SPAWN_H */
