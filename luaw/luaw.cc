#include "luaw.hh"

#include <fstream>
#include <sstream>
#include <functional>

#include <tgmath.h>
#include <zlib.h>

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
        luaL_error(L, msg.c_str());
    } else if (r == LUA_ERRMEM) {
        luaL_error(L, "Memory error");
    }

    r = lua_pcall(L, 0, nresults, 0);
    if (r == LUA_ERRRUN) {
        std::string msg = "Runtime error: "s + lua_tostring(L, -1);
        lua_pop(L, 1);
        luaL_error(L, msg.c_str());
    } else if (r == LUA_ERRMEM) {
        luaL_error(L, "Runtime memory error");
    } else if (r == LUA_ERRERR){
        luaL_error(L, "Error running the error message handler");
    }
}

struct LuaCompressedBytecode { unsigned long c, u; const char* f; unsigned char* data; };
void luaw_do_z(lua_State* L, LuaCompressedBytecode lcb[], bool keep_results)
{
    size_t i = 0;

    while (lcb[i].c != 0) {
        uint8_t result[lcb[i].u];
        uncompress(result, &lcb[i].u, lcb[i].data, lcb[i].c);
        luaw_do(L, result, lcb[i].u, keep_results ? LUA_MULTRET : 0, lcb[i].f);
        ++i;
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
        luaL_error(L, "Could not open file '%s'", filename.c_str());
    std::stringstream buffer;
    buffer << f.rdbuf();
    luaw_do(L, buffer.str(), nresults, name);
}

static std::string luaw_dump_table(lua_State* L, int index, bool pretty_print, size_t max_depth, size_t current_depth)
{
    std::string value = luaw_to_string(L, index);
    if (!value.starts_with("table: "))
        return value;

    if (current_depth > max_depth)
        return "{...}";

    bool found = false;
    std::stringstream ss;

    std::string prefix;
    if (pretty_print && current_depth > 0)
        prefix = std::string(current_depth * 2, ' ');

    luaw_ipairs(L, index, [&](lua_State* L, int) {
        ss << luaw_dump(L, -1, pretty_print, max_depth, current_depth) << ", ";
        found = true;
    });

    bool has_non_numeric_key = false;
    luaw_spairs(L, index, [&](lua_State* L, std::string const& key) {
        ss << prefix << key << "=" << luaw_dump(L, -1, pretty_print, max_depth, current_depth) << ", ";
        if (pretty_print)
            ss << "\n";
        found = true;
        has_non_numeric_key = true;
    });

    if (found) {
        std::string s = ss.str();
        if (pretty_print && has_non_numeric_key)
            return "{\n" + s.substr(0, s.length() - 2) + "\n" + prefix.substr(0, prefix.length() - 2) + "}";
        else
            return "{ " + s.substr(0, s.length() - 2) + " }";
    } else {
        return "{}";
    }

}

std::string luaw_dump(lua_State* L, int index, bool pretty_print, size_t max_depth, size_t current_depth)
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
            return (lua_toboolean(L, index) ? "true" : "false");
        case LUA_TSTRING:
            return "\""s + lua_tostring(L, index) + "\"";
        case LUA_TTABLE:
            return luaw_dump_table(L, index, pretty_print, max_depth, current_depth + 1);
        case LUA_TFUNCTION:
            return "[&]";
        case LUA_TUSERDATA:
            return "[# "s + luaw_to_string(L, index) + "]";
        case LUA_TTHREAD:
            return "[thread]";
        case LUA_TLIGHTUSERDATA: {
            char buf[30];
            snprintf(buf, sizeof buf, "(*%p)", lua_touserdata(L, index));
            return buf;
        }
        default:
            luaL_error(L, "Invalid lua type");
            return "";
    }
}

