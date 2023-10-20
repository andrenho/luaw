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

std::string luaw_dump(lua_State* L, int index)
{
    switch (lua_type(L, index)) {
        case LUA_TNIL:
            return "nil";
        case LUA_TNUMBER: {
            lua_Number n = lua_tonumber(L, index);
            lua_Number diff = abs(n - round(n));
            if (diff < 0.000001)
                return std::to_string((int) n);
            else
                return std::to_string(n);
        }
        case LUA_TBOOLEAN:
            return lua_toboolean(L, index) ? "true" : "false";
        case LUA_TSTRING:
            return "\""s + lua_tostring(L, index) + "\"";
        case LUA_TTABLE:
            return "{}";  // TODO - also what about metamethods?
        case LUA_TFUNCTION:
            return "[&]";
        case LUA_TUSERDATA:
            return "{}";  // TODO
        case LUA_TTHREAD:
            return "[thread]";
        case LUA_TLIGHTUSERDATA: {
            char buf[30];
            snprintf(buf, sizeof buf, "(*%p)", lua_touserdata(L, index));
            return buf;
        }
    }
}

std::string luaw_dump_stack(lua_State* L)
{
    return "";  // TODO
}