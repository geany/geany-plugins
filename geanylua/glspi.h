
/*
 * glspi.h - Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 *
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <ctype.h>

#include "glspi_compat.h"

#include <geanyplugin.h>
#define main_widgets	geany->main_widgets

#include "glspi_ver.h"

#define tokenWordChars  "wordchars"
#define tokenRectSel "rectsel"
#define tokenBanner "banner"
#define tokenCaller "caller"
#define tokenScript "script"
#define tokenDirSep "dirsep"
#define tokenProject "project"

#define FAIL_STRING_ARG(argnum) \
	(glspi_fail_arg_type(L,__FUNCTION__,argnum,"string"))

#define FAIL_BOOL_ARG(argnum) \
	(glspi_fail_arg_type(L,__FUNCTION__,argnum,"boolean"))

#define FAIL_NUMERIC_ARG(argnum) \
	(glspi_fail_arg_type(L,__FUNCTION__,argnum,"number"))

#define FAIL_UNSIGNED_ARG(argnum) \
	(glspi_fail_arg_type(L,__FUNCTION__,argnum,"unsigned"))



#define FAIL_TABLE_ARG(argnum) \
	(glspi_fail_arg_type(L,__FUNCTION__,argnum,"table"))

#define FAIL_STR_OR_NUM_ARG(argnum) \
	(glspi_fail_arg_types(L,__FUNCTION__,argnum, "string", "number"))


#define DOC_REQUIRED \
	GeanyDocument *doc = document_get_current();\
	if (!(doc && doc->is_valid)) {return 0;}


#define SetTableValue(name,value,pusher) \
	lua_pushstring(L, name); \
	pusher(L, value); \
	lua_rawset(L,-3);

#define SetTableStr(name,value) SetTableValue(name,value,lua_pushstring)
#define SetTableBool(name,value) SetTableValue(name,value,lua_pushboolean)
#define SetTableNum(name,value) SetTableValue(name,(lua_Number)value,lua_pushnumber)

#define DEFAULT_MAX_EXEC_TIME 15


#define push_number(L,n) lua_pushnumber(L,(lua_Number)n)



extern GeanyData *glspi_geany_data;

#define geany_data glspi_geany_data


#ifdef NEED_FAIL_ARG_TYPE
/* Pushes an error message onto Lua stack if script passes a wrong arg type */
static gint glspi_fail_arg_type(lua_State *L, const gchar *func, gint argnum, const gchar *type)
{
	lua_pushfstring(L, _("Error in module \"%s\" at function %s():\n"
											" expected type \"%s\" for argument #%d\n"),
											LUA_MODULE_NAME, func+6, type, argnum);
	lua_error(L);
	return 0;
}
#endif


#ifdef NEED_FAIL_ARG_TYPES
/* Same as above, but for two overloaded types, eg "string" OR "number" */
static gint glspi_fail_arg_types(
		lua_State *L, const gchar *func, gint argnum, const gchar *type1, const gchar *type2)
{
	lua_pushfstring(L, _("Error in module \"%s\" at function %s():\n"
											" expected type \"%s\" or \"%s\" for argument #%d\n"),
											LUA_MODULE_NAME, func+6, type1, type2, argnum);
	lua_error(L);
	return 0;
}
#endif


#ifdef NEED_FAIL_ELEM_TYPE
/*Pushes an error message onto Lua stack if table contains wrong element type*/
static gint glspi_fail_elem_type(
		lua_State *L, const gchar *func, gint argnum, gint idx, const gchar *type)
{
	lua_pushfstring(L, _("Error in module \"%s\" at function %s():\n"
											" invalid table in argument #%d:\n"
											" expected type \"%s\" for element #%d\n"),
											LUA_MODULE_NAME, func+6, argnum, type, idx);
	lua_error(L);
	return 0;
}
#endif


typedef void (*GsDlgRunHook) (gboolean running, gpointer user_data);
typedef gint (*KeyfileAssignFunc) (lua_State *L, GKeyFile*kf);


/* application functions */
void glspi_init_app_funcs(lua_State *L, const gchar*script_dir);
/* basic dialog box functions */
void glspi_init_dlg_funcs(lua_State *L, GsDlgRunHook hook);
/* document functions */
void glspi_init_doc_funcs(lua_State *L);
void glspi_init_kfile_module(lua_State *L, KeyfileAssignFunc *func);
/* menu functions */
void glspi_init_mnu_funcs(lua_State *L);
/* editor functions */
void glspi_init_sci_funcs(lua_State *L);
/* custom dialogs module */
void glspi_init_gsdlg_module(lua_State *L, GsDlgRunHook hook, GtkWindow *toplevel);
void glspi_run_script(const gchar *script_file, gint caller, GKeyFile*proj, const gchar *script_dir);

/* Pass TRUE to create hashes, FALSE to destroy them */
void glspi_set_sci_cmd_hash(gboolean create);
void glspi_set_key_cmd_hash(gboolean create);

