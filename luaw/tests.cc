#include "luaw.hh"

#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <tuple>
#include <map>
#include <set>
#include <variant>

using namespace std::string_literals;

extern "C" {
#if LUAW == JIT
# include "../luajit/src/lua.hpp"
#else
# include "lua.hpp"
#endif
}

int main()
{
    lua_State* L = luaw_newstate();

    luaw_ensure(L);

    // luaw_do

    const char* hello = "print('luaw_do const char* ok!')";
    size_t sz = strlen(hello);
    luaw_do(L, (uint8_t *) hello, sz, 0, "hello");

    luaw_do(L, "print('luaw_do string ok!')");

    try {
        luaw_do(L, "print(");
        assert(false);
    } catch (LuaException& e) {
        // printf("expected error: %s\n", e.what());
    }

    try {
        luaw_do(L, "print(a[1])");
        assert(false);
    } catch (LuaException& e) {
        // printf("expected error: %s\n", e.what());
    }

    // luaw_dump

    luaw_ensure(L);

    printf("---------------------\n");

    auto dump = [&](auto const& v) {
        luaw_do(L, "return "s + v, 1); printf("%s\n", luaw_dump(L, -1).c_str()); lua_pop(L, 1);
    };
    dump("nil");
    dump("42");
    dump("42.8");
    dump("true");
    dump("false");
    dump("function() end");
    dump("{ a=4, 'b', { 'c', 'd' } }");

    lua_pushlightuserdata(L, (void *) hello);
    printf("%s\n", luaw_dump(L, -1).c_str());
    lua_pop(L, 1);

    luaw_push(L, true);
    printf("%s\n", luaw_dump(L, -1).c_str());
    lua_pop(L, 1);

    // luaw_dump_stack

    printf("---------------------\n");

    luaw_ensure(L);

    lua_pushstring(L, "abc");
    lua_pushinteger(L, 42);
    printf("%s\n", luaw_dump_stack(L).c_str());
    lua_pop(L, 2);

    printf("---------------------\n");

    // stack management

    luaw_ensure(L);

    luaw_push(L, true); assert(luaw_pop<bool>(L));
    luaw_push(L, 42); assert(luaw_pop<int>(L) == 42);
    luaw_push(L, 42.8); assert(luaw_pop<double>(L) == 42.8);
    luaw_push(L, "hello"s); assert(luaw_pop<std::string>(L) == "hello");
    luaw_push(L, "hello"); assert(std::string(luaw_pop<const char*>(L)) == "hello");

    int* x = (int *) malloc(sizeof(int));
    *x = 40;
    luaw_push(L, x); assert(*luaw_pop<int*>(L) == 40);
    free(x);

    std::vector<int> v { 10, 20, 30 };
    luaw_push(L, v);
    assert(luaw_pop<std::vector<int>>(L) == v);

    std::optional<int> o = 42;
    luaw_push(L, o); assert(luaw_pop<std::optional<int>>(L) == 42);

    std::pair<int, std::string> p = { 42, "hello" };
    luaw_push(L, p);
    assert(luaw_pop<decltype(p)>(L) == p);

    std::tuple<bool, int, std::string> tp = { false, 48, "str" };
    luaw_push(L, tp);
    printf("%s\n", luaw_dump(L, -1).c_str());
    assert(luaw_pop<decltype(tp)>(L) == tp);

    std::map<std::string, int> mp { { "hello", 42 }, { "world", 18 } };
    luaw_push(L, mp);
    printf("%s\n", luaw_dump(L, -1).c_str());
    assert(luaw_pop<decltype(mp)>(L) == mp);

    std::variant<int, std::string> vv { 42 };
    luaw_push(L, vv);

    printf("---------------------\n");

    // iterations

    luaw_ensure(L);

    luaw_do(L, "return { 'd', 'e', a=1, b=2, c=3, [8]='f' }", 1);
    luaw_ipairs(L, -1, [](lua_State* L, int i) {
        printf("%d: %s   ", i, lua_tostring(L, -1));
    });
    printf("\n");
    luaw_spairs(L, -1, [](lua_State* L, std::string const& key) {
        printf("%s: %s   ", key.c_str(), luaw_dump(L, -1).c_str());
    });
    printf("\n");
    luaw_pairs(L, -1, [](lua_State* L) {
        printf("%s: %s   ", luaw_dump(L, -2).c_str(), luaw_dump(L, -1).c_str());
    });
    printf("\n");
    lua_pop(L, 1);
}