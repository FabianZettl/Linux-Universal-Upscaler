#version 450 core

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D uSource;
uniform int uFlipY;  // wlr-screencopy's y_invert flag, per captured frame

// MVP placeholder: the "upscale" here is GPU-filtered texture sampling
// (GL_NEAREST/GL_LINEAR, chosen by texture.cpp from upscale_mode) into a
// larger target-resolution framebuffer. This is the drop-in point for a
// real spatial upscaler (FSR EASU/RCAS) once that pass exists.
void main() {
    vec2 uv = uFlipY != 0 ? vec2(vUV.x, 1.0 - vUV.y) : vUV;
    fragColor = texture(uSource, uv);
}
