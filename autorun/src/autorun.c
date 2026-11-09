/*
 * Copyright 2026 WrapEarnPass
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

#include <geanyplugin.h>

#include "autorun.h"
#include "utils.h"

AUTORUN_GLOBALS* autorun_globals;

/* allocate an AUTORUN_CMD struct
 * should be cleaned up with a matching call to autorun_cmd_free */
AUTORUN_CMD* autorun_cmd_new() {
	AUTORUN_CMD* cmd = NULL;
	cmd = g_new0(AUTORUN_CMD, 1);
	cmd->file_type = NULL;
	cmd->interceptor = NULL;
	cmd->command = NULL;
	cmd->working_dir = NULL;
	cmd->invalid = TRUE;
	cmd->order = 0;
	return cmd;
}

/* free an AUTORUN_CMD in a safe manner */
void autorun_cmd_free(AUTORUN_CMD* cmd) {
	/* do not check for cmd->invalid because it could be a partial command
	 * or invalidated after assembly. */
	if (!cmd) {
		// who are you and why are you in my attic?
		return;
	}
	// free the command properties
	if (cmd->file_type) {
		cmd->file_type = NULL;
	}
	if (cmd->interceptor) {
		g_free(cmd->interceptor);
		cmd->interceptor = NULL;
	}
	if (cmd->command) {
		g_free(cmd->command);
		cmd->command = NULL;
	}
	if (cmd->working_dir) {
		g_free(cmd->working_dir);
		cmd->working_dir = NULL;
	}
	// and the struct itself
	g_free(cmd);
}

/* helper to free a GSList of AUTORUN_CMD */
void autorun_cmd_list_free(GSList* command_list) {
	if (!command_list) {
		// why are you naked?
		return;
	}
	g_slist_free_full(command_list, (GDestroyNotify)autorun_cmd_free);
}

/* establish the autorun_globals */
void autorun_globals_init(GeanyPlugin* plugin) {
	if (!autorun_globals) {
		autorun_globals = g_new0(AUTORUN_GLOBALS, 1);
		autorun_globals->plugin = plugin;
		autorun_globals->data = plugin->geany_data;
		autorun_globals->filedef_commands = NULL;
		autorun_globals->project_commands = NULL;
		autorun_globals->dirtybit = FALSE;
		autorun_globals->children = 0;
	}
}

/* teardown any linked properties from autorun_globals */
void autorun_globals_free(void) {
	if (autorun_globals) {
		if (autorun_globals->filedef_commands) {
			autorun_cmd_list_free(autorun_globals->filedef_commands);
			autorun_globals->filedef_commands = NULL;
		}
		if (autorun_globals->project_commands) {
			autorun_cmd_list_free(autorun_globals->project_commands);
			autorun_globals->project_commands = NULL;
		}
		autorun_globals->data = NULL;
		autorun_globals->plugin = NULL;
		g_free(autorun_globals);
		autorun_globals = NULL;
	}
}

/* helper function to process .config/geany/filedefs/
 * to load any [autorun] handlers */
void load_filedefs(void) {
	// load the config dir
	gchar* filedef_path = g_build_path(G_DIR_SEPARATOR_S, autorun_globals->data->app->configdir, GEANY_FILEDEFS_SUBDIR, NULL);
	guint filedef_len;
	GSList* file_list = utils_get_file_list(filedef_path, &filedef_len, NULL);
	// find any filetypes.FILE
	if (filedef_len > 0) {
		GSList* node = NULL;
		foreach_slist(node, file_list) {
			if (g_str_has_prefix(node->data, "filetypes.") && g_strcmp0(node->data, "filetypes.README") != 0) {
				// we have files to process
				gsize key_len;
				gchar* filedef_file = g_build_filename(G_DIR_SEPARATOR_S, filedef_path, (gchar*)node->data, NULL);
				GKeyFile* config = g_key_file_new();
				g_key_file_load_from_file(config, filedef_file, G_KEY_FILE_NONE, NULL);
				GError* gerr = NULL;
				gchar** handlers = g_key_file_get_keys(config, "autorun", &key_len, &gerr);
				// if the keyfile has an [autorun] section
				if (!gerr) {
					gchar** handler_key;
					foreach_strv(handler_key, handlers) {
						if (g_str_has_suffix(*handler_key, "CM")) {
							AUTORUN_CMD* cmd = autorun_cmd_new();
							cmd->file_type = filetypes_detect_from_file(node->data);
							parse_intercept_actions(*handler_key, config, cmd);

							if (!cmd->invalid) {
								// add the command backwards
								autorun_globals->filedef_commands = g_slist_prepend(autorun_globals->filedef_commands, cmd);
							} else {
								// free the command
								autorun_cmd_free(cmd);
							}
						}
					}
					// flip the GSList around
					autorun_globals->filedef_commands = g_slist_reverse(autorun_globals->filedef_commands);
				} else {
					g_free(gerr);
				}
				g_key_file_free(config);
				g_free(filedef_file);
				g_free(handlers);
			}
		}
	}

	// cleanup
	g_slist_free_full(file_list, (GDestroyNotify)g_free);
	g_free(filedef_path);
}

/* helper function to process Project .geany files
 * to load any [autorun] handlers */
void load_projectdefs(GKeyFile* config) {
	gsize key_len;
	GError* gerr = NULL;
	gchar** handlers = g_key_file_get_keys(config, "autorun", &key_len, &gerr);
	// if the keyfile has an [autorun] section
	if (!gerr) {
		gchar** handler_key;
		// load any handlers
		foreach_strv(handler_key, handlers) {
			// over any existing [autorun] handlers
			if (g_str_has_suffix(*handler_key, "CM")) {
				AUTORUN_CMD* cmd = autorun_cmd_new();
				parse_intercept_actions(*handler_key, config, cmd);
				if (!cmd->invalid) {
					// add the command backwards
					autorun_globals->project_commands = g_slist_prepend(autorun_globals->project_commands, cmd);
				} else {
					// free the command
					autorun_cmd_free(cmd);
				}
			}
		}
		// flip the GSList around
		autorun_globals->project_commands = g_slist_reverse(autorun_globals->project_commands);
	} else {
		g_free(gerr);
	}
	// The caller of g_key_file_get_keys takes ownership of the returned data, and is responsible for freeing it.
	g_free(handlers);
}
