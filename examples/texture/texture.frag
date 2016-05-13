#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D samplerColor;

layout (location = 0) in vec3 normal_in;
layout (location = 1) in vec3 viewPos_in;
layout (location = 2) in vec3 viewLightPos_in;
layout (location = 3) in vec2 uv_in;

layout (location = 0) out vec4 outFragColor;

void main() 
{
  outFragColor = texture(samplerColor, uv_in, 0);
}
