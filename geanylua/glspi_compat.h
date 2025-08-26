/* Lua compatibility functions */

#ifndef GLSPI_COMPAT
#define GLSPI_COMPAT 1

/*
 * Compatibility functions for Lua 5.1 and LuaJIT
 */
#if LUA_VERSION_NUM==501

#define lua_rawlen(L,i)               lua_objlen(L, (i))

void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup);

#endif /* LUAJIT_VERSION_NUM */


/*
 * Compatibility functions for Lua 5.1 only
 */
#if LUA_VERSION_NUM==501 && !defined LUAJIT_VERSION_NUM

void luaL_traceback (lua_State *L, lua_State *L1, const char *msg, int level);

#endif /* LUA_VERSION_NUM */


#endif /* GLSPI_COMPAT */
