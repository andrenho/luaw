#ifndef LUAW_HH_
#define LUAW_HH_

#include <cstddef>
#include <cstdint>
#include <string>

#include <stdexcept>

struct lua_State;

class LuaException : public std::runtime_error {
public:
    LuaException(lua_State* L, std::string const& what) : std::runtime_error(what), L(L) {}
    lua_State* L;
};

// file loading

void luaw_dobuffer(lua_State* L, uint8_t* data, size_t sz, std::string const& name="anonymous", int nresults=0);
void luaw_dobuffer(lua_State* L, std::string const& buffer, std::string const& name="anonymous", int nresults=0);
void luaw_dofile(lua_State* L, std::string const& filename, std::string const& name="anonymous", int nresults=0);

#endif //LUAW_HH_