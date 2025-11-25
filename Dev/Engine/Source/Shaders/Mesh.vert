#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inWeights;
layout(location = 4) in uvec4 inJoints;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;

layout(std140, set = 1, binding = 0) uniform UniformBlock {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 jointMatrices[256];
} ubo;

void main() {
    mat4 skinMatrix = mat4(1.0);
    
    // Check if skinned (weights sum > 0)
    if (dot(inWeights, vec4(1.0)) > 0.0) {
        skinMatrix = 
            inWeights.x * ubo.jointMatrices[inJoints.x] +
            inWeights.y * ubo.jointMatrices[inJoints.y] +
            inWeights.z * ubo.jointMatrices[inJoints.z] +
            inWeights.w * ubo.jointMatrices[inJoints.w];
    }

    gl_Position = ubo.proj * ubo.view * ubo.model * skinMatrix * vec4(inPosition, 1.0);
    outUV = inUV;
    // Normalize normal to handle scaling
    outNormal = normalize(mat3(transpose(inverse(ubo.model * skinMatrix))) * inNormal);
}
