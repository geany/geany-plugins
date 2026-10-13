/*
 * glspi_kfile.h - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */

/* Minimal interface to a GKeyFile object */

#ifndef __GLSPI_KFILE_H
#define __GLSPI_KFILE_H

typedef struct _LuaKeyFile
{
	const gchar*id;
	GKeyFile*kf;
	gboolean managed;
} LuaKeyFile;


typedef gint (*KeyfileAssignFunc) (lua_State *L, GKeyFile*kf);

void glspi_init_kfile_module(lua_State *L, KeyfileAssignFunc *func);
#endif
