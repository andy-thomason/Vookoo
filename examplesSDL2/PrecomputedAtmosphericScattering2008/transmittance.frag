#version 450
// Computes transmittance table T — Eq (5) in Bruneton 2008.

layout(set=0, binding=0) uniform sampler2D transmittanceSampler;
#include "common.glsl"

layout(location=0) out vec4 outColor;

float opticalDepthTrans(float H, float r, float mu) {
    float result = 0.0;
    float dx = limit(r, mu) / float(TRANSMITTANCE_INTEGRAL_SAMPLES);
    float xi = 0.0;
    float yi = exp(-(r - Rg) / H);
    for (int i = 1; i <= TRANSMITTANCE_INTEGRAL_SAMPLES; ++i) {
        float xj = float(i) * dx;
        float yj = exp(-(sqrt(r * r + xj * xj + 2.0 * xj * r * mu) - Rg) / H);
        result += (yi + yj) / 2.0 * dx;
        xi = xj;
        yi = yj;
    }
    return mu < -sqrt(1.0 - (Rg / r) * (Rg / r)) ? 1e9 : result;
}

void main() {
    float r, muS;
    getTransmittanceRMu(r, muS);
    vec3 depth = betaR * opticalDepthTrans(HR, r, muS) + betaMEx * opticalDepthTrans(HM, r, muS);
    outColor = vec4(exp(-depth), 0.0);
}
