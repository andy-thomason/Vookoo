// Precomputed Atmospheric Scattering — Bruneton 2008
// Common constants and math functions shared by all precomputation shaders.
//
// IMPORTANT: including shaders must declare before #include "common.glsl":
//   layout(set=0, binding=N) uniform sampler2D transmittanceSampler;
// The binding index N differs between precomp shaders (N=0) and the draw
// shader (N=1, because binding 0 is the UBO).

const float Rg = 6360.0;
const float Rt = 6420.0;
const float RL = 6421.0;

const int TRANSMITTANCE_W = 256;
const int TRANSMITTANCE_H = 64;
const int SKY_W = 64;
const int SKY_H = 16;
const int RES_R = 32;
const int RES_MU = 128;
const int RES_MU_S = 32;
const int RES_NU = 8;

const float AVERAGE_GROUND_REFLECTANCE = 0.1;

const float HR = 8.0;
const vec3 betaR = vec3(5.8e-3, 1.35e-2, 3.31e-2);

const float HM = 1.2;
const vec3 betaMSca = vec3(4e-3);
const vec3 betaMEx = betaMSca / 0.9;
const float mieG = 0.8;

const int TRANSMITTANCE_INTEGRAL_SAMPLES = 500;
const int INSCATTER_INTEGRAL_SAMPLES = 50;
const int IRRADIANCE_INTEGRAL_SAMPLES = 32;
const int INSCATTER_SPHERICAL_INTEGRAL_SAMPLES = 16;

const float M_PI = 3.141592657;

#define TRANSMITTANCE_NON_LINEAR
#define INSCATTER_NON_LINEAR

// ----------------------------------------------------------------------------
// Parameterization

vec2 getTransmittanceUV(float r, float mu) {
    float uR, uMu;
#ifdef TRANSMITTANCE_NON_LINEAR
    uR  = sqrt(max(0.0, (r - Rg) / (Rt - Rg)));  // guard against fp-noise r < Rg
    uMu = atan((mu + 0.15) / (1.0 + 0.15) * tan(1.5)) / 1.5;
#else
    uR  = (r - Rg) / (Rt - Rg);
    uMu = (mu + 0.15) / (1.0 + 0.15);
#endif
    return vec2(uMu, uR);
}

void getTransmittanceRMu(out float r, out float muS) {
    r   = gl_FragCoord.y / float(TRANSMITTANCE_H);
    muS = gl_FragCoord.x / float(TRANSMITTANCE_W);
#ifdef TRANSMITTANCE_NON_LINEAR
    r   = Rg + (r * r) * (Rt - Rg);
    muS = -0.15 + tan(1.5 * muS) / tan(1.5) * (1.0 + 0.15);
#else
    r   = Rg + r * (Rt - Rg);
    muS = -0.15 + muS * (1.0 + 0.15);
#endif
}

vec2 getIrradianceUV(float r, float muS) {
    float uR   = (r - Rg) / (Rt - Rg);
    float uMuS = (muS + 0.2) / (1.0 + 0.2);
    return vec2(uMuS, uR);
}

void getIrradianceRMuS(out float r, out float muS) {
    r   = Rg + (gl_FragCoord.y - 0.5) / (float(SKY_H) - 1.0) * (Rt - Rg);
    muS = -0.2 + (gl_FragCoord.x - 0.5) / (float(SKY_W) - 1.0) * (1.0 + 0.2);
    muS -= 1.0/256.0;
}

vec4 texture4D(sampler3D table, float r, float mu, float muS, float nu) {
    float H   = sqrt(Rt * Rt - Rg * Rg);
    // Clamp to avoid sqrt(negative) when r is fractionally below Rg due to fp error
    // in the ground-intersection computation (x_atm_top + t_reduced * v).
    float rho = sqrt(max(0.0, r * r - Rg * Rg));
#ifdef INSCATTER_NON_LINEAR
    float rmu   = r * mu;
    float delta = rmu * rmu - r * r + Rg * Rg;
    vec4 cst = rmu < 0.0 && delta > 0.0
               ? vec4(1.0, 0.0, 0.0, 0.5 - 0.5 / float(RES_MU))
               : vec4(-1.0, H * H, H, 0.5 + 0.5 / float(RES_MU));
    float uR  = 0.5 / float(RES_R) + rho / H * (1.0 - 1.0 / float(RES_R));
    // For the downward case cst.z = 0, so denominator = rho; guard against 0/0 at r = Rg.
    float uMu = cst.w + (rmu * cst.x + sqrt(delta + cst.y)) / (max(rho, 0.01) + cst.z)
                * (0.5 - 1.0 / float(RES_MU));
    float uMuS = 0.5 / float(RES_MU_S)
                 + (atan(max(muS, -0.1975) * tan(1.26 * 1.1)) / 1.1 + (1.0 - 0.26))
                 * 0.5 * (1.0 - 1.0 / float(RES_MU_S));
#else
    float uR   = 0.5 / float(RES_R) + rho / H * (1.0 - 1.0 / float(RES_R));
    float uMu  = 0.5 / float(RES_MU) + (mu + 1.0) / 2.0 * (1.0 - 1.0 / float(RES_MU));
    float uMuS = 0.5 / float(RES_MU_S)
                 + max(muS + 0.2, 0.0) / 1.2 * (1.0 - 1.0 / float(RES_MU_S));
#endif
    float lerp = (nu + 1.0) / 2.0 * (float(RES_NU) - 1.0);
    float uNu  = floor(lerp);
    lerp       = lerp - uNu;
    return texture(table, vec3((uNu + uMuS) / float(RES_NU), uMu, uR)) * (1.0 - lerp)
         + texture(table, vec3((uNu + uMuS + 1.0) / float(RES_NU), uMu, uR)) * lerp;
}

