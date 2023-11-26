#ifndef LUA_INL_
#define LUA_INL_

#include <memory>
#include <optional>
#include <map>
#include <unordered_map>
#include <tuple>

#include <cxxabi.h>

//
// CONCEPTS
//

template<typename T>
concept IntegerType = requires(T param)
{
    requires std::is_integral_v<T>;
    requires !std::is_pointer_v<T>;
    requires !std::same_as<T, bool>;
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
    requires !std::is_same_v<T, lua_CFunction>;
};

template< typename T >
concept Optional = requires( T t )
{
    typename T::value_type;
    requires std::same_as< T, std::optional< typename T::value_type > >;
    t.value();
};

template <typename T>
concept Iterable = requires(T t) {
    begin(t);
    end(t);
    t.push_back(typename T::value_type{});
    requires !std::is_same_v<T, std::string>;
};

template<typename T>
concept MapType =
    std::same_as<T, std::map<typename T::key_type, typename T::mapped_type, typename T::key_compare, typename T::allocator_type>> ||
    std::same_as<T, std::unordered_map<typename T::key_type, typename T::mapped_type, typename T::hasher, typename T::key_equal, typename T::allocator_type>>;


template<class T, std::size_t N>
concept has_tuple_element =
requires(T t) {
    typename std::tuple_element_t<N, std::remove_const_t<T>>;
    { get<N>(t) } -> std::convertible_to<const std::tuple_element_t<N, T>&>;
};

template<class T>
concept Tuple = !std::is_reference_v<T> && requires(T t) {
    typename std::tuple_size<T>::type;
    requires std::derived_from<
            std::tuple_size<T>,
            std::integral_constant<std::size_t, std::tuple_size_v<T>>
    >;
} && []<std::size_t... N>(std::index_sequence<N...>) {
    return (has_tuple_element<T, N> && ...);
}(std::make_index_sequence<std::tuple_size_v<T>>());

template <typename T>
concept PushableToLua = requires(T t) {
    { &T::to_lua };
};

template <typename T>
concept ConvertibleToLua = requires(T t) {
    { &T::from_lua };
};

template <typename T>
concept ComparableToLua = requires(T t) {
    { &T::lua_is };
};

//
// PRIVATE - metatable identifier
//

template <typename T>
constexpr bool has_mt_identifier = requires(const T& t) {
    { &T::mt_identifier };
};

template <typename T>
const char* mt_identifier() {
    if constexpr (has_mt_identifier<std::remove_pointer_t<T>>)
        return std::remove_pointer_t<T>::mt_identifier;
    else
        return typeid(std::remove_pointer_t<T>).name();
}

//
// CODE LOADING
//

template <typename T> T luaw_do(lua_State* L, std::string const& buffer, std::string const& name)
{
    luaw_do(L, buffer, 1, name);
    return luaw_pop<T>(L);
}

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

