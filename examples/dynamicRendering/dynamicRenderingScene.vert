#version 460

layout(push_constant) uniform PC { float time; } pc;

layout(location = 0) out vec3 outColor;

out gl_PerVertex { vec4 gl_Position; };

const vec2 pos[3] = vec2[](
    vec2( 0.0,  0.5),
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5)
);
const vec3 col[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    float c = cos(pc.time);
    float s = sin(pc.time);
    vec2 p = mat2(c, s, -s, c) * pos[gl_VertexIndex];
    gl_Position = vec4(p, 0.0, 1.0);
    outColor = col[gl_VertexIndex];
}
