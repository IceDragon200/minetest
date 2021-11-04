#include "mt_lua.h"
#if USE_LUAU
#include <Luau/Luau/Compiler.h>
#include <Luau/Luau/Parser.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <stdio.h>
#include <string>
#include <optional>
#include <string_view>

#ifdef _WIN32
static std::wstring fromUtf8(const std::string& path)
{
    size_t result = MultiByteToWideChar(CP_UTF8, 0, path.data(), int(path.size()), nullptr, 0);
    LUAU_ASSERT(result);

    std::wstring buf(result, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.data(), int(path.size()), &buf[0], int(buf.size()));

    return buf;
}
#endif

// Snatched from LUAU
// https://github.com/Roblox/luau/blob/586bef6a4cffec80f7f2c7d4ed397aacc8fa9313/CLI/FileUtils.cpp
static std::optional<std::string> readFile(const std::string& name)
{
#ifdef _WIN32
    FILE* file = _wfopen(fromUtf8(name).c_str(), L"rb");
#else
    FILE* file = fopen(name.c_str(), "rb");
#endif

    if (!file)
        return std::nullopt;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    if (length < 0)
    {
        fclose(file);
        return std::nullopt;
    }
    fseek(file, 0, SEEK_SET);

    std::string result(length, 0);

    size_t read = fread(result.data(), 1, length, file);
    fclose(file);

    if (read != size_t(length))
        return std::nullopt;

    return result;
}

static int mt_luau_loadstring(lua_State* L, const std::string& source, const char* chunkname)
{
  try
  {
    std::string bytecode = Luau::compile(source);
    return luau_load(L, chunkname, bytecode.data(), bytecode.size());
  }
  catch (Luau::ParseError& e)
  {
    lua_pushfstring(L, "cannot %s %s: %s", "parse error", chunkname, e.what());
    return LUA_ERRSYNTAX;
  }
  catch (Luau::CompileError& e)
  {
    lua_pushfstring(L, "cannot %s %s: %s", "compile file", chunkname, e.what());
    return LUA_ERRSYNTAX;
  }
}

int mt_luaL_loadfile(lua_State* L, const char* cfilename)
{
  std::string filename(cfilename);
  std::optional<std::string> source = readFile(filename);

  if (source)
  {
    return mt_luau_loadstring(L, *source, cfilename);
  }

  lua_pushfstring(L, "cannot %s %s: %s", "loadfile", cfilename, "file not found or inaccessible");
  return LUA_ERRFILE;
}

int mt_luaL_loadbuffer(lua_State* L, const char* cbuff, size_t sz, const char *name)
{
  std::string buff(cbuff, sz);
  return mt_luau_loadstring(L, buff, name);
}
#endif
