#version 460

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 fragColor;

layout (binding = 0) uniform UBO
{
  float t;
  float pad[3];
} ubo;

layout (binding = 1) uniform sampler2D samp;

void main() {
  float theta = 1.0f - min( length(UV), 1.0f );
  if ( length(UV) <= 1.0f ) {
    theta *= 2.0f*3.1456;
    theta += ubo.t; //*16.0/128.;
    theta *= (1.0f-smoothstep(0.0f, 0.5f, min( length(UV)/sqrt(2.), 0.5f)));
  } else {
    theta = 0.0f;
  } 
  mat2x2 R = mat2x2(vec2(cos(theta),-sin(theta)),vec2(sin(theta),cos(theta)));
  fragColor = texture(samp, R*UV * 1.0f);
}

