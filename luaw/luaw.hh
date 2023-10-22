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

std::string luaw_dump(lua_State* L, int index, size_t max_depth=3, size_t current_depth=0);
std::string luaw_dump_stack(lua_State* L, size_t max_depth=3);
void luaw_print_stack(lua_State* L, size_t max_depth=3);

// stack size

void luaw_ensure(lua_State* L, int expected_sz=0);
int luaw_len(lua_State* L, int index);

// stack management

template <typename T> void luaw_push(lua_State* L, T const& t);
template <typename T> void luaw_push(lua_State* L, T const* t);
template <typename T> bool luaw_is(lua_State* L, int index);
template <typename T> T luaw_to(lua_State* L, int index);
template <typename T> T luaw_to(lua_State* L, int index, T const& default_);
template <typename T> T luaw_pop(lua_State* L);

// iteration

template <typename F> requires std::invocable<F&, lua_State*, int>         void luaw_ipairs(lua_State* L, int index, F fn);
template <typename F> requires std::invocable<F&, lua_State*, std::string> void luaw_spairs(lua_State* L, int index, F fn);
template <typename F> requires std::invocable<F&, lua_State*>              void luaw_pairs(lua_State* L, int index, F fn);

// fields

void luaw_getfield(lua_State* L, int index, std::string const& field);
bool luaw_hasfield(lua_State* L, int index, std::string const& field);
void luaw_setfield(lua_State* L, int index, std::string const& field);

template <typename T> T luaw_getfield(lua_State* L, int index, std::string const& field);
template <typename T> void luaw_setfield(lua_State* L, int index, std::string const& field, T const& t);

#include "luaw.inl"

#endif //LUAW_HH_