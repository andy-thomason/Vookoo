cmake_minimum_required (VERSION 2.6)

project (teapot)

add_executable(teapot main.cpp)

target_link_libraries(teapot ${VULKAN_LIBRARY_NAME})

add_dependencies(teapot shaders)

