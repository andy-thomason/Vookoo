#version 450
// Computes ground irradiance due to direct sunlight E[L0] — line 2 in algorithm 4.1.

layout(set=0, binding=0) uniform sampler2D transmittanceSampler;
#include "common.glsl"

layout(location=0) out vec4 outColor;

void main() {
    float r, muS;
    getIrradianceRMuS(r, muS);
    outColor = vec4(transmittance(r, muS) * max(muS, 0.0), 0.0);
}
