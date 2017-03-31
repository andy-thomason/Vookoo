cmake_minimum_required (VERSION 2.6)

project (marching_cubes)

add_executable(marching_cubes main.cpp)

target_link_libraries(marching_cubes ${VULKAN_LIBRARY_NAME})

add_dependencies(marching_cubes shaders)

