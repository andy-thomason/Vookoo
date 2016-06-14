cmake_minimum_required (VERSION 2.6)

project (triangle_image)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
include_directories(../../include ../../external)
add_executable(triangle_image main.cpp)
target_link_libraries(triangle_image vulkan)

