cmake_minimum_required (VERSION 3.1.0)

project(VookooExamples)

if (WIN32)
  include_directories(${PROJECT_SOURCE_DIR}\\..\\external)
  include_directories(${PROJECT_SOURCE_DIR}\\..\\include)
  link_directories(${PROJECT_SOURCE_DIR}\\..\\external\\GLFW)
  link_directories(${PROJECT_SOURCE_DIR}\\..\\external\\vulkan)
else ()
  include_directories(${PROJECT_SOURCE_DIR}/../external)
  include_directories(${PROJECT_SOURCE_DIR}/../include)
  link_directories(${PROJECT_SOURCE_DIR}/../external/GLFW)
  link_directories(${PROJECT_SOURCE_DIR}/../external/vulkan)
endif ()

add_definitions(-DSOURCE_DIR="${CMAKE_SOURCE_DIR}/")
add_definitions(-DBINARY_DIR="${PROJECT_BINARY_DIR}/")

set(CMAKE_CXX_STANDARD 11)

macro(example exname)
  add_custom_command(
    OUTPUT ${exname}.frag.spv
    COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}.frag -o ${PROJECT_BINARY_DIR}/${exname}.frag.spv
    MAIN_DEPENDENCY ${exname}.frag
  )

  add_custom_command(
    OUTPUT ${exname}.vert.spv
    COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}.vert -o ${PROJECT_BINARY_DIR}/${exname}.vert.spv
    MAIN_DEPENDENCY ${exname}.vert
  )

  add_executable(${exname} ${exname}.cpp ${exname}.frag ${exname}.vert)
  target_link_libraries(${exname} glfw3)
  target_compile_features(${exname} PRIVATE cxx_range_for)

  if (WIN32)
    target_link_libraries(${exname} vulkan-1)
  endif()

  if (UNIX)
    target_link_libraries(${exname} vulkan dl pthread X11 Xrandr Xinerama Xcursor)
  endif()
endmacro(example)

macro(example2 exname)
  add_custom_command(
    OUTPUT ${exname}.shadow.frag.spv
    COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}.shadow.frag -o ${PROJECT_BINARY_DIR}/${exname}.shadow.frag.spv
    MAIN_DEPENDENCY ${exname}.shadow.frag
  )

  add_custom_command(
    OUTPUT ${exname}.shadow.vert.spv
    COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}.shadow.vert -o ${PROJECT_BINARY_DIR}/${exname}.shadow.vert.spv
    MAIN_DEPENDENCY ${exname}.shadow.vert
  )

  add_custom_command(
    OUTPUT ${exname}.frag.spv
    COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}.frag -o ${PROJECT_BINARY_DIR}/${exname}.frag.spv
    MAIN_DEPENDENCY ${exname}.frag
  )

  add_custom_command(
    OUTPUT ${exname}.vert.spv
    COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}.vert -o ${PROJECT_BINARY_DIR}/${exname}.vert.spv
    MAIN_DEPENDENCY ${exname}.vert
  )

  add_executable(${exname} ${exname}.cpp ${exname}.frag ${exname}.vert ${exname}.shadow.frag ${exname}.shadow.vert)
  target_link_libraries(${exname} glfw3)
  target_compile_features(${exname} PRIVATE cxx_range_for)

  if (WIN32)
    target_link_libraries(${exname} vulkan-1)
  endif()

  if (UNIX)
    target_link_libraries(${exname} vulkan dl pthread X11 Xrandr Xinerama Xcursor)
  endif()
endmacro(example2)

example(helloTriangle)
example(texture)
example2(teapot)
example(trypsin)

