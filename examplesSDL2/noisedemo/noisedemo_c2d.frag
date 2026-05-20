#version 450
// Modified from webgl-noise — https://github.com/ashima/webgl-noise

layout(location = 0) in  vec3 v_coord;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PC {
    layout(offset = 64) float time;
} pc;

// ---------------------------------------------------------------------------
// 2D Classic Perlin noise — inlined from webgl-noise/src/classicnoise2D.glsl
// Author: Stefan Gustavson (stefan.gustavson@liu.se).  MIT License.
// https://github.com/stegu/webgl-noise
// ---------------------------------------------------------------------------

vec4 mod289(vec4 x) { return x - floor(x * (1.0/289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x*34.0)+10.0)*x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec2 fade(vec2 t) { return t*t*t*(t*(t*6.0-15.0)+10.0); }

float cnoise(vec2 P) {
    vec4 Pi  = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 Pf  = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi = mod289(Pi);
    vec4 ix = Pi.xzxz, iy = Pi.yyww;
    vec4 fx = Pf.xzxz, fy = Pf.yyww;
    vec4 i  = permute(permute(ix) + iy);
    vec4 gx = fract(i * (1.0/41.0)) * 2.0 - 1.0;
    vec4 gy = abs(gx) - 0.5;
    vec4 tx = floor(gx + 0.5);
    gx -= tx;
    vec2 g00 = vec2(gx.x, gy.x), g10 = vec2(gx.y, gy.y);
    vec2 g01 = vec2(gx.z, gy.z), g11 = vec2(gx.w, gy.w);
    vec4 norm = taylorInvSqrt(vec4(dot(g00,g00),dot(g01,g01),dot(g10,g10),dot(g11,g11)));
    float n00 = norm.x * dot(g00, vec2(fx.x, fy.x));
    float n10 = norm.y * dot(g10, vec2(fx.y, fy.y));
    float n01 = norm.z * dot(g01, vec2(fx.z, fy.z));
    float n11 = norm.w * dot(g11, vec2(fx.w, fy.w));
    vec2 fade_xy = fade(Pf.xy);
    vec2 n_x = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);
    return 2.3 * mix(n_x.x, n_x.y, fade_xy.y);
}

// ---------------------------------------------------------------------------

void main() {
    vec2  uv = v_coord.xy;
    float t  = pc.time;

    float n = cnoise(uv           - vec2(0.0, t));
    n += 0.5000 * cnoise(uv *  2.0 - vec2(0.0, t * 1.4));
    n += 0.2500 * cnoise(uv *  4.0 - vec2(0.0, t * 2.0));
    n += 0.1250 * cnoise(uv *  8.0 - vec2(0.0, t * 2.8));
    n *= 0.7;

    fragColor = vec4(vec3(n * 0.5 + 0.5), 1.0);
}
