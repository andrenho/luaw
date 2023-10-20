#include "luaw.hh"

#include <fstream>
#include <sstream>

using namespace std::string_literals;

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

void luaw_dobuffer(lua_State* L, uint8_t* data, size_t sz, std::string const& name, int nresults)
{
    int r = luaL_loadbuffer(L, (char const *) data, sz, name.c_str());
    if (r == LUA_ERRSYNTAX)
        throw LuaException(L, "Syntax error");
    else if (r == LUA_ERRMEM)
        throw LuaException(L, "Memory error");

    r = lua_pcall(L, 0, nresults, 0);
    if (r == LUA_ERRRUN)
        throw LuaException(L, "Runtime error: "s + lua_tostring(L, -1));
    else if (r == LUA_ERRMEM)
        throw LuaException(L, "Runtime memory error");
    else if (r == LUA_ERRERR)
        throw LuaException(L, "Error running the error message handler");
}

void luaw_dobuffer(lua_State* L, std::string const& buffer, std::string const& name, int nresults)
{
    luaw_dobuffer(L, (uint8_t *) buffer.data(), buffer.length(), name, nresults);
}

void luaw_dofile(lua_State* L, std::string const& filename, std::string const& name, int nresults)
{
    std::ifstream f(filename);
    if (!f.good())
        throw LuaException(L, "Could not open file '" + filename + "'");
    std::stringstream buffer;
    buffer << f.rdbuf();
    luaw_dobuffer(L, buffer.str(), name, nresults);
}