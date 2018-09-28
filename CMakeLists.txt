cmake_minimum_required(VERSION 3.12)
project(nets)

include_directories(.)

set(CMAKE_CXX_STANDARD 11)

add_executable(nets main.cpp)

target_link_libraries(nets pthread)
