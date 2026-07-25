#version 450 core

// RCAS (Robust Contrast Adaptive Sharpening), FSR1's second pass. Ported
// to plain GLSL from the float32 path of FsrRcasCon/FsrRcasF in AMD's
// FidelityFX-FSR (MIT license):
// https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/ffx-fsr/ffx_fsr1.h
// Runs on fsr_easu.frag's output, which is already at target resolution -
// plain texelFetch, no interpolation/UV-flip needed here. The reference's
// optional noise-detection term (FSR_RCAS_DENOISE) is off by default
// upstream too, so it's left out rather than computed and unused.

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D uSource;
uniform float uSharpness;  // in stops; 0 = strongest sharpening

void main() {
    ivec2 ip = ivec2(gl_FragCoord.xy);
    // Minimal 3x3 neighborhood:
    //   b
    // d e f
    //   h
    vec3 b = texelFetch(uSource, ip + ivec2(0, -1), 0).rgb;
    vec3 d = texelFetch(uSource, ip + ivec2(-1, 0), 0).rgb;
    vec3 e = texelFetch(uSource, ip, 0).rgb;
    vec3 f = texelFetch(uSource, ip + ivec2(1, 0), 0).rgb;
    vec3 h = texelFetch(uSource, ip + ivec2(0, 1), 0).rgb;

    // Min/max of the ring (not including the center).
    vec3 mn4 = min(min(min(b, d), f), h);
    vec3 mx4 = max(max(max(b, d), f), h);

    vec3 hitMin = min(mn4, e) / (4.0 * mx4);
    vec3 hitMax = (vec3(1.0) - max(mx4, e)) / (4.0 * mn4 - 4.0);
    vec3 lobeRGB = max(-hitMin, hitMax);

    const float kLimit = 0.25 - 1.0 / 16.0;
    float lobe =
        max(-kLimit, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0)) * exp2(-uSharpness);

    float rcpL = 1.0 / (4.0 * lobe + 1.0);
    vec3 result = (lobe * (b + d + h + f) + e) * rcpL;
    fragColor = vec4(result, 1.0);
}
