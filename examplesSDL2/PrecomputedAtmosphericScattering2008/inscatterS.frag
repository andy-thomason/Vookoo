#version 450
// Computes deltaJ — line 7 in algorithm 4.1.

layout(set=0, binding=0) uniform sampler2D transmittanceSampler;
layout(set=0, binding=1) uniform sampler2D deltaESampler;
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

const float dphi   = M_PI / float(INSCATTER_SPHERICAL_INTEGRAL_SAMPLES);
const float dtheta = M_PI / float(INSCATTER_SPHERICAL_INTEGRAL_SAMPLES);

void inscatter(float r, float mu, float muS, float nu, out vec3 raymie) {
    r    = clamp(r,   Rg, Rt);
    mu   = clamp(mu,  -1.0, 1.0);
    muS  = clamp(muS, -1.0, 1.0);
    float var = sqrt(1.0 - mu * mu) * sqrt(1.0 - muS * muS);
    nu   = clamp(nu, muS * mu - var, muS * mu + var);

    float cthetamin = -sqrt(1.0 - (Rg / r) * (Rg / r));

    vec3 v  = vec3(sqrt(1.0 - mu * mu), 0.0, mu);
    float sx = v.x == 0.0 ? 0.0 : (nu - muS * mu) / v.x;
    vec3 s  = vec3(sx, sqrt(max(0.0, 1.0 - sx * sx - muS * muS)), muS);

    raymie = vec3(0.0);

    for (int itheta = 0; itheta < INSCATTER_SPHERICAL_INTEGRAL_SAMPLES; ++itheta) {
        float theta  = (float(itheta) + 0.5) * dtheta;
        float ctheta = cos(theta);

        float greflectance = 0.0;
        float dground      = 0.0;
        vec3  gtransp      = vec3(0.0);
        if (ctheta < cthetamin) {
            greflectance = AVERAGE_GROUND_REFLECTANCE / M_PI;
            dground = -r * ctheta - sqrt(r * r * (ctheta * ctheta - 1.0) + Rg * Rg);
            gtransp = transmittance(Rg, -(r * ctheta + dground) / Rg, dground);
        }

        for (int iphi = 0; iphi < 2 * INSCATTER_SPHERICAL_INTEGRAL_SAMPLES; ++iphi) {
            float phi = (float(iphi) + 0.5) * dphi;
            float dw  = dtheta * dphi * sin(theta);
            vec3 w    = vec3(cos(phi) * sin(theta), sin(phi) * sin(theta), ctheta);

            float nu1  = dot(s, w);
            float nu2  = dot(v, w);
            float pr2  = phaseFunctionR(nu2);
            float pm2  = phaseFunctionM(nu2);

            vec3 gnormal     = (vec3(0.0, 0.0, r) + dground * w) / Rg;
            vec3 girradiance = irradiance(deltaESampler, Rg, dot(gnormal, s));

            vec3 raymie1 = greflectance * girradiance * gtransp;

            if (pc.first == 1.0) {
                float pr1  = phaseFunctionR(nu1);
                float pm1  = phaseFunctionM(nu1);
                vec3 ray1  = texture4D(deltaSRSampler, r, w.z, muS, nu1).rgb;
                vec3 mie1  = texture4D(deltaSMSampler, r, w.z, muS, nu1).rgb;
                raymie1 += ray1 * pr1 + mie1 * pm1;
            } else {
                raymie1 += texture4D(deltaSRSampler, r, w.z, muS, nu1).rgb;
            }

            raymie += raymie1
                    * (betaR * exp(-(r - Rg) / HR) * pr2
                     + betaMSca * exp(-(r - Rg) / HM) * pm2) * dw;
        }
    }
}

void main() {
    vec3 raymie;
    float mu, muS, nu;
    getMuMuSNu(pc.r, pc.dhdH, mu, muS, nu);
    inscatter(pc.r, mu, muS, nu, raymie);
    outColor = vec4(raymie, 0.0);
}
