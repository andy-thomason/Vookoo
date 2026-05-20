#version 460

layout(binding = 0) uniform sampler2D iChannel1; // E field (e.z is displayed)

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

// Viridis colormap (identical to fdtd2dUpmlpass2.frag)
vec4 color_map(float s, float div) {
    float t = s/div*0.5+0.5;

    const vec3 c0 = vec3(0.2777273272234177,  0.005407344544966578, 0.3340998053353061);
    const vec3 c1 = vec3(0.1050930431085774,  1.404613529898575,    1.384590162594685);
    const vec3 c2 = vec3(-0.3308618287255563,  0.214847559468213,    0.09509516302823659);
    const vec3 c3 = vec3(-4.634230498983486,  -5.799100973351585,  -19.33244095627987);
    const vec3 c4 = vec3( 6.228269936347081,  14.17993336680509,    56.69055260068105);
    const vec3 c5 = vec3( 4.776384997670288, -13.74514537774601,   -65.35303263337234);
    const vec3 c6 = vec3(-5.435455855934631,   4.645852612178535,   26.3124352495832);

    return vec4(c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6))))), 1.0);
}

void main() {
    vec4 e = texture(iChannel1, inUV);
    outColor = color_map(e.z, 1.0);
}
