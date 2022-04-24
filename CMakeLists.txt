cmake_minimum_required(VERSION 3.16)
project(tarfull)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

add_executable(tarfull
        src/tarfull.cpp
        src/tarfull.h)

target_link_libraries(${PROJECT_NAME} stdc++fs)