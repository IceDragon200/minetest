/**
 *
 * Proxy header for getting the correct lua header.
 *
 */
#pragma once

#include "config.h"

#if USE_LUAJIT
extern "C" {
#include <luajit.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
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
