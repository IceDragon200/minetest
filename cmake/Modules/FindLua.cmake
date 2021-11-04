# Look for Lua library to use
# This selects prioritizes LuaJIT by default

option(ENABLE_LUAJIT "Enable LuaJIT support" TRUE)
option(REQUIRE_LUAJIT "Require LuaJIT support" FALSE)

option(ENABLE_LUAU "Enable Luau support" FALSE)
option(REQUIRE_LUAU "Require Luau support" FALSE)

set(USE_LUAJIT FALSE)

if(REQUIRE_LUAJIT AND REQUIRE_LUAU)
	message(FATAL_ERROR "LuaJIT and Luau were required, but only one can be used.")
endif()

if(REQUIRE_LUAJIT)
	set(ENABLE_LUAJIT TRUE)
endif()

if(REQUIRE_LUAU)
	set(ENABLE_LUAU TRUE)
endif()

if(ENABLE_LUAJIT)
	find_package(LuaJIT)

	if(LUAJIT_FOUND)
		set(USE_LUAJIT TRUE)
		message (STATUS "Using LuaJIT provided by system.")
	elseif(REQUIRE_LUAJIT)
		message(FATAL_ERROR "LuaJIT not found whereas REQUIRE_LUAJIT=\"TRUE\" is used.\n"
			"To continue, either install LuaJIT or do not use REQUIRE_LUAJIT=\"TRUE\".")
	endif()
else()
	message (STATUS "LuaJIT detection disabled! (ENABLE_LUAJIT=0)")
endif()

if(ENABLE_LUAU)
	find_package(Luau)

	if(LUAU_FOUND)
		set(USE_LUAU TRUE)
		message(STATUS "Using Luau provided by system.")
	elseif(REQUIRE_LUAU)
		message(FATAL_ERROR "Luau not found whereas REQUIRE_LUAU=\"TRUE\" is used.\n"
												"To continue, either install Luau or do not use REQUIRE_LUAU=\"TRUE\".")
	endif()
else()
	message (STATUS "Luau detection disabled! (ENABLE_LUAU=0)")
endif()

if(NOT USE_LUAJIT AND NOT USE_LUAU)
	message(STATUS "Neither LuaJIT nor Luau were found, using bundled Lua.")

	set(LUA_LIBRARY lua)
	set(LUA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/lua/src)
	add_subdirectory(lib/lua)
endif()