template <typename T> T luaw_to(lua_State* L, int index)
{
    if (!luaw_is<T>(L, index)) {
        std::string cpp_type = typeid(T).name();

        int status = -4;
        std::unique_ptr<char, void(*)(void*)> res {
                abi::__cxa_demangle(cpp_type.c_str(), NULL, NULL, &status),
                std::free
        };

        if (status == 0)
            cpp_type = res.get();

        luaL_error(L, "Type unexpected (expected C++ type `%s`, actual lua type is `%s` (%s))",
                   cpp_type.c_str(), lua_typename(L, lua_type(L, index)), luaw_dump(L, index, false).c_str());
    }
    return luaw_to_<T>(L, index);
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

template <IntegerType T> int luaw_push(lua_State* L, T const& t) { lua_pushinteger(L, t); return 1; }
template <IntegerType T> bool luaw_is(lua_State* L, int index) {
    if (!lua_isnumber(L, index))
        return false;
    else
        return lua_tointeger(L, index) == lua_tointeger(L, index);
}
template <IntegerType T> T luaw_to_(lua_State* L, int index) { return (T) lua_tointeger(L, index); }

// number

template <FloatingType T> int luaw_push(lua_State* L, T const& t) { lua_pushnumber(L, t); return 1; }
template <FloatingType T> bool luaw_is(lua_State* L, int index) { return lua_isnumber(L, index); }
template <FloatingType T> T luaw_to_(lua_State* L, int index) { return (T) lua_tonumber(L, index); }

// pointer / userdata

template<typename T, typename... Args> T* luaw_push_new_userdata(lua_State* L, Args... args)
{
    T* t = (T*) lua_newuserdata(L, sizeof(T));
    new(t) T(args...);

    // get metatable
    lua_pushstring(L, mt_identifier<T>());
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (!lua_isnil(L, -1)) {
        lua_setmetatable(L, -2);   // apply stored metatable
        // TODO - add GC
    } else {
        // metatable not found, create one with only the GC calling the destructor
        lua_pop(L, 1);
        luaL_newmetatable(L, mt_identifier<T>());
        static luaL_Reg destructor_metatable[] {
                { "__gc", [](lua_State* L) { luaw_to<T*>(L, -1)->~T(); return 0; } },
                {nullptr, nullptr}
        };
        luaL_setfuncs(L, destructor_metatable, 0);
        lua_pop(L, 1);
        luaL_setmetatable(L, mt_identifier<T>());
    }

    return t;
}

template <PointerType T> int luaw_push(lua_State* L, T const& t)
{
    lua_newtable(L);
    lua_pushlightuserdata(L, t);
    lua_setfield(L, -2, "__ptr");
    luaL_setmetatable(L, mt_identifier<T>());
    return 1;
}

template <PointerType T> bool luaw_is(lua_State* L, int index)
{
    if (lua_isuserdata(L, index))  // TODO - check for specific userdata metatable
        return true;
    if (lua_type(L, index) == LUA_TTABLE) {
        lua_getfield(L, index, "__ptr");
        bool is = (lua_type(L, -1) == LUA_TLIGHTUSERDATA);
        lua_pop(L, 1);
        return is;
    }
    return false;
}

template <PointerType T> T luaw_to_(lua_State* L, int index)
{
    if (lua_type(L, index) == LUA_TUSERDATA) {
        return (T) lua_touserdata(L, index);
    } else if (lua_type(L, index)) {
        lua_getfield(L, index, "__ptr");
        T ptr = (T) lua_touserdata(L, -1);
        lua_pop(L, 1);
        return ptr;
    } else {
        luaL_error(L, "Unexpected type - not a userdata");
        return nullptr;
    }
}

// table (vector, set...)

template <Iterable T> int luaw_push(lua_State* L, T const& t) {
    lua_newtable(L);
    int i = 1;
    for (auto const& v : t) {
        luaw_push(L, v);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}
template <Iterable T> bool luaw_is(lua_State* L, int index) { return lua_istable(L, index); }
template <Iterable T> T luaw_to_(lua_State* L, int index) {
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

template <Optional T> int luaw_push(lua_State* L, T const& t) {
    if (t.has_value())
        luaw_push<typename T::value_type>(L, *t);
    else
        lua_pushnil(L);
    return 1;
}
template <Optional T> bool luaw_is(lua_State* L, int index) {
    if (index > lua_gettop(L))
        return true;
    return lua_isnil(L, index) || luaw_is<typename T::value_type>(L, index);
}
template <Optional T> T luaw_to_(lua_State* L, int index) {
    if (index > lua_gettop(L) || lua_isnil(L, index))
        return T {};
    else
        return luaw_to<typename T::value_type>(L, index);
}

// tuple

template <Tuple T> int luaw_push(lua_State* L, T const& t) {
    lua_newtable(L);
    int i = 1;
    std::apply([L, &i](auto&&... args) { ((luaw_push(L, args), lua_rawseti(L, -2, i++)), ...); }, t);
    return 1;
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

template <Tuple T> T luaw_to_(lua_State* L, int index)
{
    T t;
    tuple_element_set<T>(L, index, t);
    return t;
}

// map

template <MapType T> int luaw_push(lua_State* L, T const& t) {
    lua_newtable(L);
    for (auto const& kv: t) {
        luaw_push(L, kv.first);
        luaw_push(L, kv.second);
        lua_rawset(L, -3);
    }
    return 1;
}

template <MapType T> bool luaw_is(lua_State* L, int index) {
    bool is = lua_type(L, index) == LUA_TTABLE;
    if (is) {
        lua_pushvalue(L, index);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (!luaw_is<typename T::key_type>(L, -2))
                is = false;
            if (!luaw_is<typename T::mapped_type>(L, -1))
                is = false;
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    return is;
}

template <MapType T> T luaw_to_(lua_State* L, int index) {
    T t;
    luaw_pairs(L, index, [&t](lua_State* L) {
        auto key = luaw_to<typename T::key_type>(L, -2);
        auto value = luaw_to<typename T::mapped_type>(L, -1);
        t[key] = value;
    });
    return t;
}

// struct objects

template <PushableToLua T> int luaw_push(lua_State* L, T const& t)
{
    t.to_lua(L);
    luaL_setmetatable(L, mt_identifier<T>());
    return 1;
}

template <ConvertibleToLua T> T luaw_to_(lua_State* L, int index)
{
    return T::from_lua(L, index);
}

template <ComparableToLua T> bool luaw_is(lua_State*L, int index)
{
    return T::lua_is(L, index);
}

/*
// variant

template <typename... Types>
int luaw_push(lua_State* L, std::variant<Types...> const& t) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        luaw_push<T>(L, arg);
    }, t);
}

template <typename T, typename... Types>
concept Variant = requires(T t) {
    std::is_same_v<T, std::variant<Types...>>;
};

template <Variant T>
bool luaw_is(lua_State* L, int index) {
    return false;
}

template <typename T, typename... Types>
std::variant<T, Types...> lua_to(lua_State* L, int index) {
    std::variant<T, Types...> t;
    return t;
}
 */

//
// GLOBALS
//

template <typename T> T luaw_getglobal(lua_State* L, std::string const& global)
{
    lua_getglobal(L, global.c_str());
    return luaw_pop<T>(L);
}

template <typename T> void luaw_setglobal(lua_State* L, std::string const& global, T const& t)
{
    luaw_push(L, t);
    lua_setglobal(L, global.c_str());
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

//
// FIELDS
//

template <typename T> T luaw_getfield(lua_State* L, int index, std::string const& field)
{
    luaw_getfield(L, index, field);
    T t = luaw_to<T>(L, -1);
    lua_pop(L, 1);
    return t;
}

template <typename T> void luaw_setfield(lua_State* L, int index, std::string const& field, T const& t)
{
    luaw_push(L, t);
    luaw_setfield(L, index - 1, field);
}

//
// CALLS
//

template <typename T> T luaw_call(lua_State* L, auto&&... args)
{
    ([&] { luaw_push(L, args); } (), ...);
    lua_call(L, sizeof...(args), 1);
    return luaw_pop<T>(L);
}

template <typename T> T luaw_call_global(lua_State* L, std::string const& global, auto&&... args)
{
    lua_getglobal(L, global.c_str());
    return luaw_call<T>(L, args...);
}

template <typename T> T luaw_call_field(lua_State* L, int index, std::string const& field, auto&&... args)
{
    luaw_getfield(L, index, field);
    return luaw_call<T>(L, args...);
}

int luaw_call_push(lua_State* L, int nresults, auto&... args)
{
    ([&] { luaw_push(L, args); } (), ...);
    lua_call(L, sizeof...(args), nresults);
    return nresults;
}

int luaw_call_push_global(lua_State* L, std::string const& global, int nresults, auto&&... args)
{
    lua_getglobal(L, global.c_str());
    ([&] { luaw_push(L, args); } (), ...);
    lua_call(L, sizeof...(args), nresults);
    return nresults;
}

int luaw_call_push_field(lua_State* L, int index, std::string const& field, int nresults, auto&&... args)
{
    luaw_getfield(L, index, field);
    ([&] { luaw_push(L, args); } (), ...);
    lua_call(L, sizeof...(args), nresults);
    return nresults;
}

//
// METATABLE
//

template<typename T> std::string luaw_set_metatable(lua_State* L, LuaMetatable const& mt)
{
    luaL_Reg regs[mt.size() + 1];
    size_t i = 0;
    for (auto const& kv : mt) {
        regs[i++] = (luaL_Reg) { kv.first.c_str(), kv.second };
    }
    regs[i] = {nullptr, nullptr};

    luaL_newmetatable(L, mt_identifier<T>());
    luaL_setfuncs(L, regs, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);

    return mt_identifier<T>();
}

#endif //LUA_INL_
