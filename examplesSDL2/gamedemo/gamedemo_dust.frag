#version 450

layout(location=0) in float v_life;
layout(location=1) in vec2  v_uv;
layout(location=2) in vec3  v_color;
layout(location=0) out vec4 out_color;

void main() {
    vec2  d     = v_uv * 2.0 - 1.0;
    float r     = dot(d, d);
    if (r > 1.0) discard;
    float alpha = sqrt(v_life) * (1.0 - r * r);
    out_color   = vec4(v_color * alpha, alpha);
}
