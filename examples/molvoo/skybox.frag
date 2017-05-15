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

void main() {
  outColour = vec4(inPos * 0.5 + 0.5, 1);
}
