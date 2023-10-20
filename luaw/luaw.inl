#ifndef LUA_INL_
#define LUA_INL_

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

//
// CONCEPTS
//

template<typename T>
concept IntegerType = requires(T param)
{
    requires std::is_integral_v<T>;
};

template<typename T>
concept FloatingType = requires(T param)
{
    requires std::is_floating_point_v<T>;
};

//
// STACK MANAGEMENT (generic)
//

template <typename T> T luaw_to(lua_State* L, int index, T const& default_)
{
    if (lua_isnil(L, -1))
        return default_;
    else
        return luaw_to<T>(L, index);
}

template <typename T> T luaw_pop(lua_State* L)
{
    T t = luaw_to<T>(L, -1);
    lua_pop(L, 1);
    return t;
}

//
// STACK MANAGEMENT (specialization)
//

template<> void luaw_push(lua_State* L, bool const& t) { lua_pushboolean(L, t); }
template<> bool luaw_is<bool>(lua_State* L, int index) { return lua_isboolean(L, index); }
template<> bool luaw_to(lua_State* L, int index) { return lua_toboolean(L, index); }

#endif //LUA_INL_
