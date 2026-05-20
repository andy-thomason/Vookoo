#version 450
// Atmosphere display — Eq (16) in Bruneton 2008.
// transmittanceSampler is at binding=1 here (binding=0 is the UBO).

layout(set=0, binding=0) uniform DrawUBO {
    vec3  c;          float pad0;
    vec3  s;          float exposure;
    mat4  projInverse;
    mat4  viewInverse;
} u;

layout(set=0, binding=1) uniform sampler2D transmittanceSampler;
layout(set=0, binding=2) uniform sampler2D irradianceSampler;
layout(set=0, binding=3) uniform sampler3D inscatterSampler;
layout(set=0, binding=4) uniform sampler2D reflectanceSampler;

#include "common.glsl"

layout(location=0) in  vec2 inCoords;
layout(location=1) in  vec3 inRay;
layout(location=0) out vec4 outColor;

#define FIX
const float ISun = 100.0;

vec3 inscatterColor(inout vec3 x, inout float t, vec3 v, vec3 s,
                    out float r, out float mu, out vec3 attenuation) {
    attenuation = vec3(1.0);
    vec3 result;
    r  = length(x);
    mu = dot(x, v) / r;
    float d = -r * mu - sqrt(r * r * (mu * mu - 1.0) + Rt * Rt);
    if (d > 0.0) {
        x += d * v;
        t -= d;
        mu = (r * mu + d) / Rt;
        r  = Rt;
    }
    if (r <= Rt) {
        float nu    = dot(v, s);
        float muS   = dot(x, s) / r;
        float phaseR = phaseFunctionR(nu);
        float phaseM = phaseFunctionM(nu);
        vec4 insc   = max(texture4D(inscatterSampler, r, mu, muS, nu), 0.0);
        if (t > 0.0) {
            vec3 x0    = x + t * v;
            float r0   = length(x0);
            float rMu0 = dot(x0, v);
            float mu0  = rMu0 / r0;
            float muS0 = dot(x0, s) / r0;
#ifdef FIX
            attenuation = analyticTransmittance(r, mu, t);
#else
            attenuation = transmittance(r, mu, v, x0);
#endif
            if (r0 > Rg + 0.01) {
                insc = max(insc - attenuation.rgbr * texture4D(inscatterSampler, r0, mu0, muS0, nu), 0.0);
#ifdef FIX
                const float EPS    = 0.004;
                float muHoriz = -sqrt(1.0 - (Rg / r) * (Rg / r));
                if (abs(mu - muHoriz) < EPS) {
                    float a = ((mu - muHoriz) + EPS) / (2.0 * EPS);
                    mu = muHoriz - EPS;
                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);
                    mu0 = (r * mu + t) / r0;
                    vec4 inScat0 = texture4D(inscatterSampler, r, mu, muS, nu);
                    vec4 inScat1 = texture4D(inscatterSampler, r0, mu0, muS0, nu);
                    vec4 inScatA = max(inScat0 - attenuation.rgbr * inScat1, 0.0);
                    mu = muHoriz + EPS;
                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);
                    mu0 = (r * mu + t) / r0;
                    inScat0 = texture4D(inscatterSampler, r, mu, muS, nu);
                    inScat1 = texture4D(inscatterSampler, r0, mu0, muS0, nu);
                    vec4 inScatB = max(inScat0 - attenuation.rgbr * inScat1, 0.0);
                    insc = mix(inScatA, inScatB, a);
                }
#endif
            }
        }
#ifdef FIX
        insc.w *= smoothstep(0.00, 0.02, muS);
#endif
        result = max(insc.rgb * phaseR + getMie(insc) * phaseM, 0.0);
    } else {
        result = vec3(0.0);
    }
    return result * ISun;
}

vec3 groundColor(vec3 x, float t, vec3 v, vec3 s, float r, float mu, vec3 attenuation) {
    if (!(t > 0.0)) { return vec3(0.0); }
    vec3 x0   = x + t * v;
    float r0  = length(x0);
    vec3 n    = x0 / r0;
    vec2 coords = vec2(atan(n.y, n.x), acos(n.z)) * vec2(0.5, 1.0) / M_PI + vec2(0.5, 0.0);
    vec4 reflectance = texture(reflectanceSampler, coords) * vec4(0.2, 0.2, 0.2, 1.0);
    if (r0 > Rg + 0.01) {
        reflectance = vec4(0.4, 0.4, 0.4, 0.0);
    }
    float muS    = dot(n, s);
    vec3 sunLight = transmittanceWithShadow(r0, muS);
    vec3 groundSkyLight = irradiance(irradianceSampler, r0, muS);
    vec3 gColor = reflectance.rgb * (max(muS, 0.0) * sunLight + groundSkyLight) * ISun / M_PI;
    if (reflectance.w > 0.0) {
        vec3 h       = normalize(s - v);
        float fresnel = 0.02 + 0.98 * pow(1.0 - dot(-v, h), 5.0);
        float wBrdf  = fresnel * pow(max(dot(h, n), 0.0), 150.0);
        gColor += reflectance.w * max(wBrdf, 0.0) * sunLight * ISun;
    }
    return attenuation * gColor;
}

vec3 sunColor(vec3 x, float t, vec3 v, vec3 s, float r, float mu) {
    if (t > 0.0) { return vec3(0.0); }
    vec3 tr = r <= Rt ? transmittanceWithShadow(r, mu) : vec3(1.0);
    float isun = step(cos(M_PI / 180.0), dot(v, s)) * ISun;
    return tr * isun;
}

vec3 HDR(vec3 L) {
    L = L * u.exposure;
    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.b);
    return L;
}

void main() {
    vec3 x = u.c;
    vec3 v = normalize(inRay);

    float r    = length(x);
    float mu   = dot(x, v) / r;
    float disc = r * r * (mu * mu - 1.0) + Rg * Rg;
    float t    = disc > 0.0 ? -r * mu - sqrt(disc) : -1.0;

    vec3 attenuation;
    vec3 iColor = inscatterColor(x, t, v, u.s, r, mu, attenuation);
    vec3 gColor = groundColor(x, t, v, u.s, r, mu, attenuation);
    vec3 sunCol = sunColor(x, t, v, u.s, r, mu);
    outColor = vec4(HDR(sunCol + gColor + iColor), 1.0);
}
