/*
 * glspi_kfile.c - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */

/* Minimal interface to a GKeyFile object */

#include <glib.h>
#include <glib/gi18n.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define LUA_MODULE_NAME "keyfile"
#define MetaName "_g_key_file_metatable"


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

#define FAIL_KEYFILE_ARG(argnum) \
	(fail_arg_type(L,__FUNCTION__,argnum,LuaKeyFileType))


#define push_number(L,n) lua_pushnumber(L,(lua_Number)n)

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
											LUA_MODULE_NAME, func+6, type, adjust_argnum(L,argnum));
	lua_error(L);
	return 0;
}


#define KF_FLAGS G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS

static const gchar*LuaKeyFileType="GKeyFile";

typedef struct _LuaKeyFile
{
	const gchar*id;
	GKeyFile*kf;
	gboolean managed;
} LuaKeyFile;


typedef gint (*KeyfileAssignFunc) (lua_State *L, GKeyFile*kf);

static gint glspi_kfile_assign(lua_State *L, GKeyFile*kf)
{
	LuaKeyFile*k=(LuaKeyFile*)lua_newuserdata(L,sizeof(LuaKeyFile));
	k->id=LuaKeyFileType;
	k->kf=kf;
	luaL_getmetatable(L, MetaName);
	lua_setmetatable(L, -2);
	k->managed=FALSE;
	return 1;
}



static gint kfile_new(lua_State *L)
{
	LuaKeyFile*k=(LuaKeyFile*)lua_newuserdata(L,sizeof(LuaKeyFile));
	k->id=LuaKeyFileType;
	k->kf=g_key_file_new();
	luaL_getmetatable(L, MetaName);
	lua_setmetatable(L, -2);
	k->managed=TRUE;
	return 1;
}



static gint kfile_done(lua_State *L)
{
	LuaKeyFile*k;
	if (lua_isnil(L, 1)) { return 0; }
	k=(LuaKeyFile*)lua_touserdata(L,1);
	if ( (k->id!=LuaKeyFileType) || (!k->managed) ) { return 1; }
	g_key_file_free(k->kf);
	return 1;
}



static LuaKeyFile* tokeyfile(lua_State *L, gint argnum)
{
	LuaKeyFile* rv;
	if ( (lua_gettop(L)<argnum) || (!lua_isuserdata(L,argnum))) return NULL;
	rv=lua_touserdata(L,argnum);
	return (rv && (rv->id==LuaKeyFileType))?rv:NULL;
}



static gint kfile_data(lua_State *L)
{
	LuaKeyFile*k;

	gsize len=0;
	GError *err=NULL;

	if (lua_gettop(L)>1) {
		const gchar *data=NULL;
		if ((lua_gettop(L)<2)||(!lua_isstring(L,2))) {return FAIL_STRING_ARG(2); }
		data=lua_tolstring(L,2,&len);
		k=tokeyfile(L,1);
		if (!k) {return FAIL_KEYFILE_ARG(1); }
		g_key_file_load_from_data(k->kf, data, len, KF_FLAGS, &err);
		if (err) {
			lua_pushstring(L,err->message);
			g_error_free(err);
		} else {lua_pushnil(L);}
		return 1;
	} else {
		gchar *data=NULL;
		k=tokeyfile(L,1);
		if (!k) {return FAIL_KEYFILE_ARG(1); }
		data=g_key_file_to_data(k->kf,&len,&err);
		if (!err) {
			lua_pushlstring(L,data,len);
			g_free(data);
			return 1;
		} else {
			lua_pushnil(L);
			lua_pushstring(L,err->message);
			g_error_free(err);
			if (data) {g_free(data);}
			return 2;
		}
	}
}



/*
	Lua "closure" function to iterate through each string in an array of strings
*/
static gint strings_closure(lua_State *L)
{
	gchar **strings;
	gchar *string;
	gint i=lua_tonumber(L, lua_upvalueindex(2));
	strings=lua_touserdata(L,lua_upvalueindex(1));
	if (!strings) {return 0;}
	string=strings[i];
	if ( string ) {
		lua_pushstring(L,string);
		push_number(L, i+1);
		lua_pushvalue(L, -1);
		lua_replace(L, lua_upvalueindex(2));
		return 2;
	} else {
		g_strfreev(strings);
		return 0;
	}
}



/* Iterate through the group names of a keyfile */
static gint kfile_groups(lua_State* L)
{
	LuaKeyFile*k=NULL;
	gchar**groups=NULL;
	gsize len=0;
	k=tokeyfile(L,1);
	if (!k) {return FAIL_KEYFILE_ARG(1); }
	groups=g_key_file_get_groups(k->kf, &len);
	lua_pushlightuserdata(L,groups);
	push_number(L,0);
	lua_pushcclosure(L, &strings_closure, 2);
	return 1;
}



/* Iterate through the key names of a group*/
static gint kfile_keys(lua_State* L)
{
	LuaKeyFile*k=NULL;
	gchar**keylist=NULL;
	gsize len=0;
	const gchar*group=NULL;
	GError *err=NULL;
	if ((lua_gettop(L)<2)||(!lua_isstring(L,2))) {return FAIL_STRING_ARG(2); }
	group=lua_tostring(L,2);
	k=tokeyfile(L,1);
	if (!k) {return FAIL_KEYFILE_ARG(1); }
	keylist=g_key_file_get_keys(k->kf, group, &len,  &err);
	if (err) {
		g_error_free(err);
	}
	lua_pushlightuserdata(L,keylist);
	push_number(L,0);
	lua_pushcclosure(L, &strings_closure, 2);
	return 1;
}



