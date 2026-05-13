#version 450

layout(set=0, binding=0) uniform sampler2D u_scene;
layout(push_constant) uniform PC { float u_time; } pc;

layout(location=0) in  vec2 v_uv;
layout(location=0) out vec4 out_color;

float hash(vec2 p) {
    p = fract(p * vec2(443.8975, 397.2973));
    p += dot(p, p + 19.19);
    return fract(p.x * p.y);
}

// Barrel distortion — k > 0 = pincushion, k < 0 = barrel.
vec2 barrel(vec2 uv, float k) {
    vec2 d = uv * 2.0 - 1.0;
    d *= 1.0 + k * dot(d, d);
    return d * 0.5 + 0.5;
}

void main() {
    vec2 uv = barrel(v_uv, 0.07);

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // 11x11 Gaussian bloom (sigma=3).
    vec2  texel = 1.0 / vec2(textureSize(u_scene, 0));
    vec3  bloom = vec3(0.0);
    float w_sum = 0.0;
    for (int x = -5; x <= 5; ++x) {
        for (int y = -5; y <= 5; ++y) {
            float w = exp(-0.5 * float(x*x + y*y) / 9.0);
            bloom += texture(u_scene, uv + vec2(x, y) * texel).rgb * w;
            w_sum += w;
        }
    }
    bloom /= w_sum;

    // Chromatic aberration — offset scales with distance from centre.
    vec2 ca = (uv - 0.5) * 0.02;
    vec3 sharp = vec3(
        texture(u_scene, uv + ca).r,
        texture(u_scene, uv).g,
        texture(u_scene, uv - ca).b);
    vec3 color = sharp + bloom * 4.0;   // sharp image + glow

    // Film grain — animated per-frame via u_time.
    float grain = (hash(gl_FragCoord.xy + pc.u_time) - 0.5) * 0.14;
    color += grain;

    // Subtle phosphor warmth — barely-there green without washing out colours.
    color *= vec3(0.88, 1.0, 0.82);

    // Vignette — starts further out, caps at 50% so edges stay visible.
    vec2 d = v_uv * 2.0 - 1.0;
    color *= 1.0 - smoothstep(0.75, 1.4, length(d)) * 0.5;

    // Interference bands — slow horizontal ripple.
    color *= 1.0 + sin(v_uv.y * 15.0 + pc.u_time * 1.2) * 0.12;

    // Screen flicker — per-frame brightness wobble.
    color *= 0.88 + hash(vec2(pc.u_time)) * 0.18;

    out_color = vec4(color, 1.0);
}
