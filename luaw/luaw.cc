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

static std::string luaw_dump_table(lua_State* L, int index, size_t max_depth, size_t current_depth)
{
    if (current_depth > max_depth)
        return "...";

    // TODO - also what about metamethods?
    bool found = false;
    std::stringstream ss;

    luaw_ipairs(L, index, [&](lua_State* L, int i) {
        ss << luaw_dump(L, -1, max_depth, current_depth) << ", ";
        found = true;
    });

    luaw_spairs(L, index, [&](lua_State* L, std::string key) {
        ss << key << "=" << luaw_dump(L, -1, max_depth, current_depth) << ", ";
        found = true;
    });

    if (found) {
        std::string s = ss.str();
        return "{ " + s.substr(0, s.length() - 2) + " }";
    } else {
        return "{}";
    }

}

std::string luaw_dump(lua_State* L, int index, size_t max_depth, size_t current_depth)
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
            return luaw_dump_table(L, index, max_depth, current_depth + 1);
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

std::string luaw_dump_stack(lua_State* L, size_t max_depth)
{
    std::stringstream ss;
    int sz = lua_gettop(L);

    for (int i = sz, j = -1; i > 0; --i, --j) {
        ss << i << " / " << j << ": " << luaw_dump(L, j, max_depth) << "\n";
    }

    return ss.str();
}

void luaw_print_stack(lua_State* L, size_t max_depth)
{
    printf("%s\n", luaw_dump_stack(L, max_depth).c_str());
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

template<> void luaw_push<bool>(lua_State* L, bool const& t) { lua_pushboolean(L, t); }
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

void luaw_getfield(lua_State* L, int index, std::string const& field)
{
    std::istringstream iss(field);
    std::string property;

    int top = lua_gettop(L);
    int levels = 0;

    while (std::getline(iss, property, '.')) {
        lua_getfield(L, index, property.c_str());
        int type = lua_type(L, -1);
        if (type == LUA_TNIL || (iss.peek() != EOF /* is not last */ && type != LUA_TTABLE)) {
            lua_settop(L, top);
            throw LuaException(L, "Field " + field + " not found.");
        }
        ++levels;
    }

    lua_insert(L, levels - 1);
    lua_pop(L, levels - 1);
}

bool luaw_isfield(lua_State* L, int index, std::string const& field)
{
    std::istringstream iss(field);
    std::string property;

    int top = lua_gettop(L);
    while (std::getline(iss, property, '.')) {
        lua_getfield(L, index, property.c_str());
        int type = lua_type(L, -1);
        if (type == LUA_TNIL || (iss.peek() != EOF /* is not last */ && type != LUA_TTABLE)) {
            lua_settop(L, top);
            return false;
        }
    }

    lua_settop(L, top);
    return true;
}

void luaw_setfield(lua_State* L, int index, std::string const& field)
{
    std::istringstream iss(field);
    std::string property;
    std::vector<std::string> ps;

    while (std::getline(iss, property, '.'))
        ps.push_back(property);

    int top = lua_gettop(L);
    int levels = 0;

    for (size_t i = 0; i < ps.size() - 1; ++i) {
        lua_getfield(L, index, ps.at(i).c_str());
        int type = lua_type(L, -1);
        if (type == LUA_TNIL || (iss.peek() != EOF /* is not last */ && type != LUA_TTABLE)) {
            lua_settop(L, top);
            throw LuaException(L, "Field " + field + " not found.");
        }
        ++levels;
    }

    lua_pushvalue(L, levels - 1);   // copy value
    lua_setfield(L, -levels, field.c_str());

    lua_settop(L, top);
}