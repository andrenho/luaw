#include "luaw.hh"

#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>

using namespace std::string_literals;

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // luaw_do

    const char* hello = "print('luaw_do const char* ok!')";
    size_t sz = strlen(hello);
    luaw_do(L, (uint8_t *) hello, sz, 0, "hello");

    luaw_do(L, "print('luaw_do string ok!')");

    try {
        luaw_do(L, "print(");
        assert(false);
    } catch (LuaException& e) {
        printf("expected error: %s\n", e.what());
    }

    try {
        luaw_do(L, "print(a[1])");
        assert(false);
    } catch (LuaException& e) {
        printf("expected error: %s\n", e.what());
    }

    // luaw_dump

    printf("---------------------\n");

    auto dump = [&](auto const& v) {
        luaw_do(L, "return "s + v, 1); printf("%s\n", luaw_dump(L, -1).c_str()); lua_pop(L, 1);
    };
    dump("nil");
    dump("42");
    dump("42.8");
    dump("true");
    dump("false");
    dump("function() end");
    dump("{ a=4, 'b', { 'c', 'd' } }");

    lua_pushlightuserdata(L, (void *) hello);
    printf("%s\n", luaw_dump(L, -1).c_str());
    lua_pop(L, 1);

    // luaw_dump_stack

    printf("---------------------\n");

    lua_pushstring(L, "abc");
    lua_pushinteger(L, 42);
    printf("%s\n", luaw_dump_stack(L).c_str());
    lua_pop(L, 2);

    // lua types
    luaw_push(L, true); assert(luaw_pop<bool>(L));
}