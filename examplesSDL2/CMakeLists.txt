cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(VookooExamplesSDL2)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/../external)
include_directories(${PROJECT_SOURCE_DIR}/../include)

add_definitions(-DBINARY_DIR="${PROJECT_BINARY_DIR}/")

find_package(Vulkan REQUIRED)
find_package(SDL2  REQUIRED)

find_program(GLSLC glslc REQUIRED)

# example_sdl2(order name
#   [SHADERS shader...]
#   [DATA    file...]
#   [GLSLC_DEPENDS file...]
# )
# SHADERS:      compiled with glslc; .spv lands in PROJECT_BINARY_DIR.
#               The example's own source dir is always on the glslc include path.
# DATA:         copied as-is into PROJECT_BINARY_DIR at configure time.
# GLSLC_DEPENDS: extra files (relative to the example dir) that trigger shader recompilation.
function(example_sdl2 order exname)
  cmake_parse_arguments(ARG "" "" "SHADERS;DATA;GLSLC_DEPENDS" ${ARGN})
  set(shaders "")
  set(depends "")
  foreach(dep ${ARG_GLSLC_DEPENDS})
    list(APPEND depends "${PROJECT_SOURCE_DIR}/${exname}/${dep}")
  endforeach()

  foreach(shader ${ARG_SHADERS})
    add_custom_command(
      OUTPUT  ${shader}.spv
      COMMAND ${GLSLC} -I ${PROJECT_SOURCE_DIR}/${exname}
        ${PROJECT_SOURCE_DIR}/${exname}/${shader}
        -o ${PROJECT_BINARY_DIR}/${shader}.spv
      MAIN_DEPENDENCY ${exname}/${shader}
      DEPENDS ${depends}
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

  add_executable(${order}-${exname}
    ${exname}/${exname}.cpp
    ${shaders}
    ../include/vku/vku.hpp
    ../include/vku/vku_framework.hpp
  )

  target_link_libraries(${order}-${exname}
    SDL2::SDL2
    Vulkan::Vulkan
  )
endfunction()

example_sdl2(01 helloSDL2
  SHADERS helloSDL2.vert helloSDL2.frag helloSDL2.comp
)
example_sdl2(02 MSAA
  SHADERS MSAA.vert MSAA.frag
)
example_sdl2(03 noisedemo
  SHADERS
    noisedemo.vert
    noisedemo.frag
    noisedemo_s2d.frag
    noisedemo_s4d.frag
    noisedemo_c2d.frag
    noisedemo_c3d.frag
    noisedemo_c4d.frag
)
example_sdl2(04 fluid3D
  SHADERS
    fluid_advect.comp
    fluid_forces.comp
    fluid_divg.comp
    fluid_jacobi.comp
    fluid_grad.comp
    fluid_display.vert
    fluid_display.frag
)
example_sdl2(05 PrecomputedAtmosphericScattering2008
  DATA earth.png
  SHADERS
    quad.vert
    earth.vert
    transmittance.frag
    irradiance1.frag
    inscatter1.frag
    copyIrradiance.frag
    copyInscatter1.frag
    inscatterS.frag
    irradianceN.frag
    inscatterN.frag
    copyInscatterN.frag
    earth.frag
)
example_sdl2(06 PrecomputedAtmosphericScattering2017
  SHADERS scene.vert scene.frag
  DATA transmittance.ktx scattering.ktx irradiance.ktx
  GLSLC_DEPENDS definitions.glsl functions.glsl
)
example_sdl2(07 gamedemo
  SHADERS gamedemo.vert gamedemo.frag gamedemo_post.vert gamedemo_post.frag
          gamedemo_dust.comp gamedemo_dust.vert gamedemo_dust.frag
          gamedemo_rock.vert gamedemo_rock.frag
          gamedemo_hud.vert gamedemo_hud.frag
  DATA FINALNEW.TTF_sdf.png FINALNEW.TTF_sdf.txt
)
