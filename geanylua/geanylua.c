
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


PLUGIN_EXPORT
PLUGIN_VERSION_CHECK(MY_GEANY_API_VER)

PLUGIN_EXPORT
PLUGIN_SET_INFO(PLUGIN_NAME, PLUGIN_DESC, PLUGIN_VER, PLUGIN_AUTHOR)

PLUGIN_EXPORT
GeanyFunctions *geany_functions;

PLUGIN_EXPORT
GeanyPlugin *geany_plugin;



typedef void (*InitFunc) (GeanyData *data, GeanyFunctions *functions, GeanyPlugin *plugin);
typedef void (*ConfigFunc) (GtkWidget *parent);
typedef void (*CleanupFunc) (void);


static gchar **glspi_version = NULL;
static guint *glspi_abi = NULL;
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
	glspi_abi = NULL;
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


static gchar *get_lib_dir(void)
{
#ifdef G_OS_WIN32
	gchar *install_dir, *result;
# if GLIB_CHECK_VERSION(2, 16, 0)
	install_dir = g_win32_get_package_installation_directory_of_module(NULL);
# else
	install_dir = g_win32_get_package_installation_directory(NULL, NULL);
# endif
	result = g_strconcat(install_dir, "\\lib", NULL);
	g_free(install_dir);
	return result;
#else
	return g_strdup(LIBDIR);
#endif
}



static gboolean load_support_lib(const gchar *libname)
{
	if ( !g_file_test(libname,G_FILE_TEST_IS_REGULAR) ) {
		return FALSE;
	}
	libgeanylua=g_module_open(libname,0);
	if (!libgeanylua) {
		g_printerr("%s\n", g_module_error());
		g_printerr(_("%s: Can't load support library %s!\n"), PLUGIN_NAME, libname);
		return FALSE;
	}
	if ( !(
		GETSYM("glspi_version", glspi_version) &&
		GETSYM("glspi_abi", glspi_abi) &&
		GETSYM("glspi_init", glspi_init) &&
		GETSYM("glspi_configure", glspi_configure) &&
		GETSYM("glspi_cleanup", glspi_cleanup) &&
		GETSYM("glspi_geany_callbacks", glspi_geany_callbacks)
	)) {
		g_printerr("%s\n", g_module_error());
		g_printerr(_("%s: Failed to initialize support library %s!\n"), PLUGIN_NAME, libname);
		fail_init();
		return FALSE;
	}
	if (!g_str_equal(*glspi_version, VERSION)) {
		g_printerr(_("%s: Support library version mismatch: %s for %s (should be %s)!\n"),
			PLUGIN_NAME, *glspi_version, libname, VERSION);
		fail_init();
		return FALSE;
	}
	if (*glspi_abi != GEANY_ABI_VERSION) {
		g_printerr(_("%s: Support library ABI mismatch: %s for %s (should be %s)!\n"),
			PLUGIN_NAME, *glspi_abi, libname, GEANY_ABI_VERSION);
		fail_init();
		return FALSE;
	}
	if (geany->app->debug_mode) {
		g_printerr("%s: Using support library path: %s\n", PLUGIN_NAME, libname);
	}
	return TRUE;
}


PLUGIN_EXPORT
void plugin_init(GeanyData *data)
{
	gchar *libname=NULL;

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	geany_data=data;
	/* first try the user config path */
	libname=g_build_path(G_DIR_SEPARATOR_S, data->app->configdir, "plugins", "geanylua", SUPPORT_LIB, NULL);
	if (!load_support_lib(libname)) {
		/* try the system path */
		gchar *libdir=get_lib_dir();
		g_free(libname);
		libname=g_build_path(G_DIR_SEPARATOR_S, libdir, "geany-plugins", "geanylua", SUPPORT_LIB, NULL);
		g_free(libdir);
		if (!load_support_lib(libname)) {
			g_printerr(_("%s: Can't find support library %s!\n"), PLUGIN_NAME, libname);
			g_free(libname);
			return;
		}
	}
	g_free(libname);
	copy_callbacks();

	glspi_init(data, geany_functions, geany_plugin);
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
