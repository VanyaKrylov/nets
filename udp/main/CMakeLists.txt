cmake_minimum_required(VERSION 3.12)
project(main)

set(CMAKE_CXX_STANDARD 14)

add_executable(main main.cpp)

target_link_libraries(main pthread)