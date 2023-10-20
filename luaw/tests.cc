#include "luaw.hh"

#include <cstring>

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

int main()
{
    const char* hello = "print('hello world!')";
    size_t sz = strlen(hello);
    luaw_dobuffer(L, (uint8_t *) hello, sz, "hello");
}