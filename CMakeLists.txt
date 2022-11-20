cmake_minimum_required(VERSION 2.8)
project(tarfful)

set(CMAKE_CXX_STANDARD 11)
link_libraries(stdc++fs)

include_directories(src)
add_library(tarfful src/tarfful.h src/tarfful.cpp)