#version 450
// Copies deltaSR+deltaSM into S — line 5 in algorithm 4.1.
// Packs Mie red channel into alpha of Rayleigh sample.

layout(set=0, binding=0) uniform sampler2D transmittanceSampler;
layout(set=0, binding=2) uniform sampler3D deltaSRSampler;
layout(set=0, binding=3) uniform sampler3D deltaSMSampler;
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
    vec3 uvw = vec3(gl_FragCoord.xy, float(pc.layer) + 0.5)
             / vec3(float(RES_MU_S * RES_NU), float(RES_MU), float(RES_R));
    vec4 ray = texture(deltaSRSampler, uvw);
    vec4 mie = texture(deltaSMSampler, uvw);
    outColor = vec4(ray.rgb, mie.r);
}
