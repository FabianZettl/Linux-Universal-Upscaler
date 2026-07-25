#version 450 core

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D uFontAtlas;  // single-channel, 1 = lit pixel
uniform vec3 uColor;

void main() {
    float alpha = texture(uFontAtlas, vUV).r;
    fragColor = vec4(uColor, alpha);
}
