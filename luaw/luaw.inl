#ifndef LUA_INL_
#define LUA_INL_

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

#include <tuple>

//
// CONCEPTS
//

template<typename T>
concept IntegerType = requires(T param)
{
    requires std::is_integral_v<T>;
    requires !std::is_pointer_v<T>;
};

template<typename T>
concept FloatingType = requires(T param)
{
    requires std::is_floating_point_v<T>;
};

template<typename T>
concept PointerType = requires(T param)
{
    requires std::is_pointer_v<T>;
    requires !std::is_same_v<T, const char*>;
};

template< typename T >
concept Optional = requires( T t )
{
    typename T::value_type;
    std::same_as< T, std::optional< typename T::value_type > >;
    t.value();
};

template <typename T>
concept Iterable = requires(T t) {
    begin(t);
    end(t);
    t.push_back(typename T::value_type{});
    requires !std::is_same_v<T, std::string>;
};


template<class P>
concept Pair = requires(P p) {
    typename P::first_type;
    typename P::second_type;
    p.first;
    p.second;
    requires std::same_as<decltype(p.first), typename P::first_type>;
    requires std::same_as<decltype(p.second), typename P::second_type>;
};

template<class T, std::size_t N>
concept has_tuple_element =
requires(T t) {
    typename std::tuple_element_t<N, std::remove_const_t<T>>;
    { get<N>(t) } -> std::convertible_to<const std::tuple_element_t<N, T>&>;
};

template<class T>
concept Tuple = !std::is_reference_v<T>
                     && requires(T t) {
    typename std::tuple_size<T>::type;
    requires std::derived_from<
            std::tuple_size<T>,
            std::integral_constant<std::size_t, std::tuple_size_v<T>>
    >;
    requires std::tuple_size_v<std::remove_cvref_t<T>> != 2;
} && []<std::size_t... N>(std::index_sequence<N...>) {
    return (has_tuple_element<T, N> && ...);
}(std::make_index_sequence<std::tuple_size_v<T>>());

//
// STACK MANAGEMENT
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
    T t = (T) luaw_to<T>(L, -1);
    lua_pop(L, 1);
    return t;
}

//
// STACK MANAGEMENT (specialization)
//

// integer

template <IntegerType T> void luaw_push(lua_State* L, T const& t) { lua_pushinteger(L, t); }
template <IntegerType T> bool luaw_is(lua_State* L, int index) {
    if (!lua_isnumber(L, index))
        return false;
    else
        return lua_tointeger(L, index) == lua_tointeger(L, index);
}
template <IntegerType T> T luaw_to(lua_State* L, int index) { return (T) lua_tointeger(L, index); }

// number

template <FloatingType T> void luaw_push(lua_State* L, T const& t) { lua_pushnumber(L, t); }
template <FloatingType T> bool luaw_is(lua_State* L, int index) { return lua_isnumber(L, index); }
template <FloatingType T> T luaw_to(lua_State* L, int index) { return (T) lua_tonumber(L, index); }

template <PointerType T> void luaw_push(lua_State* L, T t) { lua_pushlightuserdata(L, t); }
template <PointerType T> bool luaw_is(lua_State* L, int index) { return lua_isuserdata(L, index); }
template <PointerType T> T luaw_to(lua_State* L, int index) { return (T) lua_touserdata(L, index); }

// table (vector, set...)

template <Iterable T> void luaw_push(lua_State* L, T const& t) {
    lua_newtable(L);
    int i = 1;
    for (auto const& v : t) {
        luaw_push(L, v);
        lua_rawseti(L, -2, i++);
    }
}
template <Iterable T> bool luaw_is(lua_State* L, int index) { return lua_istable(L, index); }
template <Iterable T> T luaw_to(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    T ts;
    int sz = luaw_len(L, index);
    for (int i = 1; i <= sz; ++i) {
        lua_rawgeti(L, index, i);
        ts.push_back(luaw_to<typename T::value_type>(L, -1));
        lua_pop(L, 1);
    }
    return ts;
}

