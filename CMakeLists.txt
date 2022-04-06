cmake_minimum_required(VERSION 3.16)
project(microtar)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

add_executable(microtar
        src/microtar.cpp
        src/microtar.h)
