#version 450
// Computes higher order scattering deltaS — line 9 in algorithm 4.1.

layout(set=0, binding=0) uniform sampler2D transmittanceSampler;
layout(set=0, binding=4) uniform sampler3D deltaJSampler;
#include "common.glsl"

layout(push_constant) uniform PC {
    float r;
    float k;
    float first;
    int   layer;
    vec4  dhdH;
} pc;

layout(location=0) out vec4 outColor;

vec3 integrand(float r, float mu, float muS, float nu, float t) {
    float ri  = sqrt(r * r + t * t + 2.0 * r * mu * t);
    float mui = (r * mu + t) / ri;
    float muSi = (nu * t + muS * r) / ri;
    return texture4D(deltaJSampler, ri, mui, muSi, nu).rgb * transmittance(r, mu, t);
}

vec3 inscatter(float r, float mu, float muS, float nu) {
    vec3 raymie = vec3(0.0);
    float dx    = limit(r, mu) / float(INSCATTER_INTEGRAL_SAMPLES);
    float xi    = 0.0;
    vec3 raymiei = integrand(r, mu, muS, nu, 0.0);
    for (int i = 1; i <= INSCATTER_INTEGRAL_SAMPLES; ++i) {
        float xj   = float(i) * dx;
        vec3 raymiej = integrand(r, mu, muS, nu, xj);
        raymie += (raymiei + raymiej) / 2.0 * dx;
        xi      = xj;
        raymiei = raymiej;
    }
    return raymie;
}

void main() {
    float mu, muS, nu;
    getMuMuSNu(pc.r, pc.dhdH, mu, muS, nu);
    outColor = vec4(inscatter(pc.r, mu, muS, nu), 0.0);
}
