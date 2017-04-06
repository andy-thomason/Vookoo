#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inCameraDir;

layout(location = 0) out vec4 outColour;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  vec4 cameraPos;
} u;

layout (binding = 1) uniform samplerCube samp;

void main() {
  vec3 cameraDir = normalize(inCameraDir);
  vec3 normal = normalize(inNormal);

  vec3 reflectDir = normalize(reflect(cameraDir, normal));
  outColour = texture(samp, reflectDir);
}

