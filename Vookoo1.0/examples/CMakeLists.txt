cmake_minimum_required (VERSION 2.6)

add_definitions(-DVOOKOO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

project(examples)

add_subdirectory(shaders)
add_subdirectory(teapot)
add_subdirectory(marching_cubes)
add_subdirectory(texture)
add_subdirectory(triangle)
add_subdirectory(triangle_image)
add_subdirectory(glfw)

