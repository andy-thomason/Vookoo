#version 450
// Modified from webgl-noise — https://github.com/ashima/webgl-noise

layout(location = 0) in  vec3 v_coord;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PC {
    layout(offset = 64) float time;
} pc;

// ---------------------------------------------------------------------------
// 2D Simplex noise — inlined from webgl-noise/src/noise2D.glsl
// Original authors: Ian McEwan, Ashima Arts.  MIT License.
// https://github.com/stegu/webgl-noise
// ---------------------------------------------------------------------------

vec3 mod289(vec3 x) { return x - floor(x * (1.0/289.0)) * 289.0; }
vec2 mod289(vec2 x) { return x - floor(x * (1.0/289.0)) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x*34.0)+10.0)*x); }

float snoise(vec2 v) {
    const vec4 C = vec4(0.211324865405187, 0.366025403784439,
                       -0.577350269189626, 0.024390243902439);
    vec2 i  = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);
    vec2 i1  = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod289(i);
    vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));
    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
    m = m*m; m = m*m;
    vec3 x  = 2.0 * fract(p * C.www) - 1.0;
    vec3 h  = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * (a0*a0 + h*h);
    vec3 g;
    g.x  = a0.x  * x0.x   + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

// ---------------------------------------------------------------------------

void main() {
    vec2  uv = v_coord.xy;
    float t  = pc.time;

    float n = snoise(uv           - vec2(0.0, t));
    n += 0.5000 * snoise(uv *  2.0 - vec2(0.0, t * 1.4));
    n += 0.2500 * snoise(uv *  4.0 - vec2(0.0, t * 2.0));
    n += 0.1250 * snoise(uv *  8.0 - vec2(0.0, t * 2.8));
    n *= 0.7;

    fragColor = vec4(vec3(n * 0.5 + 0.5), 1.0);
}
