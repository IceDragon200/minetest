# Locate Luau library
# This module defines
#  LUAU_FOUND, if false, do not try to link to Luau
#  LUA_LIBRARY, where to find the lua library
#  LUA_INCLUDE_DIR, where to find lua.h
#
# This module is similar to FindLua51.cmake except that it finds Luau instead.

FIND_PATH(LUA_INCLUDE_DIR "lua.h"
	HINTS
	$ENV{LUA_DIR}
	PATH_SUFFIXES include/Luau
	PATHS
	~/Library/Frameworks
	/Library/Frameworks
	/sw # Fink
	/opt/local # DarwinPorts
	/opt/csw # Blastwave
	/opt
)

if(DEFINED LUA_INCLUDE_DIR)
  set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR}/Luau)
endif()

# Test if running on vcpkg toolchain
if(DEFINED VCPKG_TARGET_TRIPLET AND DEFINED VCPKG_APPLOCAL_DEPS)
	# On vcpkg luajit is 'lua51' and normal lua is 'lua'
	FIND_LIBRARY(LUAU_VM_LIBRARY
		NAMES "libLuau.VM.a"
		HINTS
		$ENV{LUA_DIR}
		PATH_SUFFIXES lib/Luau
	)

	FIND_LIBRARY(LUAU_COMPILER_LIBRARY
		NAMES "libLuau.Compiler.a"
		HINTS
		$ENV{LUA_DIR}
		PATH_SUFFIXES lib/Luau
	)
else()
	FIND_LIBRARY(LUAU_VM_LIBRARY
		NAMES "libLuau.VM.a"
		HINTS
		$ENV{LUA_DIR}
		PATH_SUFFIXES lib64/Luau lib/Luau
		PATHS
		~/Library/Frameworks
		/Library/Frameworks
		/sw
		/opt/local
		/opt/csw
		/opt
	)

	FIND_LIBRARY(LUAU_COMPILER_LIBRARY
		NAMES "libLuau.Compiler.a"
		HINTS
		$ENV{LUA_DIR}
		PATH_SUFFIXES lib64/Luau lib/Luau
		PATHS
		~/Library/Frameworks
		/Library/Frameworks
		/sw
		/opt/local
		/opt/csw
		/opt
	)

	FIND_LIBRARY(LUAU_AST_LIBRARY
		NAMES "libLuau.Ast.a"
		HINTS
		$ENV{LUA_DIR}
		PATH_SUFFIXES lib64/Luau lib/Luau
		PATHS
		~/Library/Frameworks
		/Library/Frameworks
		/sw
		/opt/local
		/opt/csw
		/opt
	)
endif()

SET(LUA_LIBRARY "${LUAU_VM_LIBRARY}")

INCLUDE(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LUAU_FOUND to TRUE if
# all listed variables exist
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Luau
	REQUIRED_VARS LUA_LIBRARY LUA_INCLUDE_DIR)

MARK_AS_ADVANCED(
	LUA_INCLUDE_DIR
	LUA_INCLUDE_DIRS
	LUA_LIBRARY
	LUAU_VM_LIBRARY
	LUAU_COMPILER_LIBRARY
	LUAU_AST_LIBRARY
)
