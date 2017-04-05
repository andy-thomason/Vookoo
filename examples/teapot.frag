#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColour;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  vec4 colour;
} u;

layout (binding = 1) uniform sampler2D samp; 

void main() {
  //outColour = vec4(normalize(inNormal)*0.5f + 0.5f, 1);
  //outColour = vec4(inUv, 1, 1);
  outColour = texture(samp, inUv);
}

