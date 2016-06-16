cmake_minimum_required (VERSION 2.6)

project (triangle_image)

add_executable(triangle_image main.cpp)

target_link_libraries(triangle_image ${VULKAN_LIBRARY_NAME})

add_dependencies(triangle_image shaders)

