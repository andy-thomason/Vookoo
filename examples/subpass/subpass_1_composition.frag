#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 v2fCol;
layout(location = 1) in vec2 v2fUV;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColorAttachment;

void main() {

    outColor = vec4(0.0, 0.0, 0.0, 0.0);
    
    // accumulate color and bloom attachments
    vec4 inCol = subpassLoad(inputColorAttachment);

    float exposure = 1.0;
    // apply tonemapping
    vec4 remapped = vec4(1.0) - exp(-inCol * exposure);
    // output to swapchain image
    outColor += remapped;
    
}
