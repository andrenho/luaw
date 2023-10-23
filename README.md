# luaw

A C++ extension to the Lua C library with a focus on interaction between Lua and C++. 
It allows converting Lua objects to C++ objects and vice-versa. 

(You need to be familiar with the Lua C API for this library make sense to you: https://www.lua.org/manual/5.4/)

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

See [Build instructions](#build-instructions).

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

### Embedding Lua code in a C++ application

The applications `luazh-54` and `luazh-jit` allow for generating a C header containing compressed
lua bytecode, that can be embedded in the application. The executable can be run as this:

```bash
./luazh-54 test_lua -s test.lua > test.hh      # -s will strip debugging info
```

This will generate a header can be loaded with the following function:

```c++
void luaw_do_z(lua_State* L, unsigned char data[], size_t compressed_sz, size_t uncompressed_sz, 
               int nresults=0, string name="anonymous");

// example usage:
#include "test.hh"
luaw_do_z(L, test_lua);
```

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

### Custom C++ classes as Lua userdata

```c++
void luaw_push_userdata<T>(lua_State* L, ConstructorArgs...);
```

Custom C++ can be added to Lua as userdata. In this case, Lua itself will manage the object memory.

Example:

```c++
struct MyStruct {
    MyStruct(int x) : x(x) { printf("Constructor called.\n"); }
    ~MyStruct()            { printf("Destructor called.\n"); }
    
    int x;
};

lua_State* L = luaw_newstate();
luaw_push_userdata<MyStruct>(L, 42);      // "Constructor called"

MyStruct* my_obj = luaw_to<MyStruct*>(L, -1);
int x = my_obj->x;                        // x = 42

lua_close(L);                             // "Destructor called"
```

A metatable is automatically applied to any objects, in which the `__gc` metamethod automatically
calls the constructor.

### Metatables

```c++
void luaw_set_metatable<T>(lua_State* L,const luaL_Reg* reg);
```

This function can be used to create a metatable for a custom C++ class. After this function is
called for a particular class, any objects created (either as table or userdata) will have that
metatable applied to it.

Example:

```c++
class X {
    std::string to_string() const { return "My class X"; }
};

luaw_set_metatable<X>(L, (luaL_Reg[]) {
    { "__tostring", [](lua_State *L) {
        X* x = luaw_to<X*>(L, 1);
        luaw_push(x->to_string());
        return 1;
    },
    { nullptr, nullptr }
});

luaw_push_userdata<X>(L);
luaL_set_global(L, "x");

luaw_do(L, "print(x)");            // result: "My class X":w
```

## Globals

```c++
T    luaw_getglobal(lua_State* L, string global_name);
void luaw_setglobal(lua_State* L, string global_name, T value);
```

These functions will manage globals doing the direct C++/Lua conversion.

## Iteration

```c++
// iterate over a Lua table array
void luaw_ipairs(lua_State*L, int index, void(lua_State*, int) function);

// iterate over a Lua table with string keys
void luaw_spairs(lua_State*L, int index, void(lua_State*, string) function);

// iterate over a Lua table
void luaw_spairs(lua_State*L, int index, void(lua_State*) function);
```

These functions are used to iterate over Lua tables. A lambda is called for each record. For the 
first two versions the value is placed on the stack, and in the thirds the key and value are placed
into the stack. No need to pop it during iterations.

Example:

```c++
luaw_do(L, "return { 10, 20, 30 }");
luaw_ipairs(L, -1, [](lua_State* L, int key) { printf("%d ", luaw_to<int>(L, -1)); });
// result: "10 20 30"
```

## Fields

```c++
// the `index` always refer to the table position in the stack

void luaw_getfield(lua_State* L, int index, string field);  // get a field and put it on the stack
void luaw_setfield(lua_State* L, int index, string field);  // set the field on the stack position -1

bool luaw_hasfield(lua_State* L, int index, string field);  // return if the field exists

// these are the same versions as above, but return the C++ object instead of putting in the stack
T    luaw_getfield<T>(lua_State* L, int index, string field);
void luaw_setfield(lua_State* L, int index, string field, T value);
```

These functions are similar to `lua_getfield`and `lua_setfield` (from the Lua API), but they can
read compound fields. For example, in the following Lua table:

```Lua
{
  a = {
    b = {
      c = 48
    }
  }
} 
```

Getting the field `"a.b.c"` using these functions will return 48.

## Function calls

```c++
// call the function on the stack, passing the C++ values as parameters
// put the result on the stack
void luaw_call_push(lua_State* L, int nresults, auto parameters...);

// same thing, but return result as a C++ value
T    luaw_call(lua_State* L, auto parameters...);

// same as the versions above, but the function is a global
void luaw_call_global_push(lua_State* L, string global, int nresults, auto parameters...);
T    luaw_call_global(lua_State* L, string global, auto parameters...);

// same as the versions above, but the function is a field
void luaw_call_field_push(lua_State* L, int index, string field, int nresults, auto parameters...);
T    luaw_call_field(lua_State* L, int index, string field, auto parameters...);
```

## Other

```c++
// get Lua table size
int luaw_len(lua_State* L, int index);

// ensure that the stack has the expected number of items, otherwise abort
int luaw_ensure(lua_State* L, int expected=0);

// call the "tostring" metamethod on the object
string luaw_to_string(lua_State* L, int index);
```

## Debugging

```c++
// return the string representation of an object in the stack
string luaw_dump(lua_State* L, int index, size_t max_depth=3);

// return the string representation of the whole stack
string luaw_dump_stack(lua_State* L, size_t max_depth=3);

// print the whole stack
string luaw_print_stack(lua_State* L, size_t max_depth=3);
```

`max_depth` is the maxiumum depth when printing tables. If any object has a `__tostring` metamethod,
this is used instead.

## Build instructions

The only dependency is zlib - you probably already have it installed.

```bash
git submodule update --init --recursive
make
```

This will generate the static libraries `libluaw-54.a` and `libluaw-jit.a`, and the applications
`luazh-54` and `luazh-jit` (for compressing lua files for embedding).

General recommendation is that this library is provided as a git submodule to any projects using it.