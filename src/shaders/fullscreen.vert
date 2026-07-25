#version 450 core

// Draws a single triangle covering the whole viewport without any vertex
// buffer (glDrawArrays(GL_TRIANGLES, 0, 3)) - the standard full-screen-quad
// trick derived purely from gl_VertexID.
layout(location = 0) out vec2 vUV;

void main() {
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUV = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