std::string luaw_dump_stack(lua_State* L, size_t max_depth)
{
    std::stringstream ss;
    int sz = lua_gettop(L);

    for (int i = sz, j = -1; i > 0; --i, --j) {
        ss << i << " / " << j << ": " << luaw_dump(L, j, false, max_depth) << "\n";
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
        luaL_error(L, "Stack size expected to be %d, but found to be %d", expected_sz, lua_gettop(L));
}

int luaw_len(lua_State* L, int index)
{
#if LUAW == JIT
    return lua_objlen(L, index);
#else
    return luaL_len(L, index);
#endif
}

template<> int luaw_push<bool>(lua_State* L, bool const& t) { lua_pushboolean(L, t); return 1; }
template<> bool luaw_is<bool>(lua_State* L, int index) { return lua_isboolean(L, index); }
template<> bool luaw_to_(lua_State* L, int index) { return lua_toboolean(L, index); }

template<nullptr_t> int luaw_push(lua_State* L, [[maybe_unused]] nullptr_t const& t=nullptr) { lua_pushnil(L); return 1; }
template<> bool luaw_is<nullptr_t>(lua_State* L, int index) { return lua_isnil(L, index); }
template<> nullptr_t luaw_to_([[maybe_unused]] lua_State* L, [[maybe_unused]] int index) { return nullptr; }

template<> int luaw_push(lua_State* L, std::string const& t) { lua_pushstring(L, t.c_str()); return 1; }
template<> bool luaw_is<std::string>(lua_State* L, int index) { return lua_isstring(L, index); }
template<> std::string luaw_to_<std::string>(lua_State* L, int index) { return lua_tostring(L, index); }

template<> int luaw_push(lua_State* L, const char* t) { lua_pushstring(L, t); return 1; }
template<> bool luaw_is<const char*>(lua_State* L, int index) { return lua_isstring(L, index); }
template<> const char* luaw_to_<const char*>(lua_State* L, int index) { return lua_tostring(L, index); }

template<> int luaw_push(lua_State* L, lua_CFunction const& f) { lua_pushcfunction(L, f); return 1; }
int luaw_push(lua_State* L, lua_CFunction f) { lua_pushcfunction(L, f); return 1; }

void luaw_getfield(lua_State* L, int index, std::string const& field)
{
    std::istringstream iss(field);
    std::string property;

    int top = lua_gettop(L);
    int levels = 0;

    lua_pushvalue(L, index);

    while (std::getline(iss, property, '.')) {
        lua_getfield(L, -1, property.c_str());
        int type = lua_type(L, -1);
        if (type == LUA_TNIL || (iss.peek() != EOF /* is not last */ && type != LUA_TTABLE)) {
            lua_settop(L, top);
            luaL_error(L, "Field '%s' not found.", field.c_str());
        }
        ++levels;
    }

    lua_insert(L, top + 1);
    lua_pop(L, levels);
}

bool luaw_hasfield(lua_State* L, int index, std::string const& field)
{
    std::istringstream iss(field);
    std::string property;

    int top = lua_gettop(L);

    lua_pushvalue(L, index);

    while (std::getline(iss, property, '.')) {
        lua_getfield(L, -1, property.c_str());
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

    lua_pushvalue(L, index);

    for (size_t i = 0; i < ps.size() - 1; ++i) {
        lua_getfield(L, -1, ps.at(i).c_str());
        int type = lua_type(L, -1);
        if (type == LUA_TNIL || (iss.peek() != EOF /* is not last */ && type != LUA_TTABLE)) {
            lua_settop(L, top);
            luaL_error(L, "Field '%s' not found.", field.c_str());
        }
        ++levels;
    }

    lua_pushvalue(L, - levels - 2);
    lua_setfield(L, -2, ps.at(ps.size() - 1).c_str());

    lua_settop(L, top - 1);
}

std::string luaw_to_string(lua_State* L, int index)
{
    lua_getglobal(L, "tostring");
    lua_pushvalue(L, index - 1);
    lua_call(L, 1, 1);
    return luaw_pop<std::string>(L);
}
