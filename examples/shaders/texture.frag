#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) out vec4 fragColor_out;

layout (binding = 1) uniform UBO 
{
  vec4 colour;
} ubo;

layout (binding = 2) uniform sampler2D samp; 

void main() 
{
  fragColor_out = texture(samp, gl_FragCoord.xy * 0.01);
}
