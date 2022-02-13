#version 460

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 1) uniform Uniform {
 float u_depthOpacity;
} u;

#define u_depthOpacity u.u_depthOpacity

layout (binding = 2) uniform sampler2D u_texture; // contentFBO

layout(location = 0) in vec2 v_uv;
layout(location = 1) in float v_z;

layout(location = 0) out vec4 FragColor;

void main() {
  vec4 vtexture = texture(u_texture, v_uv);
  vtexture.a -= u_depthOpacity * v_z;

  FragColor = vtexture;
}
