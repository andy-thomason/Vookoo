#version 450

layout (push_constant) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 cameraToWorld;

  vec3 rayStart;
  float timeStep;
  vec3 rayDir;
  uint numAtoms;
  uint numConnections;
  uint pickIndex;
  uint pass;
} u;

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec4 outColour;

layout (binding = 4) uniform samplerCube cubeMap;

void main() {
  vec3 dir = normalize(inPos);
  outColour = texture(cubeMap, dir);
}
