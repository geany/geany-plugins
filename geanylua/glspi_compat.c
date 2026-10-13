/* Lua compatibility functions */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


/*
 * Compatibility functions for Lua 5.1 and LuaJIT
 */
#if LUA_VERSION_NUM==501

/* Adapted from Lua 5.2.0 */
void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
	luaL_checkstack(L, nup+1, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		int i;
		lua_pushstring(L, l->name);
		for (i = 0; i < nup; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -(nup+1));
		lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_settable(L, -(nup + 3));
	}
	lua_pop(L, nup);  /* remove upvalues */
}

#endif /* LUAJIT_VERSION_NUM */


/*
 * Compatibility functions for Lua 5.1 only
 */
#if LUA_VERSION_NUM==501 && !defined LUAJIT_VERSION_NUM

void luaL_traceback (lua_State *L, lua_State *L1, const char *msg,
				int level)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return;
	}
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 2);
	lua_call(L, 2, 1);

	return;
}

#endif /* LUA_VERSION_NUM */