static gint kfile_value(lua_State* L)
{
	const gchar *group=NULL;
	const gchar *key=NULL;
	const gchar *value=NULL;
	LuaKeyFile*k=NULL;
	GError *err=NULL;
	if (lua_gettop(L)>=4) {
		if (!lua_isstring(L,4)) {return FAIL_STRING_ARG(4);}
		value=lua_tostring(L,4);
	}
	if ((lua_gettop(L)<3)||(!lua_isstring(L,3))) {return FAIL_STRING_ARG(3); }
	key=lua_tostring(L,3);
	if (!lua_isstring(L,2)) {return FAIL_STRING_ARG(2); }
	group=lua_tostring(L,2);
	k=tokeyfile(L,1);
	if (!k) {return FAIL_KEYFILE_ARG(1); }

	if (value) {
		g_key_file_set_value(k->kf,group,key,value);
		return 0;
	} else {
		gchar *kfv=g_key_file_get_value(k->kf,group,key,&err);
		if (err) {
			g_error_free(err);
		}
		if (kfv) {
			lua_pushstring(L, kfv);
			g_free(kfv);
			return 1;
		} else {
			return 0;
		}
	}
}



#define str_or_nil(L,argnum) (lua_isstring(L,argnum)||lua_isnil(L,argnum))


static gint kfile_comment(lua_State* L)
{
	const gchar *group=NULL;
	const gchar *key=NULL;
	const gchar *comment=NULL;
	LuaKeyFile*k=NULL;
	GError *err=NULL;
	if (lua_gettop(L)>=4) {
		if (!lua_isstring(L,4)) {return FAIL_STRING_ARG(4);}
		comment=lua_tostring(L,4);
	}
	if ((lua_gettop(L)<3)||(!str_or_nil(L,3))) {return FAIL_STRING_ARG(3); }
	key=lua_tostring(L,3);
	if (!str_or_nil(L,2)) {return FAIL_STRING_ARG(2); }
	group=lua_tostring(L,2);
	k=tokeyfile(L,1);
	if (!k) {return FAIL_KEYFILE_ARG(1); }

	if (comment) {
		g_key_file_set_comment(k->kf,group,key,comment,&err);
		return 0;
	} else {
		gchar*kfc=g_key_file_get_comment(k->kf,group,key,&err);
		if (err) {
			g_error_free(err);
		}
		if (kfc) {
			lua_pushstring(L, kfc);
			g_free(kfc);
			return 1;
		} else {
			return 0;
		}
	}
}



static gint kfile_has(lua_State* L)
{
	const gchar *group=NULL;
	const gchar *key=NULL;
	gint argc=lua_gettop(L);
	gboolean hasit=FALSE;
	LuaKeyFile*k=NULL;
	GError*err=NULL;
	if (argc>=3) {
		if (lua_isstring(L,3)) {
			key=lua_tostring(L,3);
		} else {
			if (!lua_isnil(L,3)) { return FAIL_STRING_ARG(3); }
		}
	}
	if ((lua_gettop(L)<2)||(!lua_isstring(L,2))) { return FAIL_STRING_ARG(2); }
	group=lua_tostring(L,2);
	k=tokeyfile(L,1);
	if (!k) { return FAIL_KEYFILE_ARG(1); }
	if (key) {
		hasit=g_key_file_has_key(k->kf, group, key, &err);
	} else {
		hasit=g_key_file_has_group(k->kf, group);
	}
	lua_pushboolean(L,hasit);
	if (err) {
		g_error_free(err);
	}
	return 1;
}



static gint kfile_remove(lua_State* L)
{
	const gchar *group=NULL;
	const gchar *key=NULL;
	gint argc=lua_gettop(L);
	LuaKeyFile*k=NULL;
	GError*err=NULL;
	if (argc>=3) {
		if (lua_isstring(L,3)) {
			key=lua_tostring(L,3);
		} else {
			if (!lua_isnil(L,3)) { return FAIL_STRING_ARG(3); }
		}
	}
	if ((lua_gettop(L)<2)||(!lua_isstring(L,2))) { return FAIL_STRING_ARG(2); }
	group=lua_tostring(L,3);
	k=tokeyfile(L,1);
	if (!k) { return FAIL_KEYFILE_ARG(1); }
	if (key) {
		g_key_file_remove_key(k->kf, group, key, &err);
	} else {
		g_key_file_remove_group(k->kf, group, &err);
	}
	if (err) {
		g_error_free(err);
	}
	return 0;
}



static const struct luaL_Reg kfile_funcs[] = {
	{"new",     kfile_new},
	{"data",    kfile_data},
	{"groups",  kfile_groups},
	{"keys",    kfile_keys},
	{"value",   kfile_value},
	{"comment", kfile_comment},
	{"has",     kfile_has},
	{"remove",  kfile_remove},
	{NULL,NULL}
};




static gint luaopen_keyfile(lua_State *L)
{
	luaL_newmetatable(L, MetaName);
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_getmetatable(L, MetaName);
	lua_pushstring(L,"__gc");
	lua_pushcfunction(L,kfile_done);
	lua_rawset(L,-3);
	luaL_register(L, NULL, &kfile_funcs[1]);
	luaL_register(L, LUA_MODULE_NAME, kfile_funcs);
	return 0;
}





void glspi_init_kfile_module(lua_State *L, KeyfileAssignFunc *func)
{

*func=glspi_kfile_assign;
	luaopen_keyfile(L);
}



