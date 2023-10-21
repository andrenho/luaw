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

lua_State* luaw_newstate();

// file loading

void luaw_do(lua_State* L, uint8_t* data, size_t sz, int nresults=0, std::string const& name="anonymous");
void luaw_do(lua_State* L, std::string const& buffer, int nresults=0, std::string const& name="anonymous");
void luaw_dofile(lua_State* L, std::string const& filename, int nresults=0, std::string const& name="anonymous");

// dump

std::string luaw_dump(lua_State* L, int index);
std::string luaw_dump_stack(lua_State* L);

// stack size

void luaw_ensure(lua_State* L, int expected_sz=0);

// stack management

template <typename T> void luaw_push(lua_State* L, T const& t);
template <typename T> void luaw_push(lua_State* L, T const* t);
template <typename T> bool luaw_is(lua_State* L, int index);
template <typename T> T luaw_to(lua_State* L, int index);
template <typename T> T luaw_to(lua_State* L, int index, T const& default_);
template <typename T> T luaw_pop(lua_State* L);

#include "luaw.inl"

#endif //LUAW_HH_