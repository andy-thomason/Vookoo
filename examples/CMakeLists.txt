cmake_minimum_required(VERSION 3.5)

project(VookooExamples)

include_directories(${PROJECT_SOURCE_DIR}/../external)
include_directories(${PROJECT_SOURCE_DIR}/../include)

add_definitions(-DBINARY_DIR="${PROJECT_BINARY_DIR}/")

set(CMAKE_CXX_STANDARD 20)

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

# example(order name
#   [SHADERS shader...]
#   [DATA    file...]
# )
# SHADERS: compiled with glslangValidator; .spv lands in PROJECT_BINARY_DIR.
# DATA:    copied as-is into PROJECT_BINARY_DIR at configure time.
function(example order exname)
  cmake_parse_arguments(ARG "" "" "SHADERS;DATA" ${ARGN})
  set(shaders "")

  foreach(shader ${ARG_SHADERS})
    add_custom_command(
      OUTPUT ${shader}.spv
      COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/${exname}/${shader} -o ${PROJECT_BINARY_DIR}/${shader}.spv
      MAIN_DEPENDENCY ${exname}/${shader}
    )
    list(APPEND shaders "${exname}/${shader}")
  endforeach()

  foreach(datafile ${ARG_DATA})
    configure_file(
      ${PROJECT_SOURCE_DIR}/${exname}/${datafile}
      ${PROJECT_BINARY_DIR}/${datafile}
      COPYONLY
    )
  endforeach()

  add_executable(${order}-${exname} ${exname}/${exname}.cpp ${shaders} ../include/vku/vku.hpp ../include/vku/vku_framework.hpp)

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
endfunction()

example(01 helloTriangle
  SHADERS helloTriangle.vert helloTriangle.frag
)
example(02 pushConstants
  SHADERS pushConstants.vert pushConstants.frag
)
example(03 uniforms
  SHADERS uniforms.vert uniforms.frag
)
example(04 texture
  SHADERS texture.vert texture.frag
)
example(05 swirltexture
  SHADERS swirltexture.vert swirltexture.frag
)
example(06 teapot
  DATA okretnica.ktx
  SHADERS teapot.vert teapot.frag teapot.shadow.vert teapot.shadow.frag
)
example(07 helloCompute
  SHADERS helloCompute.comp
)
example(08 cybertruck
  SHADERS cybertruck.vert cybertruck.frag
)
example(09 dynamicUniformBuffer
  SHADERS dynamicUniformBuffer.vert dynamicUniformBuffer.frag
)
example(10 helloInstancing
  SHADERS helloInstancing.vert helloInstancing.frag
)
example(11 threaded
  SHADERS threaded.vert threaded.frag
)
example(12 flockaroo
  SHADERS flockaroo.vert flockaroo.frag advection.frag
)
example(13 renderToCubemapByMultiview
  SHADERS
    renderToCubemapByMultiview.vert renderToCubemapByMultiview.frag
    renderToCubemapByMultiviewPass2.vert renderToCubemapByMultiviewPass2.frag
)
example(14 crystalLogo
  SHADERS
    content.frag content.vert
    cube.frag cube.vert
    reflectionPlane.frag reflectionPlane.vert
    reflectionReflector.frag reflectionReflector.vert
)
example(15 fdtd2d
  SHADERS fdtd2d.vert fdtd2dpass0.frag fdtd2dpass1.frag fdtd2dpass2.frag
)
example(16 fdtd2dUpml
  SHADERS fdtd2dUpml.vert fdtd2dUpmlpass0.frag fdtd2dUpmlpass1.frag fdtd2dUpmlpass2.frag
)
example(17 fdtd2dUpmlCompute
  SHADERS
    fdtd2dUpmlComputePass0.comp fdtd2dUpmlComputePass1.comp
    fdtd2dUpmlComputeDisplay.vert fdtd2dUpmlComputeDisplay.frag
)
example(18 perlinNoise
  SHADERS perlinNoise.vert perlinNoise.frag
)
example(19 helloGeometryShader
  SHADERS helloGeometryShader.vert helloGeometryShader.frag helloGeometryShader.geom
)
example(20 helloTesselationShader
  SHADERS
    helloTesselationShader.vert helloTesselationShader.tesc
    helloTesselationShader.tese helloTesselationShader.geom
    helloTesselationShader.frag
)
example(21 gumbo
  SHADERS gumbo.vert gumbo.tesc gumbo.tese gumbo.geom gumbo.frag
)
example(22 subpass
  SHADERS
    subpass_0_scene.vert subpass_0_scene.frag
    subpass_1_composition.vert subpass_1_composition.frag
    subpass_2_fx.vert subpass_2_fx.frag
    subpass_3_decal.vert subpass_3_decal.frag
)
example(23 computeDisplay
  SHADERS computeDisplay.comp computeDisplay.vert computeDisplay.frag
)
example(24 computeImageDisplay
  SHADERS computeImageDisplay.comp computeImageDisplay.vert computeImageDisplay.frag
)
example(25 dynamicRendering
  SHADERS dynamicRenderingScene.vert dynamicRenderingScene.frag dynamicRenderingPost.vert dynamicRenderingPost.frag
)
