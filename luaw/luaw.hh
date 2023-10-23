#ifndef LUAW_HH_
#define LUAW_HH_

#include <cstddef>
#include <cstdint>
#include <string>

#include <stdexcept>

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

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

void luaw_do_z(lua_State* L, struct LuaCompressedBytecode lcb[], bool keep_results=false);

template <typename T> T luaw_do(lua_State* L, std::string const& buffer, std::string const& name="anonymous");

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

void luaw_push(lua_State* L, lua_CFunction f);

// userdata

template<typename T, typename... Args> T* luaw_push_userdata(lua_State* L, Args... args);

// globals

template <typename T> T    luaw_getglobal(lua_State* L, std::string const& global);
template <typename T> void luaw_setglobal(lua_State* L, std::string const& global, T const& t);

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

// calls

template <typename T=nullptr_t> T luaw_call(lua_State* L, auto&&... args);
template <typename T=nullptr_t> T luaw_call_global(lua_State* L, std::string const& global, auto&&... args);
template <typename T=nullptr_t> T luaw_call_field(lua_State* L, int index, std::string const& field, auto&&... args);

void luaw_call_push(lua_State* L, int nresults, auto&&... args);
void luaw_call_push_global(lua_State* L, std::string const& global, int nresults, auto&&... args);
void luaw_call_push_field(lua_State* L, int index, std::string const& field, int nresults, auto&&... args);

// metatables

template<typename T> void luaw_set_metatable(lua_State* L, const luaL_Reg *reg);

// other

std::string luaw_to_string(lua_State* L, int index);

#include "luaw.inl"

#endif //LUAW_HH_