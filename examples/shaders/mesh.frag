#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) out vec4 fragColor_out;

void main() 
{
  fragColor_out = vec4(0, 0, 1, 1);
}
