
/*
 * geanylua.c - Lua scripting plugin for the Geany IDE
 * Copyright 2007-2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 * Portions copyright Enrico Tröger <enrico.troeger(at)uvena(dot)de>
 * Portions copyright Nick Treleaven <nick.treleaven(at)btinternet(dot)com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * Some portions of this code were adapted from the Lua standalone
 * interpreter sources, and may be subject to the terms and conditions
 * specified under the Lua license. See the file COPYRIGHT.LUA for more
 * information, or visit  http://www.lua.org/license.html .
 */



#include "geany.h"
#include "plugindata.h"
#include "keybindings.h"
#include "ui_utils.h"

#include "geanyfunctions.h"

#include <glib/gi18n.h>

#include "glspi_ver.h"

#define SUPPORT_LIB "libgeanylua." G_MODULE_SUFFIX

#define CB_COPY(field) plugin_callbacks[i].field=glspi_geany_callbacks[i].field;

#define GETSYM(name,ptr) ( g_module_symbol(libgeanylua, name, (gpointer) &ptr) && ptr )

#define MKPATH(dir) g_build_path(G_DIR_SEPARATOR_S, dir, "plugins", "geanylua", SUPPORT_LIB, NULL);

PLUGIN_EXPORT
PLUGIN_VERSION_CHECK(MY_GEANY_API_VER)

PLUGIN_EXPORT
PLUGIN_SET_INFO(PLUGIN_NAME, PLUGIN_DESC, PLUGIN_VER, PLUGIN_AUTHOR)

PLUGIN_EXPORT
GeanyFunctions *geany_functions;

PLUGIN_EXPORT
GeanyKeyGroup plugin_key_group[1];




typedef void (*InitFunc) (GeanyData *data, GeanyFunctions *functions, GeanyKeyGroup *kg);
typedef void (*ConfigFunc) (GtkWidget *parent);
typedef void (*CleanupFunc) (void);


static gchar **glspi_version = NULL;
static InitFunc glspi_init = NULL;
static ConfigFunc glspi_configure = NULL;
static CleanupFunc glspi_cleanup = NULL;
static PluginCallback *glspi_geany_callbacks = NULL;



/*
  It seems to me like we could simply pass the callbacks pointer directly
  from the support library to the application. But for some reason that
  doesn't work at all. So we make a copy of the callbacks array here,
  and all is well...
*/
PLUGIN_EXPORT
PluginCallback	plugin_callbacks[8] = {
	{NULL, NULL, FALSE, NULL},
	{NULL, NULL, FALSE, NULL},
	{NULL, NULL, FALSE, NULL},
	{NULL, NULL, FALSE, NULL},
	{NULL, NULL, FALSE, NULL},
	{NULL, NULL, FALSE, NULL},
	{NULL, NULL, FALSE, NULL},
	{NULL, NULL, FALSE, NULL}
};

static void copy_callbacks(void)
{
	gint i;
	for (i=0; glspi_geany_callbacks[i].signal_name; i++) {
		CB_COPY(signal_name);
		CB_COPY(callback);
		CB_COPY(after);
		CB_COPY(user_data);
	}
}



static GModule *libgeanylua = NULL;


static void fail_init(void) {
	if (libgeanylua) { g_module_close(libgeanylua); }
	libgeanylua = NULL;
	glspi_version = NULL;
	glspi_init = NULL;
	glspi_configure = NULL;
	glspi_cleanup = NULL;
	glspi_geany_callbacks = NULL;
	plugin_callbacks[0].signal_name=NULL;
	plugin_callbacks[0].callback=NULL;
	plugin_callbacks[0].after=FALSE;
	plugin_callbacks[0].user_data=NULL;
}

static GeanyData *geany_data=NULL;



PLUGIN_EXPORT
void plugin_init(GeanyData *data)
{
	gchar *libname=NULL;

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	geany_data=data;
	libname=MKPATH(data->app->configdir);
	if ( !g_file_test(libname,G_FILE_TEST_IS_REGULAR) ) {
		g_free(libname);
		libname=MKPATH(data->app->datadir);
	}
	if ( !g_file_test(libname,G_FILE_TEST_IS_REGULAR) ) {
		g_free(libname);
		libname=NULL;
		g_printerr(_("%s: Can't find support library!\n"), PLUGIN_NAME);
		return;
	}
	libgeanylua=g_module_open(libname,0);
	g_free(libname);
	if (!libgeanylua) {
		g_printerr("%s\n", g_module_error());
		g_printerr(_("%s: Can't load support library!\n"), PLUGIN_NAME);
		return;
	}
	if ( !(
		GETSYM("glspi_version", glspi_version) &&
		GETSYM("glspi_init", glspi_init) &&
		GETSYM("glspi_configure", glspi_configure) &&
		GETSYM("glspi_cleanup", glspi_cleanup) &&
		GETSYM("glspi_geany_callbacks", glspi_geany_callbacks)
	)) {
		g_printerr("%s\n", g_module_error());
		g_printerr(_("%s: Failed to initialize support library!\n"), PLUGIN_NAME);
		fail_init();
		return;
	}
	if (!g_str_equal(*glspi_version, VERSION)) {
		g_printerr(_("%s: Support library version mismatch: %s <=> %s\n"),
			PLUGIN_NAME, *glspi_version, VERSION);
		fail_init();
		return;
	}
	copy_callbacks();

	glspi_init(data, geany_functions, plugin_key_group);
}


PLUGIN_EXPORT
GtkWidget *plugin_configure(G_GNUC_UNUSED GtkDialog *dialog)
{
	if (glspi_configure) {
		glspi_configure(geany->main_widgets->window);
	} else {
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("The %s plugin failed to load properly.\n"
			"Please check your installation."), PLUGIN_NAME );
	}
	return NULL; /* FIXME */
}


PLUGIN_EXPORT
void plugin_cleanup(void)
{
	if (glspi_cleanup) { glspi_cleanup(); }
	if (libgeanylua) { g_module_close(libgeanylua); }
}
