cmake_minimum_required (VERSION 2.6)

project (glfw)

add_executable(glfw main.cpp)

target_link_libraries(glfw ${VULKAN_LIBRARY_NAME})

add_dependencies(glfw shaders)

