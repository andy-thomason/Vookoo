#version 450
// Copies deltaE into E — lines 4 and 10 in algorithm 4.1.
// k=1 on line 4 (overwrite); additive blend enabled on line 10 (accumulate).

layout(set=0, binding=0) uniform sampler2D transmittanceSampler;
layout(set=0, binding=1) uniform sampler2D deltaESampler;
#include "common.glsl"

layout(push_constant) uniform PC {
    float r;
    float k;
    float first;
    int   layer;
    vec4  dhdH;
} pc;

layout(location=0) out vec4 outColor;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(SKY_W, SKY_H);
    outColor = pc.k * texture(deltaESampler, uv);
}
