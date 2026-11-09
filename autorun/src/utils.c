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

/* Read a filedef or keyfile for Intercept Actions
 * key which interceptor to read (should end with CM)
 * key_file to get remaining values from
 * cmd AUTORUN_CMD struct to store data to */
void parse_intercept_actions(gchar* key, GKeyFile* key_file, AUTORUN_CMD* cmd) {
	// key should be a (filetype_)action_number_flag string
	if (!key || !cmd) {
		return;
	}
	// is this a key?
	if (!g_strstr_len(key, -1, "_")) {
		// no
		cmd->invalid = TRUE;
		return;
	}
	cmd->invalid = FALSE;
	// what is this key?
	gchar** tokens;
	tokens = g_strsplit(key, "_", 4);

	switch (g_strv_length(tokens)) {
		// strv of 3 is filedef, 4 is project
	case 3: {
		// the caller has to set filetype
		if (g_strcmp0(tokens[0], "BS") == 0) {
			cmd->interceptor = g_strdup(tokens[0]);
		} else if (g_strcmp0(tokens[0], "OS") == 0) {
			cmd->interceptor = g_strdup(tokens[0]);
		} else {
			cmd->invalid = TRUE;
		}

		gint offset = atoi(tokens[1]);
		if (offset < 0 || offset > 99) {
			cmd->invalid = TRUE;
		} else {
			cmd->order = offset;
		}
		if (g_strcmp0(tokens[2], "CM") == 0) {
			cmd->command = utils_get_setting_string(key_file, "autorun", key, "");
			if (strlen(cmd->command) < 2) {
				// GKeyFile doesnt actually delete keys, but sets them to ""
				cmd->invalid = TRUE;
				break;
			}
			gchar* next_key;
			next_key = g_strconcat(tokens[0], "_", tokens[1], "_", "WD", NULL);
			cmd->working_dir = utils_get_setting_string(key_file, "autorun", next_key, "");
			g_free(next_key);
		} else {
			cmd->invalid = TRUE;
		}
		break;
	}
	case 4: {
		cmd->file_type = filetypes_lookup_by_name(tokens[0]);

		if (g_strcmp0(tokens[1], "BS") == 0) {
			cmd->interceptor = g_strdup(tokens[1]);
		} else if (g_strcmp0(tokens[1], "OS") == 0) {
			cmd->interceptor = g_strdup(tokens[1]);
		} else {
			cmd->invalid = TRUE;
		}

		gint offset = atoi(tokens[2]);
		if (offset < 0 || offset > 99) {
			cmd->invalid = TRUE;
		} else {
			cmd->order = offset;
		}

		if (g_strcmp0(tokens[3], "CM") == 0) {
			cmd->command = utils_get_setting_string(key_file, "autorun", key, "");
			gchar* next_key;
			next_key = g_strconcat(tokens[0], "_", tokens[1], "_", tokens[2], "_", "WD", NULL);
			cmd->working_dir = utils_get_setting_string(key_file, "autorun", next_key, "");
			g_free(next_key);
		} else {
			cmd->invalid = TRUE;
		}
		break;
	}
	default: {
		cmd->invalid = TRUE;
	}
	}

	g_strfreev(tokens);
}

/* split stdout and stderr into blocks and format them for Compiler
 * input the current data to parse */
void parse_output(gchar* input) {
	// make sure we're using utf8
	gchar* output = encodings_convert_to_utf8(input, -1, NULL);
	// split the output on \n
	gchar** lines = g_regex_split_simple("\\R", output, 0, G_REGEX_MATCH_NEWLINE_ANYCRLF);
	gchar** line = NULL;

	// foreach line, if it contains a filename, it is red.
	foreach_strv(line, lines) {
		guint i;
		gboolean found = FALSE;
		GeanyData* geany_data = autorun_globals->data;
		foreach_document(i) {
			GeanyDocument* doc_test = documents[i];
			gchar* locale_name = utils_get_locale_from_utf8(doc_test->file_name);
			gchar* base_test = g_path_get_basename(locale_name);
			if (g_strrstr(*line, base_test)) {
				// contains a known filename
				found = TRUE;
				break;
			}
			if (locale_name) {
				g_free(locale_name);
			}
			if (base_test) {
				g_free(base_test);
			}
		}
		/* make known filenames clicky (perhaps)
		 *  the compiler requires each newline to be its own message
		 *  or the compiler clicky breaks.
		 *  the compiler requires each clicky to be RED
		 *  or the compiler clicky breaks. */
		if (found) {
			msgwin_compiler_add_string(COLOR_RED, *line);
		} else {
			msgwin_compiler_add_string(COLOR_BLACK, *line);
		}
	}

	line = NULL;
	g_strfreev(lines);
	if (output) {
		g_free(output);
	}
}
/* attempt to escape a filename or directory so it is safe inside of
 * a platform specific quote
 * the caller owns the returned string */
gchar* escape_filename(gchar* filename) {
	GString* filename_str = g_string_new(filename);
#ifdef G_OS_WIN32
	/*the goal is that the output from this function should work inside of a
	double-quoted execve argument.
	 * From https://www.robvanderwoude.com/escapechars.php*/
	// % 	%%
	g_string_replace(filename_str, "%", "%%", 0);
	// ^ 	^^ 	May not always be required in doublequoted strings, but it won't hurt
	g_string_replace(filename_str, "^", "^^", 0);
	// & 	^&
	g_string_replace(filename_str, "&", "^&", 0);
	// < 	^<
	g_string_replace(filename_str, "<", "^<", 0);
	// > 	^>
	g_string_replace(filename_str, ">", "^>", 0);
	// | 	^|
	g_string_replace(filename_str, "|", "^|", 0);
#else
	/*the goal is that the output from this function should work inside of a
	single quoted execve argument.
	File: `'~!@#$%^&*()_-+={}[] \ |:;"<>?,.c (created through GUI)
	ls suggested
	'`'\''~!@#$%^&*()_-+={}[] \ |:;"<>?,.c'
	*/
	// '	'\''
	g_string_replace(filename_str, "'", "'\\''", 0);
#endif
	return g_string_free_and_steal(filename_str);
}
