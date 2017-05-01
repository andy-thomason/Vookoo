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

function(example exname)
  set(shaders "")
  
  foreach(shader ${ARGN})
    add_custom_command(
      OUTPUT ${shader}.spv
      COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}/${shader} -o ${PROJECT_BINARY_DIR}/${shader}.spv
      MAIN_DEPENDENCY ${exname}/${shader}
    )
    list(APPEND shaders "${exname}/${shader}")
  endforeach(shader)

  message(${shaders})

  add_executable(${exname} ${exname}/${exname}.cpp ${shaders} ../include/vku/vku.hpp ../include/vku/vku_framework.hpp)
  target_link_libraries(${exname} glfw3)
  target_compile_features(${exname} PRIVATE cxx_range_for)

  if (WIN32)
    target_link_libraries(${exname} vulkan-1)
  endif()

  if (UNIX)
    target_link_libraries(${exname} vulkan dl pthread X11 Xrandr Xinerama Xcursor)
  endif()
endfunction(example)

example(helloTriangle helloTriangle.vert helloTriangle.frag)
example(texture texture.vert texture.frag)
example(molvoo molvoo.vert molvoo.frag molvoo.comp)
example(teapot teapot.vert teapot.frag teapot.shadow.vert teapot.shadow.frag)

