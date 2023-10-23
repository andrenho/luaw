# luaw

A C++ extension to the Lua C library with a focus on interaction between Lua and C++. 
It allows converting Lua objects to C++ objects and vice-versa. 

It allows, for example, for things like this:

```c++
auto vv = luaw_do<map<string, int>>(L, "return { hello=42, world=28 } }");
// result: C++ map with map<string, int> { { "hello", 42 }, { "world", 28 } }
```

or this:

```c++
luaw_do(L, "function concat(a, b) return a .. b end");
string result = luaw_call_global<string>(L, "concat", "hello", 42);  // result: "hello42":w
```

Supports [Lua 5.4](https://www.lua.org/) and [LuaJIT](https://luajit.org/). One static library
(`*.a`) is generated for each environment - the static library also includes the Lua library.

Errors during execution of the functions below will throw a `LuaException`.

## Initialization

```c++
lua_State* luaw_newstate();
```

Initialize the state, load basic libraries and put Lua in static mode (declaring globals in
local scope is forbidden).

## Code execution

```c++
// execute code or bytecode in binary data
void luaw_do(lua_State* L, uint8_t* data, size_t sz, int nresults=0, string env_name="anonymous");

// execute code
void luaw_do(lua_State* L, string code, int nresults=0, string env_name="anonymous");

// load code from file and execute it
void luaw_do(lua_State* L, string code, int nresults=0, string env_name="anonymous");

// execute code, and return result as C++ value
T luaw_do<T>(lua_State* L, string code, string env_name="anonymous");
```

Execute arbitrary lua code. The first 3 calls will put the result(s) in the stack, the last
one will return the result as a C++ value.

## Stack management

```c++
// push a value into the stack
void luaw_push(lua_State* L, T t);

// check if a stack value is of that type
bool luaw_is<T>(lua_State* L, int index);

// convert a value in stack to a C++ value
T luaw_to<T>(lua_State* L, int index);

// pop a value from the stack, converting to C++
T luaw_pop<T>(lua_Sstate* L);
```

Any kind of value can be pushed/popped. Regular Lua types are converted to their C++ counterparts
(ex.: `number` to `double`). The following C++ types can also be converted:

* `std::vector` or `std::set`: converted from a Lua table with numeric keys
* `std::map`: converted from a Lua table with non-numeric keys
* `std::optional`: converted from Lua value can also be `nil`
* `std::pair` or `std::tuple`: convert form Lua tables that contain distinct types (such as
  `{ "hello", false, 42 }`)

### Custom C++ classes as Lua tables

A C++ that can be converted to Lua automatically looks like this:

```c++
struct Point {
    int x, y;

    void to_lua(lua_State* L) const {     // enable `luaw_push`
        lua_newtable(L);
        luaw_setfield(L, -1, "x", x);
        luaw_setfield(L, -1, "y", y);
    }

    static Point from_lua(lua_State* L, int index) {  // enable `luaw_to` and `luaw_pop`
        return {
            .x = luaw_getfield<int>(L, index, "x"),
            .y = luaw_getfield<int>(L, index, "y"),
        };
    }

    static bool lua_is(lua_State* L, int index) {   // enable `luaw_is`
        return luaw_hasfield(L, index, "x")
            && luaw_hasfield(L, index, "y");
    }
};
```

The library will be looking for functions with these particular names.

Each of these functions are optional, and enable one type of call. For example, if only pushes
are needed, then only `to_lua` needs to be implemented.

This type can then be used like this:

```c++
Point point { 30, 40 };
luaw_push(L, point);              // the following is added to Lua stack: `{ x=30, y=40 }`
bool ok = luaw_is<Point>(L, -1);  // result: true

Point mp = luaw_pop<Point>(L);
printf("%d", mp.x);               // result: 30
```



## Other

```
int luaw_len(L, index)             -> get Lua table size
int luaw_ensure(L, expected=0)     -> abort if stack size is different than expected
```


## Debugging