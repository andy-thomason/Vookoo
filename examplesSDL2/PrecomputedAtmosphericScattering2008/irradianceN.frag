#version 450
// Computes ground irradiance due to skylight E[deltaS] — line 8 in algorithm 4.1.

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

const float dphi   = M_PI / float(IRRADIANCE_INTEGRAL_SAMPLES);
const float dtheta = M_PI / float(IRRADIANCE_INTEGRAL_SAMPLES);

void main() {
    float r, muS;
    getIrradianceRMuS(r, muS);
    vec3 s = vec3(sqrt(1.0 - muS * muS), 0.0, muS);

    vec3 result = vec3(0.0);
    for (int iphi = 0; iphi < 2 * IRRADIANCE_INTEGRAL_SAMPLES; ++iphi) {
        float phi  = (float(iphi) + 0.5) * dphi;
        for (int itheta = 0; itheta < IRRADIANCE_INTEGRAL_SAMPLES / 2; ++itheta) {
            float theta = (float(itheta) + 0.5) * dtheta;
            float dw    = dtheta * dphi * sin(theta);
            vec3 w      = vec3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
            float nu    = dot(s, w);
            if (pc.first == 1.0) {
                float pr1  = phaseFunctionR(nu);
                float pm1  = phaseFunctionM(nu);
                vec3 ray1  = texture4D(deltaSRSampler, r, w.z, muS, nu).rgb;
                vec3 mie1  = texture4D(deltaSMSampler, r, w.z, muS, nu).rgb;
                result += (ray1 * pr1 + mie1 * pm1) * w.z * dw;
            } else {
                result += texture4D(deltaSRSampler, r, w.z, muS, nu).rgb * w.z * dw;
            }
        }
    }
    outColor = vec4(result, 0.0);
}