// optional

template <Optional T> void luaw_push(lua_State* L, T const& t) {
    if (t.has_value())
        luaw_push<typename T::value_type>(L, *t);
    else
        lua_pushnil(L);
}
template <Optional T> bool luaw_is(lua_State* L, int index) {
    return lua_isnil(L, index) || luaw_is<T::value_type>(L, index);
}
template <Optional T> T luaw_to(lua_State* L, int index) {
    if (lua_isnil(L, index))
        return T {};
    else
        return luaw_to<typename T::value_type>(L, index);
}

// pair

template <Pair T> void luaw_push(lua_State* L, T const& t) {
    lua_newtable(L);
    luaw_push(L, t.first);
    lua_rawseti(L, -2, 1);
    luaw_push(L, t.second);
    lua_rawseti(L, -2, 2);
}
template <Pair T> bool luaw_is(lua_State* L, int index) {
    return lua_type(L, index) == LUA_TTABLE && luaw_len(L, index) == 2;
}
template <Pair T> T luaw_to(lua_State* L, int index) {
    if (luaw_len(L, index) != 2)
        luaL_error(L, "Expected array of size 2.");
    luaL_checktype(L, index, LUA_TTABLE);

    lua_rawgeti(L, index, 1);
    typename T::first_type t = luaw_to<typename T::first_type>(L, -1);
    lua_pop(L, 1);

    lua_rawgeti(L, index, 2);
    typename T::second_type u = luaw_to<typename T::second_type>(L, -1);
    lua_pop(L, 1);

    return { t, u };
}

// tuple

template <Tuple T> void luaw_push(lua_State* L, T const& t) {
    lua_newtable(L);
    int i = 1;
    std::apply([L, &i](auto&&... args) { ((luaw_push(L, args), lua_rawseti(L, -2, i++)), ...); }, t);
}

template <typename T, std::size_t I = 0>
static bool tuple_element_is(lua_State* L)
{
    if constexpr (I < std::tuple_size_v<T>) {
        lua_rawgeti(L, -1, I + 1);
        bool is = luaw_is<std::tuple_element_t<I, T>>(L, -1);
        lua_pop(L, 1);
        if (!is)
            return false;
        else
            return tuple_element_is<T, I + 1>(L);
    }
    return true;
}

template <Tuple T> bool luaw_is(lua_State* L, int index) {
    bool is = lua_type(L, index) == LUA_TTABLE && luaw_len(L, index) == std::tuple_size_v<T>;
    if (is)
        return tuple_element_is<T>(L);
    else
        return false;
}

template <typename T, std::size_t I = 0>
static void tuple_element_set(lua_State* L, int index, T& t)
{
    if constexpr (I < std::tuple_size_v<T>) {
        lua_rawgeti(L, index, I + 1);
        std::get<I>(t) = luaw_pop<std::tuple_element_t<I, T>>(L);
        tuple_element_set<T, I + 1>(L, index, t);
    }
}

template <Tuple T> T luaw_to(lua_State* L, int index)
{
    T t;
    tuple_element_set<T>(L, index, t);
    return t;
}

//
// ITERATION
//

template <typename F> requires std::invocable<F&, lua_State*, int>
void luaw_ipairs(lua_State* L, int index, F fn)
{
    int sz = luaw_len(L, index);
    lua_pushvalue(L, index);   // clone the table

    for (int i = 1; i <= sz; ++i) {
        lua_rawgeti(L, -1, i);
        fn(L, i);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

template <typename F> requires std::invocable<F&, lua_State*, std::string>
void luaw_spairs(lua_State* L, int index, F fn)
{
    lua_pushvalue(L, index);   // clone the table

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING)
            fn(L, luaw_to<std::string>(L, -2));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

template <typename F> requires std::invocable<F&, lua_State*>
void luaw_pairs(lua_State* L, int index, F fn)
{
    lua_pushvalue(L, index);   // clone the table

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        fn(L);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

#endif //LUA_INL_
