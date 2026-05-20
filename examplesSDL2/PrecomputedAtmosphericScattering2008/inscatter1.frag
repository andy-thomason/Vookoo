#version 450
// Computes single scattering deltaSR + deltaSM — line 3 in algorithm 4.1.
// Dual output: location 0 = Rayleigh (deltaSR), location 1 = Mie (deltaSM).
// Geometry shader removed: layer index comes via push constant.

layout(set=0, binding=0) uniform sampler2D transmittanceSampler;
#include "common.glsl"

layout(push_constant) uniform PC {
    float r;
    float k;
    float first;
    int   layer;
    vec4  dhdH;
} pc;

layout(location=0) out vec4 outRay;
layout(location=1) out vec4 outMie;

void integrand(float r, float mu, float muS, float nu, float t,
               out vec3 ray, out vec3 mie) {
    ray = vec3(0.0);
    mie = vec3(0.0);
    float ri   = sqrt(r * r + t * t + 2.0 * r * mu * t);
    float muSi = (nu * t + muS * r) / ri;
    ri = max(Rg, ri);
    if (muSi >= -sqrt(1.0 - Rg * Rg / (ri * ri))) {
        vec3 ti = transmittance(r, mu, t) * transmittance(ri, muSi);
        ray = exp(-(ri - Rg) / HR) * ti;
        mie = exp(-(ri - Rg) / HM) * ti;
    }
}

void inscatter(float r, float mu, float muS, float nu,
               out vec3 ray, out vec3 mie) {
    ray = vec3(0.0);
    mie = vec3(0.0);
    float dx = limit(r, mu) / float(INSCATTER_INTEGRAL_SAMPLES);
    float xi = 0.0;
    vec3 rayi, miei;
    integrand(r, mu, muS, nu, 0.0, rayi, miei);
    for (int i = 1; i <= INSCATTER_INTEGRAL_SAMPLES; ++i) {
        float xj = float(i) * dx;
        vec3 rayj, miej;
        integrand(r, mu, muS, nu, xj, rayj, miej);
        ray += (rayi + rayj) / 2.0 * dx;
        mie += (miei + miej) / 2.0 * dx;
        xi   = xj;
        rayi = rayj;
        miei = miej;
    }
    ray *= betaR;
    mie *= betaMSca;
}

void main() {
    vec3 ray, mie;
    float mu, muS, nu;
    getMuMuSNu(pc.r, pc.dhdH, mu, muS, nu);
    inscatter(pc.r, mu, muS, nu, ray, mie);
    outRay = vec4(ray, 0.0);
    outMie = vec4(mie, 0.0);
}
