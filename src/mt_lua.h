/**
 *
 * Proxy header for getting the correct lua header.
 *
 */
#pragma once

#include "config.h"

#if USE_LUAU
/* LUAU uses C++ linkage */
#include <Luau/lua.h>
#include <Luau/lualib.h>

#define LUA_SIGNATURE "\033Lua"
#define LUA_RELEASE "Luau 5.1 (compat)"
#define LUA_ERRFILE (LUA_ERRERR+1)

/* Aliases for some missing functions in the Luau ABI */
#define luaL_typerror luaL_typeerror
#define mt_lua_newuserdata(state, size) lua_newuserdata(state, size, 0)
/* stolen from luajit */
#define lua_register(L,n,f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))
int mt_luaL_loadfile(lua_State* L, const char* filename);
int mt_luaL_loadbuffer(lua_State* L, const char* buff, size_t sz, const char *name);
#define mt_luaL_ref(L, idx) lua_ref(L, idx)
#define mt_luaL_unref(L, _idx, ref) lua_unref(L, ref)
#elif USE_LUAJIT
extern "C" {
#include <luajit.h>
#include <lua.h>
#include <lualib.h>
#define mt_lua_newuserdata lua_newuserdata
#define mt_luaL_loadfile luaL_loadfile
#define mt_luaL_loadbuffer luaL_loadbuffer
#define mt_luaL_ref luaL_ref
#define mt_luaL_unref luaL_unref
}
#else
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#define mt_lua_newuserdata lua_newuserdata
#define mt_luaL_loadfile luaL_loadfile
#define mt_luaL_loadbuffer luaL_loadbuffer
#define mt_luaL_ref luaL_ref
#define mt_luaL_unref luaL_unref
}
#endif
