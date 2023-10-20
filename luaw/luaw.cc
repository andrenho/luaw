#include "luaw.hh"

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

int luaw_dobuffer(lua_State* L, uint8_t* data, size_t sz, const char* name)
{
    return (luaL_loadbuffer(L, (char const *) data, sz, name) || lua_pcall(L, 0, LUA_MULTRET, 0));
}