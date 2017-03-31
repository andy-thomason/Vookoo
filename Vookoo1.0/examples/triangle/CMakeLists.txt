cmake_minimum_required (VERSION 2.6)

project (triangle)

add_executable(triangle main.cpp)

target_link_libraries(triangle ${VULKAN_LIBRARY_NAME})

add_dependencies(triangle shaders)

