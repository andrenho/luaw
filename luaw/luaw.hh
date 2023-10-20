#ifndef LUAW_HH_
#define LUAW_HH_

#include <cstddef>
#include <cstdint>
#include <string>

struct lua_State* L;

// file loading

int luaw_dobuffer(lua_State* L, uint8_t* data, size_t sz, const char* name);

#endif //LUAW_HH_