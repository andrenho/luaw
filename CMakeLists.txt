cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(luaw)

add_subdirectory(lua)
# add_subdirectory(luajit-cmake)

set(SRC
        luaw/luaw.hh
        luaw/luaw.cc)

option(LUA_BUILD_AS_CXX "Build lua as C++" ON)
option(LUA_ENABLE_TESTING "Build and run tests" OFF)
add_library(luaw-54 SHARED ${SRC})
target_link_libraries(luaw-54 lua_static)

#set(LUAJIT_DIR luajit-cmake CACHE INTERNAL)
#add_library(luaw-jit SHARED ${SRC})
#target_link_libraries(luaw-jit PRIVATE luajit::lib)
