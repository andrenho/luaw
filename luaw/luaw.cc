#include "luaw.hh"

#include <fstream>
#include <sstream>
#include <functional>

using namespace std::string_literals;

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

static const char* strict_lua = R"(
local getinfo, error, rawset, rawget = debug.getinfo, error, rawset, rawget

local mt = getmetatable(_G)
if mt == nil then
    mt = {}
    setmetatable(_G, mt)
end

mt.__declared = {}

local function what ()
    local d = getinfo(3, "S")
    return d and d.what or "C"
end

mt.__newindex = function (t, n, v)
    if not mt.__declared[n] then
        local w = what()
        if w ~= "main" and w ~= "C" then
            error("assign to undeclared variable '"..n.."'", 2)
        end
        mt.__declared[n] = true
    end
    rawset(t, n, v)
end

mt.__index = function (t, n)
    if not mt.__declared[n] and what() ~= "C" then
        error("variable '"..n.."' is not declared", 2)
    end
    return rawget(t, n)
end
)";

lua_State* luaw_newstate()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaw_do(L, strict_lua, 0, "strict.lua");
    return L;
}

void luaw_do(lua_State* L, uint8_t* data, size_t sz, int nresults, std::string const& name)
{
    int r = luaL_loadbuffer(L, (char const *) data, sz, name.c_str());
    if (r == LUA_ERRSYNTAX) {
        std::string msg = "Syntax error: "s + lua_tostring(L, -1);
        lua_pop(L, 1);
        throw LuaException(L, msg);
    } else if (r == LUA_ERRMEM) {
        throw LuaException(L, "Memory error");
    }

    r = lua_pcall(L, 0, nresults, 0);
    if (r == LUA_ERRRUN) {
        std::string msg = "Runtime error: "s + lua_tostring(L, -1);
        lua_pop(L, 1);
        throw LuaException(L, msg);
    } else if (r == LUA_ERRMEM) {
        throw LuaException(L, "Runtime memory error");
    } else if (r == LUA_ERRERR){
        throw LuaException(L, "Error running the error message handler");
    }
}

void luaw_do(lua_State* L, std::string const& buffer, int nresults, std::string const& name)
{
    luaw_do(L, (uint8_t *) buffer.data(), buffer.length(), nresults, name);
}

void luaw_dofile(lua_State* L, std::string const& filename, int nresults, std::string const& name)
{
    std::ifstream f(filename);
    if (!f.good())
        throw LuaException(L, "Could not open file '" + filename + "'");
    std::stringstream buffer;
    buffer << f.rdbuf();
    luaw_do(L, buffer.str(), nresults, name);
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
        default:
            throw LuaException(L, "Invalid lua type");
    }
}

std::string luaw_dump_stack(lua_State* L)
{
    std::stringstream ss;
    int sz = lua_gettop(L);

    for (int i = sz, j = -1; i > 0; --i, --j) {
        ss << i << " / " << j << ": " << luaw_dump(L, j) << "\n";
    }

    return ss.str();
}

void luaw_ensure(lua_State* L, int expected_sz)
{
    if (lua_gettop(L) != expected_sz)
        throw LuaException(L, "Stack size expected " + std::to_string(expected_sz) + ", but found to be " + std::to_string(
                lua_gettop(L)));
}

int luaw_len(lua_State* L, int index)
{
#if LUAW == JIT
    return lua_objlen(L, index);
#else
    return luaL_len(L, index);
#endif
}

template<> void luaw_push(lua_State* L, bool const& t) { lua_pushboolean(L, t); }
template<> bool luaw_is<bool>(lua_State* L, int index) { return lua_isboolean(L, index); }
template<> bool luaw_to(lua_State* L, int index) { return lua_toboolean(L, index); }

template<nullptr_t> void luaw_push(lua_State* L, [[maybe_unused]] nullptr_t const& t=nullptr) { lua_pushnil(L); }
template<> bool luaw_is<nullptr_t>(lua_State* L, int index) { return lua_isnil(L, index); }
template<> nullptr_t luaw_to([[maybe_unused]] lua_State* L, [[maybe_unused]] int index) { return nullptr; }

template<> void luaw_push(lua_State* L, std::string const& t) { lua_pushstring(L, t.c_str()); }
template<> bool luaw_is<std::string>(lua_State* L, int index) { return lua_isstring(L, index); }
template<> std::string luaw_to<std::string>(lua_State* L, int index) { return lua_tostring(L, index); }

template<> void luaw_push(lua_State* L, const char* t) { lua_pushstring(L, t); }
template<> bool luaw_is<const char*>(lua_State* L, int index) { return lua_isstring(L, index); }
template<> const char* luaw_to<const char*>(lua_State* L, int index) { return lua_tostring(L, index); }