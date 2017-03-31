cmake_minimum_required (VERSION 2.6)

project (texture)

add_executable(texture main.cpp)

target_link_libraries(texture ${VULKAN_LIBRARY_NAME})

add_dependencies(texture shaders)

