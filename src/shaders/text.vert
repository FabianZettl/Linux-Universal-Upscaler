#version 450 core

// Unit (0,0)-(1,1) quad from gl_VertexID, no vertex buffer - same
// no-VBO trick fullscreen.vert uses, just 6 vertices (2 triangles)
// instead of an oversized single triangle, since a text glyph needs
// actual quad bounds rather than full-screen coverage.

uniform vec2 uScreenSize;  // pixels
uniform vec4 uRectPx;      // x, y, width, height - top-left origin, y-down
uniform vec4 uUVRect;      // u0, v0, u1, v1 - this glyph's atlas sub-rect

layout(location = 0) out vec2 vUV;

void main() {
    vec2 quad[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
        vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
    );
    vec2 p = quad[gl_VertexID];

    vec2 pixelPos = uRectPx.xy + p * uRectPx.zw;
    vec2 ndc = vec2((pixelPos.x / uScreenSize.x) * 2.0 - 1.0,
                     1.0 - (pixelPos.y / uScreenSize.y) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);

    vUV = mix(uUVRect.xy, uUVRect.zw, p);
}
