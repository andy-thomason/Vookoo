cmake_minimum_required (VERSION 2.6)

project (triangle_image)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
include_directories(../../include ../../external)

add_executable(triangle_image main.cpp)


add_custom_target(
  mesh.frag.spv ALL
  COMMAND glslangValidator -V mesh.frag -o mesh.frag.spv
  DEPENDS mesh.frag
)

add_custom_target(
  mesh.vert.spv ALL
  COMMAND glslangValidator -V mesh.vert -o mesh.vert.spv
  DEPENDS mesh.vert
)

target_link_libraries(triangle_image vulkan)

