#version 460

layout(binding = 0) uniform sampler2D offscreen;

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 d = 1.0 / textureSize(offscreen, 0);

    // The core idea: sample the 8 neighbours around the current texel, apply a     
    //   Sobel kernel to get horizontal/vertical gradient magnitude, output that as the edge strength
    vec3 tl = texture(offscreen, inUV + d * vec2(-1,-1)).rgb;
    vec3 tc = texture(offscreen, inUV + d * vec2( 0,-1)).rgb;
    vec3 tr = texture(offscreen, inUV + d * vec2( 1,-1)).rgb;
    vec3 ml = texture(offscreen, inUV + d * vec2(-1, 0)).rgb;
    vec3 mr = texture(offscreen, inUV + d * vec2( 1, 0)).rgb;
    vec3 bl = texture(offscreen, inUV + d * vec2(-1, 1)).rgb;
    vec3 bc = texture(offscreen, inUV + d * vec2( 0, 1)).rgb;
    vec3 br = texture(offscreen, inUV + d * vec2( 1, 1)).rgb;

    // Per-channel Sobel — max gradient across R, G, B catches low-luma colour edges (e.g. blue).
    vec3 gx = -tl - 2*ml - bl + tr + 2*mr + br;
    vec3 gy = -tl - 2*tc - tr + bl + 2*bc + br;
    float edge = max(length(vec2(gx.r, gy.r)),
                 max(length(vec2(gx.g, gy.g)),
                     length(vec2(gx.b, gy.b))));

    vec3 orig = texture(offscreen, inUV).rgb;
    float e = smoothstep(0.2, 0.25, edge);   // narrow band → sharp snap
    outColor = vec4(mix(orig, vec3(1.0), e), 1.0);  // white edges
}