void getMuMuSNu(float r, vec4 dhdH, out float mu, out float muS, out float nu) {
    float x = gl_FragCoord.x - 0.5;
    float y = gl_FragCoord.y - 0.5;
#ifdef INSCATTER_NON_LINEAR
    if (y < float(RES_MU) / 2.0) {
        float d = 1.0 - y / (float(RES_MU) / 2.0 - 1.0);
        d  = min(max(dhdH.z, d * dhdH.w), dhdH.w * 0.999);
        mu = (Rg * Rg - r * r - d * d) / (2.0 * r * d);
        mu = min(mu, -sqrt(1.0 - (Rg / r) * (Rg / r)) - 0.001);
    } else {
        float d = (y - float(RES_MU) / 2.0) / (float(RES_MU) / 2.0 - 1.0);
        d  = min(max(dhdH.x, d * dhdH.y), dhdH.y * 0.999);
        mu = (Rt * Rt - r * r - d * d) / (2.0 * r * d);
    }
    muS = mod(x, float(RES_MU_S)) / (float(RES_MU_S) - 1.0);
    muS = tan((2.0 * muS - 1.0 + 0.26) * 1.1) / tan(1.26 * 1.1);
    nu  = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;
#else
    mu  = -1.0 + 2.0 * y / (float(RES_MU) - 1.0);
    muS = mod(x, float(RES_MU_S)) / (float(RES_MU_S) - 1.0);
    muS = -0.2 + muS * 1.2;
    nu  = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;
#endif
}

// ----------------------------------------------------------------------------
// Utility

float limit(float r, float mu) {
    float dout   = -r * mu + sqrt(r * r * (mu * mu - 1.0) + RL * RL);
    float delta2 = r * r * (mu * mu - 1.0) + Rg * Rg;
    if (delta2 >= 0.0) {
        float din = -r * mu - sqrt(delta2);
        if (din >= 0.0) { dout = min(dout, din); }
    }
    return dout;
}

vec3 transmittance(float r, float mu) {
    vec2 uv = getTransmittanceUV(r, mu);
    return texture(transmittanceSampler, uv).rgb;
}

vec3 transmittanceWithShadow(float r, float mu) {
    return mu < -sqrt(1.0 - (Rg / r) * (Rg / r)) ? vec3(0.0) : transmittance(r, mu);
}

vec3 transmittance(float r, float mu, vec3 v, vec3 x0) {
    float r1  = length(x0);
    float mu1 = dot(x0, v) / r;
    if (mu > 0.0) {
        return min(transmittance(r, mu) / transmittance(r1, mu1), 1.0);
    } else {
        return min(transmittance(r1, -mu1) / transmittance(r, -mu), 1.0);
    }
}

float opticalDepth(float H, float r, float mu, float d) {
    float a    = sqrt((0.5/H)*r);
    vec2 a01   = a * vec2(mu, mu + d / r);
    vec2 a01s  = sign(a01);
    vec2 a01sq = a01 * a01;
    float x    = a01s.y > a01s.x ? exp(a01sq.x) : 0.0;
    vec2 y     = a01s / (2.3193 * abs(a01) + sqrt(1.52 * a01sq + 4.0))
               * vec2(1.0, exp(-d/H*(d/(2.0*r)+mu)));
    return sqrt((6.2831*H)*r) * exp((Rg-r)/H) * (x + dot(y, vec2(1.0, -1.0)));
}

vec3 analyticTransmittance(float r, float mu, float d) {
    return exp(- betaR * opticalDepth(HR, r, mu, d) - betaMEx * opticalDepth(HM, r, mu, d));
}

vec3 transmittance(float r, float mu, float d) {
    float r1  = sqrt(r * r + d * d + 2.0 * r * mu * d);
    float mu1 = (r * mu + d) / r1;
    if (mu > 0.0) {
        return min(transmittance(r, mu) / transmittance(r1, mu1), 1.0);
    } else {
        return min(transmittance(r1, -mu1) / transmittance(r, -mu), 1.0);
    }
}

vec3 irradiance(sampler2D irrTex, float r, float muS) {
    vec2 uv = getIrradianceUV(r, muS);
    return texture(irrTex, uv).rgb;
}

float phaseFunctionR(float mu) {
    return (3.0 / (16.0 * M_PI)) * (1.0 + mu * mu);
}

float phaseFunctionM(float mu) {
    return 1.5 * 1.0 / (4.0 * M_PI) * (1.0 - mieG*mieG)
           * pow(1.0 + (mieG*mieG) - 2.0*mieG*mu, -3.0/2.0)
           * (1.0 + mu * mu) / (2.0 + mieG*mieG);
}

vec3 getMie(vec4 rayMie) {
    return rayMie.rgb * rayMie.w / max(rayMie.r, 1e-4) * (betaR.r / betaR);
}
