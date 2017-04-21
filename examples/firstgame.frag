#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout (binding = 0) uniform UBO 
{
  vec4 screenScale;
} ubo;

layout (binding = 1) uniform sampler2D samp; 

void main() {
  outColor = texture(samp, uv);
  outColor.w = 1;
}

