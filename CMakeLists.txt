cmake_minimum_required(VERSION 3.16)
project(tarfful)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

add_executable(tarfful
        src/tarfful.cpp
        src/tarfful.h)

target_link_libraries(${PROJECT_NAME} stdc++fs)
