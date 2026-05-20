#version 450

layout(location=0) in flat float v_color;
layout(location=0) out vec4 out_color;

void main() {
    // 0=white(ship)  1=yellow(rocks)  2=cyan(bullets)  3=red(unused)
    const vec3 palette[4] = vec3[4](
        vec3(1.00, 1.00, 1.00),
        vec3(1.00, 0.85, 0.25),
        vec3(0.25, 0.90, 1.00),
        vec3(1.00, 0.25, 0.25)
    );
    out_color = vec4(palette[clamp(int(v_color), 0, 3)], 1.0);
}
