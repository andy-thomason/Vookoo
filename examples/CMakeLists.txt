cmake_minimum_required(VERSION 3.28 FATAL_ERROR)
cmake_policy(VERSION 3.28)

project(VookooExamples)

include_directories(${PROJECT_SOURCE_DIR}/../external)
include_directories(${PROJECT_SOURCE_DIR}/../include)

add_definitions(-DSOURCE_DIR="${CMAKE_SOURCE_DIR}/")
add_definitions(-DBINARY_DIR="${PROJECT_BINARY_DIR}/")

set(CMAKE_CXX_STANDARD 23)

if (${CMAKE_VERSION} VERSION_LESS "3.7.0")
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
  message(STATUS "Will be using custom FindVulkan.cmake module to find Vulkan SDK")
  message(WARNING "Better upgrade your CMake")
else()
  message(STATUS "Will be using standard FindVulkan.cmake module to find Vulkan SDK")
endif()

find_package(Vulkan REQUIRED)

function(setspirvsupport)
  get_filename_component(VULKAN_DIR ${Vulkan_INCLUDE_DIR} DIRECTORY)
  set(SPIR_V_INCLUDE_DIR ${VULKAN_DIR}/spirv-tools/external/spirv-headers/include/spirv/)
  set(SPIR_V_FILE ${SPIR_V_INCLUDE_DIR}unified1/spirv.hpp11)
  if(EXISTS ${SPIR_V_FILE})
	message(STATUS "Found " ${SPIR_V_FILE})
	message(STATUS "Setting VOOKOO_SPIRV_SUPPORT definition")
    include_directories(${SPIR_V_INCLUDE_DIR})
	add_definitions(-DVOOKOO_SPIRV_SUPPORT)
  endif()
endfunction(setspirvsupport)

setspirvsupport()

function(example order exname)
  set(shaders "")

  foreach(shader ${ARGN})
    add_custom_command(
      OUTPUT ${shader}.spv
      COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}/${shader} -o ${PROJECT_BINARY_DIR}/${shader}.spv
      MAIN_DEPENDENCY ${exname}/${shader}
    )
    list(APPEND shaders "${exname}/${shader}")
  endforeach(shader)

  #message(STATUS "Included ${shaders} shaders")

  add_executable(${order}-${exname} ${exname}/${exname}.cpp ${shaders} ../include/vku/vku.hpp ../include/vku/vku_framework.hpp)

  # Require C++11
  target_compile_features(${order}-${exname} PRIVATE cxx_range_for)

  target_link_libraries(${order}-${exname} glfw Vulkan::Vulkan)

  if (WIN32)
    target_link_libraries(${order}-${exname})
	add_definitions(-DNOMINMAX)
  endif()

  if(APPLE)
    target_link_libraries(${order}-${exname} dl pthread)
  else(APPLE)
    if (UNIX)
      target_link_libraries(${order}-${exname} dl pthread X11 Xrandr Xinerama Xcursor)
    endif()
  endif(APPLE)
endfunction(example)

example(01 helloTriangle helloTriangle.vert helloTriangle.frag)
example(02 pushConstants pushConstants.vert pushConstants.frag)
example(03 uniforms uniforms.vert uniforms.frag)
example(04 texture texture.vert texture.frag)
example(05 teapot teapot.vert teapot.frag teapot.shadow.vert teapot.shadow.frag)
example(06 helloCompute helloCompute.comp)
example(07 cybertruck cybertruck.vert cybertruck.frag)
example(08 dynamicUniformBuffer dynamicUniformBuffer.vert dynamicUniformBuffer.frag)
example(09 helloInstancing helloInstancing.vert helloInstancing.frag)
example(10 threaded threaded.vert threaded.frag)
example(11 flockaroo flockaroo.vert flockaroo.frag advection.frag)
example(12 renderToCubemapByMultiview renderToCubemapByMultiview.vert renderToCubemapByMultiview.frag renderToCubemapByMultiviewPass2.vert renderToCubemapByMultiviewPass2.frag)
example(13 crystalLogo content.frag  content.vert  cube.frag  cube.vert  reflectionPlane.frag  reflectionPlane.vert  reflectionReflector.frag  reflectionReflector.vert)
example(14 fdtd2d fdtd2d.vert fdtd2dpass0.frag fdtd2dpass1.frag fdtd2dpass2.frag)
example(15 fdtd2dUpml fdtd2dUpml.vert fdtd2dUpmlpass0.frag fdtd2dUpmlpass1.frag fdtd2dUpmlpass2.frag)
example(16 perlinNoise perlinNoise.vert perlinNoise.frag)
example(17 helloGeometryShader helloGeometryShader.vert helloGeometryShader.frag helloGeometryShader.geom)
example(18 helloTesselationShader helloTesselationShader.vert helloTesselationShader.tesc helloTesselationShader.tese helloTesselationShader.geom helloTesselationShader.frag)
example(19 gumbo gumbo.vert gumbo.tesc gumbo.tese gumbo.geom gumbo.frag)
