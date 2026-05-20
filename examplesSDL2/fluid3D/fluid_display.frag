#version 450

// Ray-march through the 3D fluid volume.
// stateA.w = density, stateB.x = temperature.
layout(binding = 0) uniform sampler3D stateATex;   // (vel.xyz, density)
layout(binding = 1) uniform sampler3D stateBTex;   // (temperature, ...)

layout(push_constant) uniform CameraPC {
    vec4 eye;
    mat4 invVP;
} cam;

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

// Ray vs axis-aligned box intersection; returns (tEnter, tExit).
vec2 intersectAABB(vec3 ro, vec3 rd, vec3 bMin, vec3 bMax) {
    vec3 t0 = (bMin - ro) / rd;
    vec3 t1 = (bMax - ro) / rd;
    return vec2(max(max(min(t0.x, t1.x), min(t0.y, t1.y)), min(t0.z, t1.z)),
                min(min(max(t0.x, t1.x), max(t0.y, t1.y)), max(t0.z, t1.z)));
}

void main() {
    // Reconstruct world-space ray from the inverse view-projection.
    vec2  ndc = inUV * 2.0 - 1.0;

    vec4 wNear = cam.invVP * vec4(ndc, -1.0, 1.0);
    wNear /= wNear.w;
    vec4 wFar  = cam.invVP * vec4(ndc,  1.0, 1.0);
    wFar  /= wFar.w;

    vec3 ro = cam.eye.xyz;
    vec3 rd = normalize(wFar.xyz - wNear.xyz);

    // Volume occupies [-0.5, 0.5]³ in world space.
    vec2 t = intersectAABB(ro, rd, vec3(-0.5), vec3(0.5));

    vec3 accumulated = vec3(0.0);
    float alpha      = 0.0;

    if (t.x < t.y) {
        const int   MAX_STEPS  = 128;
        const float STEP_SIZE  = 1.5 / float(MAX_STEPS);   // covers full diagonal
        const float ABSORPTION = 30.0;

        float jitter = fract(sin(dot(ndc, vec2(12.9898, 78.233))) * 43758.5453);
        float tCur = max(t.x, 0.0) + jitter * STEP_SIZE;

        for (int i = 0; i < MAX_STEPS && alpha < 0.95; ++i) {
            if (tCur > t.y) break;

            vec3 pos = ro + tCur * rd;                  // world position
            vec3 uvw = pos + vec3(0.5);                 // map [-0.5,0.5]→[0,1]

            float density = max(texture(stateATex, uvw).w, 0.0);
            float temp    = max(texture(stateBTex, uvw).x, 0.0);

            float sampleAlpha = clamp(density * STEP_SIZE * ABSORPTION, 0.0, 1.0);
            if (sampleAlpha > 0.001) {
                // Cool blue-white smoke blending toward orange heat
                vec3 smokeCol = mix(vec3(0.80, 0.90, 1.00),
                                    vec3(1.00, 0.50, 0.10),
                                    clamp(temp * 0.08, 0.0, 1.0));
                //vec3 smokeCol = vec3(0.3); // Dark smoke
                vec3 glow = vec3(1.0, 0.5, 0.1) * pow(temp * 0.1, 2.0); // Hot glow

                // Front-to-back compositing
                //accumulated += (1.0 - alpha) * sampleAlpha * smokeCol;
                accumulated += (1.0 - alpha) * sampleAlpha * (smokeCol + glow);
                alpha       += (1.0 - alpha) * sampleAlpha;
            }

            tCur += STEP_SIZE;
        }
    }

    // Dark background
    outColor = vec4(accumulated + (1.0 - alpha) * vec3(0.0, 0.125, 0.25), 1.0);
}
