cmake_minimum_required (VERSION 2.6)

project (triangle_image)

add_executable(triangle_image main.cpp)

target_link_libraries(triangle_image vulkan-1)

message(${CMAKE_SOURCE_DIR}/../external/vulkan)

add_custom_target(
  triangle_image_shaders ALL
  COMMAND glslangValidator -V mesh.frag -o mesh.frag.spv
  COMMAND glslangValidator -V mesh.vert -o mesh.vert.spv
  DEPENDS mesh.frag mesh.vert
)


