/*
 *      codenavigation.h - this file is part of "codenavigation", which is
 *      part of the "geany-plugins" project.
 *
 *      Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
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

#ifndef CODENAVIGATION_H
#define CODENAVIGATION_H

/* First */
#include "geany.h"		/* for the GeanyApp data type */

/* Other includes */
#include "support.h"	/* for the _() translation macro (see also po/POTFILES.in) */
#include "editor.h"		/* for the declaration of the GeanyEditor struct, not strictly necessary
						   as it will be also included by plugindata.h */
#include "document.h"	/* for the declaration of the GeanyDocument struct */
#include "ui_utils.h"
#include "Scintilla.h"	/* for the SCNotification struct */

#include "keybindings.h"
#include "filetypes.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>

#include "switch_head_impl.h"

/* Last */
#include "plugindata.h"		/* this defines the plugin API */
#include "geanyfunctions.h"	/* this wraps geany_functions function pointers */

/* Debug flag */
/*#define CODE_NAVIGATION_DEBUG
*/

#define CODE_NAVIGATION_VERSION "0.1"

/* Log utilities */
#ifdef CODE_NAVIGATION_DEBUG
static void log_debug(const gchar* s, ...)
{
	gchar* format = g_strconcat("[CODENAV DEBUG] : ", s, "\n", NULL);
	va_list l;
	va_start(l, s);
	g_vprintf(format, l);
	g_free(format);
	va_end(l);
}

#define log_func() g_print("[CODENAV FUNC] : %s", __func__)	/*	NB : this needs the C99 standard, but is only
																used when debugging, so no problem :) */
#else
static void log_debug(const gchar* s, ...) {}
static void log_func() {}
#endif

/* IDs for keybindings */
enum
{
	KEY_ID_SWITCH_HEAD_IMPL,
	KEY_ID_GOTO_FILE,
	NB_KEY_IDS
};

/* Items for controlling geany */
extern GeanyPlugin		*geany_plugin;
extern GeanyData		*geany_data;
extern GeanyFunctions	*geany_functions;

extern GeanyKeyGroup *plugin_key_group;

#endif /* CODENAVIGATION_H */
