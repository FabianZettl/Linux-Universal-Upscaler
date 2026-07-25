#version 450 core

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D uPrev;
layout(binding = 1) uniform sampler2D uCurr;
uniform int uFlipY;  // both textures come from the same capture source

// MVP placeholder: "frame generation" here is a plain 50/50 crossfade
// between the two most recent real captures, drawn on the vsync tick in
// between them instead of re-displaying a stale frame. This is the
// drop-in point for real motion-compensated interpolation (optical flow,
// LSFG-style) once that pass exists.
void main() {
    vec2 uv = uFlipY != 0 ? vec2(vUV.x, 1.0 - vUV.y) : vUV;
    fragColor = mix(texture(uPrev, uv), texture(uCurr, uv), 0.5);
}
