cmake_minimum_required(VERSION 3.12)
project(remote_terminal)

include_directories(.)

set(CMAKE_CXX_STANDARD 11)

add_executable(remote_terminal main.cpp)

target_link_libraries(remote_terminal pthread)
