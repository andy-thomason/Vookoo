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

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColour;

layout (binding = 5) uniform sampler2D fountTexture;

void main() {
  vec4 tex = texture(fountTexture, inUV);
  outColour = vec4(tex.rrr * inColour, tex.r);
}
