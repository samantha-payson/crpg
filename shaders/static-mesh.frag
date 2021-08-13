#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
  vec3 lightDir = normalize(vec3(1.0, -0.5, -1.0));
  outColor = vec4(vec3(0.9)*max(0, dot(lightDir, normalize(fragNormal))) + vec3(0.01), 1);
}
