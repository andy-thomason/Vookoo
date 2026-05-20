#version 450
// Adds deltaS into S — line 11 in algorithm 4.1.
// Uses additive blend (ONE+ONE) enabled at the pipeline level.

layout(set=0, binding=0) uniform sampler2D transmittanceSampler;
layout(set=0, binding=5) uniform sampler3D deltaSSampler;
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
    float mu, muS, nu;
    getMuMuSNu(pc.r, pc.dhdH, mu, muS, nu);
    vec3 uvw = vec3(gl_FragCoord.xy, float(pc.layer) + 0.5)
             / vec3(float(RES_MU_S * RES_NU), float(RES_MU), float(RES_R));
    outColor = vec4(texture(deltaSSampler, uvw).rgb / phaseFunctionR(nu), 0.0);
}
