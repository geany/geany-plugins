/*
 *  codenavigation.h - this file is part of "codenavigation", which is
 *  part of the "geany-plugins" project.
 *
 *  Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
 *  Copyright 2014 Federico Reghenzani <federico(dot)dev(at)reghe(dot)net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CODENAVIGATION_H
#define CODENAVIGATION_H

/* First */
#include <geanyplugin.h>

/* Other includes */
#include "Scintilla.h"	/* for the SCNotification struct */

#include <gdk/gdkkeysyms.h>

#include <string.h>

#include "switch_head_impl.h"

/* Debug flag */
/*#define CODE_NAVIGATION_DEBUG*/

#define CODE_NAVIGATION_VERSION "0.2"

/* Log utilities */
#ifdef CODE_NAVIGATION_DEBUG
#include <glib/gprintf.h>


static void log_debug(const gchar* s, ...)
{
	gchar* format = g_strconcat("[CODENAV DEBUG] : ", s, "\n", NULL);
	va_list l;
	va_start(l, s);
	g_vprintf(format, l);
	g_free(format);
	va_end(l);
}

#define log_func() g_print("[CODENAV FUNC] : %s\n", G_STRFUNC)
#else
#define log_debug(...) {}
#define log_func() {}
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

extern GeanyKeyGroup *plugin_key_group;

#endif /* CODENAVIGATION_H */
