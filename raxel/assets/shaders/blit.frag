#version 450

layout(location = 0) out vec4 outColor;

// Descriptor set #0, binding #0 as a combined image sampler
layout(set = 0, binding = 0) uniform sampler2D inputImage;

void main() {
    outColor = texture(inputImage, gl_FragCoord.xy / vec2(800.0, 600.0));
}
