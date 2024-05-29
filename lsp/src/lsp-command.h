/*
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

#ifndef LSP_COMMAND_H
#define LSP_COMMAND_H 1

#include "lsp-server.h"

#include <glib.h>

typedef struct
{
	guint line;
	gchar *title;
	gchar *command;
	GVariant *arguments;
	GVariant *edit;
} LspCommand;

void lsp_command_free(LspCommand *cmd);

void lsp_command_perform(LspServer *server, LspCommand *cmd, GCallback callback);

void lsp_command_send_code_action_init(void);
void lsp_command_send_code_action_destroy(void);
void lsp_command_send_code_action_request(gint pos, LspCallback actions_resolved_cb, gpointer user_data);
GPtrArray *lsp_command_get_resolved_code_actions(void);

#endif  /* LSP_COMMAND_H */
