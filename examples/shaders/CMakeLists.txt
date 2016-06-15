cmake_minimum_required (VERSION 2.6)

add_custom_target(shaders
  SOURCES mesh.frag mesh.vert
)

add_custom_command(
  OUTPUT mesh.frag.spv
  COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/shaders/mesh.frag -o ${PROJECT_BINARY_DIR}/shaders/mesh.frag.spv
  MAIN_DEPENDENCY mesh.frag
)

add_custom_command(
  OUTPUT mesh.vert.spv
  COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/shaders/mesh.vert -o ${PROJECT_BINARY_DIR}/shaders/mesh.vert.spv
  MAIN_DEPENDENCY mesh.vert
)


