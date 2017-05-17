#version 450

layout (binding = 1) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 cameraToWorld;
  vec4 colour;
} u;

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec4 outColour;

layout (binding = 4) uniform samplerCube cubeMap;

void main() {
  vec3 dir = normalize(inPos);
  outColour = texture(cubeMap, dir);
}
