#version 450

const vec3 positions[3] = vec3[3](
    vec3(+0.37f, +0.5f, +0.f),
    vec3(-0.37f, +0.5f, +0.f),
    vec3(+0.0f, -0.5f, +0.f));

const vec3 colors[3] = vec3[3](
    vec3(+1.f, +0.f, +0.f),
    vec3(+0.f, +1.f, +0.f),
    vec3(+0.f, +0.f, +1.f));

layout (location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex]*1.4, 1.0f);
    fragColor = colors[gl_VertexIndex];
}
