#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 fragNormal;
layout (location = 1) out vec2 fragUV;

layout (push_constant) uniform constants {
  mat4 renderMatrix;
} PushConstants;

void main() {
  mat3 normalMatrix = transpose(inverse(mat3(PushConstants.renderMatrix)));
  gl_Position = PushConstants.renderMatrix * vec4(inPosition, 1.0);
  fragNormal  = normalMatrix * inNormal;
  fragUV      = inUV;
}
