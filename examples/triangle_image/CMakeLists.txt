cmake_minimum_required (VERSION 2.6)

project (triangle_image)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
include_directories(../../include ../../external)

add_executable(triangle_image main.cpp)

add_custom_target(
  triangle_image_shaders ALL
  COMMAND glslangValidator -V mesh.frag -o mesh.frag.spv
  COMMAND glslangValidator -V mesh.vert -o mesh.vert.spv
  DEPENDS mesh.frag mesh.vert
)

target_link_libraries(triangle_image vulkan)

