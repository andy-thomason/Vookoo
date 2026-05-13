#version 450

layout(set=1, binding=0) uniform sampler2D u_atlas;

layout(location=0) in  vec2 v_uv;
layout(location=0) out vec4 out_color;

void main() {
    float d     = texture(u_atlas, v_uv).r;
    float w     = clamp(fwidth(d), 0.001, 0.5);
    float alpha = smoothstep(0.5 - w, 0.5 + w, d);
    out_color   = vec4(0.75, 1.0, 0.85, alpha);  // pale green sci-fi tint
}
