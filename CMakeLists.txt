cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(luaw)

set(SRC
        luaw/luaw.hh
        luaw/luaw.cc)

add_library(luaw SHARED ${SRC})