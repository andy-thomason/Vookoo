cmake_minimum_required (VERSION 2.6)

project(Vookoo_examples)

IF(WIN32)
  set(VULKAN_LIBRARY_NAME vulkan-1)
  link_directories(${PROJECT_SOURCE_DIR}/../external/vulkan)
ELSE(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
  set(VULKAN_LIBRARY_NAME vulkan)
ENDIF(WIN32)

include_directories(${PROJECT_SOURCE_DIR}/../include/ ${PROJECT_SOURCE_DIR}/../external/)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(triangle_image)

