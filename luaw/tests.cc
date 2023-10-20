#include "luaw.hh"

#include <cassert>
#include <cstring>
#include <cstdio>

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

    lua_atpanic(L, [](lua_State *L) {
        fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
        return 0;
    });

    // luaw_dobuffer

    const char* hello = "print('luaw_dobuffer const char* ok!')";
    size_t sz = strlen(hello);
    luaw_dobuffer(L, (uint8_t *) hello, sz, "hello", 0);

    luaw_dobuffer(L, "print('luaw_dobuffer string ok!')");

    try {
        luaw_dobuffer(L, "print(");
        assert(false);
    } catch (LuaException& e) {
        printf("expected error: %s\n", e.what());
    }

    try {
        luaw_dobuffer(L, "print(a[1])");
        assert(false);
    } catch (LuaException& e) {
        printf("expected error: %s\n", e.what());
    }
}