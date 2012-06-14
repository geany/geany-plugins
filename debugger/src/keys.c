/*
 *      keys.c
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

/*
 * 		Contains hotkeys definition and handlers.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

extern GeanyFunctions	*geany_functions;
extern GeanyPlugin		*geany_plugin;

#include "keys.h"
#include "callbacks.h"

/* geany debugger key group */
struct GeanyKeyGroup* key_group;

/* type to hold information about a hotkey */
typedef struct _keyinfo {
	const char* key_name;
	const char* key_label;
	enum KEYS key_id;
} keyinfo; 

/* hotkeys list */
keyinfo keys[] = {
	{ "key_debug_run", N_("Run / Continue"), KEY_RUN},
	{ "key_debug_stop", N_("Stop"), KEY_STOP},
	{ "key_debug_restart", N_("Restart"), KEY_RESTART},
	{ "key_debug_step_into", N_("Step into"), KEY_STEP_INTO},
	{ "key_debug_step_over", N_("Step over"), KEY_STEP_OVER},
	{ "key_debug_step_out", N_("Step out"), KEY_STEP_OUT},
	{ "key_debug_exec_until", N_("Run to cursor"), KEY_EXECUTE_UNTIL},
	{ "key_debug_break", N_("Add / Remove breakpoint"), KEY_BREAKPOINT},
	{ "key_current_instruction", N_("Jump to the currect instruction"), KEY_CURRENT_INSTRUCTION},
	{ NULL, NULL, 0}
};

/* 
 * init hotkeys
 */
gboolean keys_init(void)
{
	int _index, count;

	/* keys count */
	count = 0;
	while (keys[count++].key_name)
		;
	
	/* set keygroup */
	key_group = plugin_set_key_group(
		geany_plugin,
		_("Debug"),
		count - 1,
		keys_callback);

	/* add keys */
	_index = 0;
	while (keys[_index].key_name)
	{
		keybindings_set_item(
			key_group,
			keys[_index].key_id,
			NULL,
			0,
			0,
			keys[_index].key_name,
			_(keys[_index].key_label),
			NULL);
		_index++;
	}
	 	
	return 1;
}

