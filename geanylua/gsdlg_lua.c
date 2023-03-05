/*
 * gsdlg_lua.c - Lua bindings for gsdlg.c
 *
 * Copyright 2007-2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
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
 *
 * Some portions of this code were adapted from the Lua standalone
 * interpreter sources, and may be subject to the terms and conditions
 * specified under the Lua license. See the file COPYRIGHT.LUA for more
 * information, or visit  http://www.lua.org/license.html .
 *
 *
 */



#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


#define GSDLG_ALL_IN_ONE
#include "gsdlg.h"

#define LUA_MODULE_NAME "dialog"
#define MetaName "_gsdlg_metatable"


#define SetTableValue(name,value,pusher) \
	lua_pushstring(L, name); \
	pusher(L, value); \
	lua_rawset(L,-3);

#define SetTableStr(name,value) SetTableValue(name,value,lua_pushstring)


#define FAIL_STRING_ARG(argnum) \
	(fail_arg_type(L,__FUNCTION__,argnum,"string"))


#define FAIL_BOOL_ARG(argnum) \
	(fail_arg_type(L,__FUNCTION__,argnum,"boolean"))


#define FAIL_TABLE_ARG(argnum) \
	(fail_arg_type(L,__FUNCTION__,argnum,"table"))


#define FAIL_DBOX_ARG(argnum) \
	(fail_arg_type(L,__FUNCTION__,argnum,"DialogBox"))


static const gchar*DialogBoxType="DialogBox";



typedef struct _DialogBox {
	const gchar*id;
	GtkDialog *dlg;
} DialogBox;


/* Subtract one from error argnum if we have implicit "self" */
static gint adjust_argnum(lua_State *L, gint argnum) {
	lua_Debug ar;
	if (lua_getstack(L, 0, &ar)){
		lua_getinfo(L, "n", &ar);
		if (g_str_equal(ar.namewhat, "method")) { return argnum-1; }
	}
	return argnum;
}



/* Pushes an error message onto Lua stack if script passes a wrong arg type */
static gint fail_arg_type(lua_State *L, const gchar *func, gint argnum, const gchar *type)
{
	lua_pushfstring(L, _("Error in module \"%s\" at function %s():\n"
											" expected type \"%s\" for argument #%d\n"),
											LUA_MODULE_NAME, func+5, type, adjust_argnum(L, argnum));
	lua_error(L);
	return 0;
}



/*Pushes an error message onto Lua stack if table contains wrong element type*/
static gint gsdl_fail_elem_type(
		lua_State *L, const gchar *func, gint argnum, gint idx, const gchar *type)
{
	lua_pushfstring(L, _("Error in module \"%s\" at function %s():\n"
											" invalid table in argument #%d:\n"
											" expected type \"%s\" for element #%d\n"),
											LUA_MODULE_NAME, func+5, argnum, type, idx);
	lua_error(L);
	return 0;
}



static DialogBox* todialog(lua_State *L, gint argnum)
{
	DialogBox*rv=lua_touserdata(L,argnum);
	return (rv && (DialogBoxType==rv->id))?rv:NULL;
}



#define DLG_REQUIRE \
	DialogBox*D=todialog(L,1); \
	if (!D) { return FAIL_DBOX_ARG(1); }



static gint gsdl_checkbox(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4)||!lua_isstring(L,4)) {return FAIL_STRING_ARG(4); }
	if (!lua_isboolean(L,3)) { return FAIL_BOOL_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_checkbox(D->dlg, lua_tostring(L,2),lua_toboolean(L,3),lua_tostring(L,4));
	return 0;
}



static gint gsdl_hr(lua_State *L)
{
	DLG_REQUIRE;
	gsdlg_hr(D->dlg);
	return 0;
}



static gint gsdl_text(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }	
	if (!(lua_isstring(L,3)||lua_isnil(L,3))) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_text(D->dlg, lua_tostring(L,2),lua_tostring(L,3),lua_tostring(L,4));
	return 0;
}



static gint gsdl_password(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }	
	if (!(lua_isstring(L,3)||lua_isnil(L,3))) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_password(D->dlg, lua_tostring(L,2),lua_tostring(L,3),lua_tostring(L,4));
	return 0;
}



static gint gsdl_file(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }	
	if (!(lua_isstring(L,3)||lua_isnil(L,3))) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_file(D->dlg, lua_tostring(L,2),lua_tostring(L,3),lua_tostring(L,4));
	return 0;
}



static gint gsdl_color(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }	
	if (!(lua_isstring(L,3)||lua_isnil(L,3))) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_color(D->dlg, lua_tostring(L,2),lua_tostring(L,3),lua_tostring(L,4));
	return 0;
}



static gint gsdl_font(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }	
	if (!(lua_isstring(L,3)||lua_isnil(L,3))) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_font(D->dlg, lua_tostring(L,2),lua_tostring(L,3),lua_tostring(L,4));
	return 0;
}


#define str_or_nil(argnum) (lua_isstring(L,argnum)||lua_isnil(L,argnum))

static gint gsdl_textarea(lua_State *L)
{
	GsDlgStr key=NULL;
	GsDlgStr value=NULL;
	GsDlgStr label=NULL;
	gint argc=lua_gettop(L);
	DLG_REQUIRE;
	if (argc>=4) {
		if (!str_or_nil(4)) { return FAIL_STRING_ARG(4); }	
		label=lua_tostring(L,4);
	}
	if (argc>=3) {
		if (!str_or_nil(3)) { return FAIL_STRING_ARG(3); }	
		value=lua_tostring(L,3);
	}
	if ((argc<2)||!lua_isstring(L,2)) {FAIL_STRING_ARG(2);}
	key=lua_tostring(L,2);
	gsdlg_textarea(D->dlg, key,value,label);
	return 0;
}



