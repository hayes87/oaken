#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inWeights;
layout(location = 4) in vec4 inJoints; // COMPACT joint indices (0 to usedJointCount-1)

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out float outDebug;

// Scene uniforms (MVP)
layout(std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} scene;

// Skinning uniforms - indexed by COMPACT joint index
layout(std140, set = 1, binding = 1) uniform SkinUniforms {
    mat4 jointMatrices[256];
} skin;

void main() {
    // Convert float compact indices to int
    ivec4 jointIndices = clamp(ivec4(inJoints), 0, 255);
    
    // Build skin matrix from weighted joint matrices (using COMPACT indices)
    mat4 skinMatrix = inWeights.x * skin.jointMatrices[jointIndices.x] +
                      inWeights.y * skin.jointMatrices[jointIndices.y] +
                      inWeights.z * skin.jointMatrices[jointIndices.z] +
                      inWeights.w * skin.jointMatrices[jointIndices.w];
    
    // Apply skinning to position
    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    
    // Debug: output diagonal sum (should be ~4.0 for identity)
    outDebug = skinMatrix[0][0] + skinMatrix[1][1] + skinMatrix[2][2] + skinMatrix[3][3];
    
    gl_Position = scene.proj * scene.view * scene.model * skinnedPos;
    outUV = inUV;
    
    // Transform normal by skinMatrix
    mat3 normalMatrix = mat3(skinMatrix);
    vec3 transformedNormal = normalMatrix * inNormal;
    float len = length(transformedNormal);
    outNormal = (len > 0.0001) ? transformedNormal / len : vec3(0.0, 1.0, 0.0);
}
