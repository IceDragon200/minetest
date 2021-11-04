/**
 *
 * Proxy header for getting the correct lua header.
 *
 */
#pragma once

#include "config.h"

extern "C" {
#if USE_LUAU
#include <Luau/lua.h>
#include <Luau/lualib.h>
/* Aliases for some missing functions in the Luau ABI */
#define luaL_typerror luaL_typeerror
#elif USE_LUAJIT
#include <luajit.h>
#include <lua.h>
#include <lualib.h>
#else
#error "System LUA"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif
}
