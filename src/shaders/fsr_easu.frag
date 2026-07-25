#version 450 core

// EASU (Edge Adaptive Spatial Upsampling), the first of FSR1's two passes.
// Ported to plain GLSL from the float32 path of FsrEasuCon/FsrEasuF/
// FsrEasuSetF/FsrEasuTapF in AMD's FidelityFX-FSR (MIT license):
// https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/ffx-fsr/ffx_fsr1.h
// The constant setup (con0..con3 there) is computed inline per-fragment
// from uSrcSize/uDstSize instead of AMD's precomputed bit-packed constant
// buffers - those exist for compute-shader dispatch efficiency, irrelevant
// to a single fragment-shader pass here. Output feeds fsr_rcas.frag.
//
// Samples the 12-tap kernel via texelFetch at explicit integer offsets
// rather than AMD's textureGather-based approach: HLSL Gather() and GLSL
// textureGather don't necessarily return their 2x2 texel quad in the same
// component order, and getting that silently wrong produces exactly the
// kind of periodic row-interlace artifact this shader had before this
// rewrite - texelFetch sidesteps the whole question.

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D uSource;
uniform vec2 uSrcSize;  // capture resolution, texels
uniform vec2 uDstSize;  // target resolution, texels
uniform int uFlipY;

vec3 fetchTap(ivec2 base, int dx, int dy) {
    ivec2 c = base + ivec2(dx, dy);
    if (uFlipY != 0) c.y = int(uSrcSize.y) - 1 - c.y;
    c = clamp(c, ivec2(0), ivec2(uSrcSize) - ivec2(1));
    return texelFetch(uSource, c, 0).rgb;
}

float luma(vec3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }

// Accumulates gradient direction/length for one of the 4 taps immediately
// surrounding the sample point, weighted by bilinear proximity `w`.
void easuSet(inout vec2 dir, inout float len, float w, float lA, float lB, float lC, float lD,
             float lE) {
    float dc = lD - lC;
    float cb = lC - lB;
    float lenX = max(abs(dc), abs(cb));
    lenX = 1.0 / max(lenX, 1e-8);
    float dirX = lD - lB;
    dir.x += dirX * w;
    lenX = clamp(abs(dirX) * lenX, 0.0, 1.0);
    lenX *= lenX;
    len += lenX * w;

    float ec = lE - lC;
    float ca = lC - lA;
    float lenY = max(abs(ec), abs(ca));
    lenY = 1.0 / max(lenY, 1e-8);
    float dirY = lE - lA;
    dir.y += dirY * w;
    lenY = clamp(abs(dirY) * lenY, 0.0, 1.0);
    lenY *= lenY;
    len += lenY * w;
}

// One of the 12 taps making up the final anisotropic-Lanczos accumulation.
void easuTap(inout vec3 aC, inout float aW, vec2 off, vec2 dir, vec2 len2, float lob, float clp,
             vec3 c) {
    vec2 v;
    v.x = off.x * dir.x + off.y * dir.y;
    v.y = off.x * (-dir.y) + off.y * dir.x;
    v *= len2;
    float d2 = min(v.x * v.x + v.y * v.y, clp);
    float wB = (2.0 / 5.0) * d2 - 1.0;
    float wA = lob * d2 - 1.0;
    wB *= wB;
    wA *= wA;
    wB = (25.0 / 16.0) * wB - (25.0 / 16.0 - 1.0);
    float w = wB * wA;
    aC += c * w;
    aW += w;
}

void main() {
    // Position of 'f' (see tap diagram below) in input-texel space, and
    // the fractional offset within that texel used as the bilinear weight.
    vec2 ip = floor(gl_FragCoord.xy);
    vec2 pp = ip * (uSrcSize / uDstSize) + (0.5 * uSrcSize / uDstSize - 0.5);
    vec2 fp = floor(pp);
    pp -= fp;
    ivec2 fpi = ivec2(fp);

    // 12-tap kernel:
    //    b c
    //  e f g h
    //  i j k l
    //    n o
    vec3 b = fetchTap(fpi, 0, -1);
    vec3 c = fetchTap(fpi, 1, -1);
    vec3 e = fetchTap(fpi, -1, 0);
    vec3 f = fetchTap(fpi, 0, 0);
    vec3 g = fetchTap(fpi, 1, 0);
    vec3 h = fetchTap(fpi, 2, 0);
    vec3 i = fetchTap(fpi, -1, 1);
    vec3 j = fetchTap(fpi, 0, 1);
    vec3 k = fetchTap(fpi, 1, 1);
    vec3 l = fetchTap(fpi, 2, 1);
    vec3 n = fetchTap(fpi, 0, 2);
    vec3 o = fetchTap(fpi, 1, 2);

    float bL = luma(b), cL = luma(c), eL = luma(e), fL = luma(f);
    float gL = luma(g), hL = luma(h), iL = luma(i), jL = luma(j);
    float kL = luma(k), lL = luma(l), nL = luma(n), oL = luma(o);

    vec2 dir = vec2(0.0);
    float len = 0.0;
    easuSet(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);
    easuSet(dir, len, pp.x * (1.0 - pp.y), cL, fL, gL, hL, kL);
    easuSet(dir, len, (1.0 - pp.x) * pp.y, fL, iL, jL, kL, nL);
    easuSet(dir, len, pp.x * pp.y, gL, jL, kL, lL, oL);

    vec2 dir2 = dir * dir;
    float dirR = dir2.x + dir2.y;
    bool zro = dirR < (1.0 / 32768.0);
    dirR = inversesqrt(dirR);
    dirR = zro ? 1.0 : dirR;
    dir.x = zro ? 1.0 : dir.x;
    dir *= dirR;

    len = len * 0.5;
    len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));
    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    float lob = 0.5 + (1.0 / 4.0 - 0.04 - 0.5) * len;
    float clp = 1.0 / lob;

    vec3 min4 = min(min(f, g), min(j, k));
    vec3 max4 = max(max(f, g), max(j, k));

    vec3 aC = vec3(0.0);
    float aW = 0.0;
    easuTap(aC, aW, vec2(0.0, -1.0) - pp, dir, len2, lob, clp, b);
    easuTap(aC, aW, vec2(1.0, -1.0) - pp, dir, len2, lob, clp, c);
    easuTap(aC, aW, vec2(-1.0, 1.0) - pp, dir, len2, lob, clp, i);
    easuTap(aC, aW, vec2(0.0, 1.0) - pp, dir, len2, lob, clp, j);
    easuTap(aC, aW, vec2(0.0, 0.0) - pp, dir, len2, lob, clp, f);
    easuTap(aC, aW, vec2(-1.0, 0.0) - pp, dir, len2, lob, clp, e);
    easuTap(aC, aW, vec2(1.0, 1.0) - pp, dir, len2, lob, clp, k);
    easuTap(aC, aW, vec2(2.0, 1.0) - pp, dir, len2, lob, clp, l);
    easuTap(aC, aW, vec2(2.0, 0.0) - pp, dir, len2, lob, clp, h);
    easuTap(aC, aW, vec2(1.0, 0.0) - pp, dir, len2, lob, clp, g);
    easuTap(aC, aW, vec2(1.0, 2.0) - pp, dir, len2, lob, clp, o);
    easuTap(aC, aW, vec2(0.0, 2.0) - pp, dir, len2, lob, clp, n);

    vec3 pix = clamp(aC / aW, min4, max4);
    fragColor = vec4(pix, 1.0);
}