static gint gsdl_group(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }
	if (!lua_isstring(L,3)) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_group(D->dlg, lua_tostring(L,2),lua_tostring(L,3),lua_tostring(L,4));
	return 0;
}



static gint gsdl_select(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }
	if (!lua_isstring(L,3)) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_select(D->dlg, lua_tostring(L,2),lua_tostring(L,3),lua_tostring(L,4));
	return 0;
}



static gint gsdl_radio(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }
	if (!lua_isstring(L,3)) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_radio(D->dlg, lua_tostring(L,2),lua_tostring(L,3),lua_tostring(L,4));
	return 0;
}








static gint gsdl_option(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<4) || !lua_isstring(L,4)) { return FAIL_STRING_ARG(4); }
	if (!lua_isstring(L,3)) { return FAIL_STRING_ARG(3); }
	if (!lua_isstring(L,2)) { return FAIL_STRING_ARG(2); }
	gsdlg_option(D->dlg,
	lua_tostring(L,2),
	lua_tostring(L,3),
	lua_tostring(L,4));
	return 0;
}



static gint gsdl_label(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<2)||(!lua_isstring(L,2))) { return FAIL_STRING_ARG(2); }
	gsdlg_label(D->dlg,lua_tostring(L,2));
	return 0;
}



static gint gsdl_heading(lua_State *L)
{
	DLG_REQUIRE;
	if ((lua_gettop(L)<2)||(!lua_isstring(L,2))) { return FAIL_STRING_ARG(2); }
	gsdlg_heading(D->dlg,lua_tostring(L,2));
	return 0;
}



static gint gsdl_new(lua_State *L) {
	gint argc=lua_gettop(L);
	GsDlgStr title=NULL;
	GsDlgStr *btns;
	gint i,n;
	DialogBox*D;
	if (argc>=1) {
		if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
		title=lua_tostring(L,1);
	}
	if (argc>=2) {
		if (!lua_istable(L,2)) { return FAIL_TABLE_ARG(2); }
	}
	n=lua_objlen(L,2);
	for (i=1;i<=n; i++) {
		lua_rawgeti(L,2,i);
		if (!lua_isstring(L, -1)) {
			return gsdl_fail_elem_type(L, __FUNCTION__, 2, i, "string");
		}
		lua_pop(L, 1);
	}
	btns=g_malloc0(sizeof(gchar*)*(n+1));
	for (i=1;i<=n; i++) {
		lua_rawgeti(L,2,i);
		btns[i-1]=lua_tostring(L, -1);
		lua_pop(L, 1);
	}
	D=(DialogBox*)lua_newuserdata(L,sizeof(DialogBox));
	luaL_getmetatable(L, MetaName);
	lua_setmetatable(L, -2);
	D->id=DialogBoxType;
	D->dlg=gsdlg_new(title,btns);
	g_free(btns);
	return 1;
}



static void gsdl_hash_cb(gpointer key, gpointer value, gpointer L)
{
	SetTableStr((gchar*)key, (gchar*)value);
}


#define push_num(L,n) lua_pushnumber(L,(lua_Number)n)

static gint gsdl_run(lua_State *L)
{
	gint rv=-1;
	GHashTable *h;
	DLG_REQUIRE;
	h=gsdlg_run(D->dlg, &rv, L);
	push_num(L,rv+1);
	if (h) {
		lua_newtable(L);
		g_hash_table_foreach(h,gsdl_hash_cb,L);
		g_hash_table_destroy(h);
		return 2;
	}
	return 1;
}



static gint gsdl_done(lua_State *L)
{
	DialogBox*D;
	if (lua_isnil(L, 1)) { return 0; }
	D=(DialogBox*)lua_touserdata(L,1);
	if (D->id!=DialogBoxType) { return 1; }
	gtk_widget_destroy(GTK_WIDGET(D->dlg));
	return 1;
}




static const struct luaL_Reg gsdl_funcs[] = {
	{"new",      gsdl_new},
	{"run",      gsdl_run},
	{"label",    gsdl_label},	
	{"text",     gsdl_text},
	{"select",   gsdl_select},
	{"option",   gsdl_option},
	{"group",    gsdl_group},
	{"radio",    gsdl_radio},
	{"password", gsdl_password},
	{"heading",  gsdl_heading},
	{"checkbox", gsdl_checkbox},
	{"hr",       gsdl_hr},
	{"textarea", gsdl_textarea},
	{"file",     gsdl_file},
	{"color",    gsdl_color},
	{"font",     gsdl_font},
	{0,0}
};



#ifndef DIALOG_LIB
static
#endif
gint luaopen_dialog(lua_State *L)
{
	gtk_init(NULL,NULL);
	luaL_newmetatable(L, MetaName);
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);

	luaL_getmetatable(L, MetaName);
	lua_pushstring(L,"__gc");
	lua_pushcfunction(L,gsdl_done);
	lua_rawset(L,-3);

	luaL_register(L, NULL, &gsdl_funcs[1]);
	luaL_register(L, LUA_MODULE_NAME, gsdl_funcs);
	return 0;
}


#ifndef DIALOG_LIB
void glspi_init_gsdlg_module(lua_State *L, GsDlgRunHook hook, GtkWindow *toplevel)
{
	gsdlg_set_run_hook(hook);
	gsdlg_toplevel=toplevel;
	luaopen_dialog(L);
}
#endif

