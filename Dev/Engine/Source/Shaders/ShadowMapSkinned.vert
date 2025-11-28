#version 450

// Shadow Map Vertex Shader for Skinned Meshes
// Renders skinned meshes from light's perspective to create depth map

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inBoneWeights;
layout(location = 4) in vec4 inBoneIndices;  // Bone indices as floats (convert to int)

// Light space matrix (from light's POV)
layout(std140, set = 1, binding = 0) uniform ShadowUniforms {
    mat4 lightSpaceMatrix;
} shadow;

// Model matrix for this skinned mesh
layout(std140, set = 1, binding = 1) uniform ModelUniforms {
    mat4 model;
} modelUbo;

// Bone matrices (256 bones max)
layout(std140, set = 1, binding = 2) uniform SkinUniforms {
    mat4 boneMatrices[256];
} skin;

void main() {
    // Get bone indices
    ivec4 boneIds = ivec4(inBoneIndices);
    
    // Calculate skinned position
    vec4 skinnedPos = vec4(0.0);
    
    // Apply bone transforms weighted by bone weights
    if (inBoneWeights.x > 0.0) {
        skinnedPos += inBoneWeights.x * (skin.boneMatrices[boneIds.x] * vec4(inPosition, 1.0));
    }
    if (inBoneWeights.y > 0.0) {
        skinnedPos += inBoneWeights.y * (skin.boneMatrices[boneIds.y] * vec4(inPosition, 1.0));
    }
    if (inBoneWeights.z > 0.0) {
        skinnedPos += inBoneWeights.z * (skin.boneMatrices[boneIds.z] * vec4(inPosition, 1.0));
    }
    if (inBoneWeights.w > 0.0) {
        skinnedPos += inBoneWeights.w * (skin.boneMatrices[boneIds.w] * vec4(inPosition, 1.0));
    }
    
    // Fallback if no weights (shouldn't happen for skinned mesh)
    float totalWeight = inBoneWeights.x + inBoneWeights.y + inBoneWeights.z + inBoneWeights.w;
    if (totalWeight < 0.001) {
        skinnedPos = vec4(inPosition, 1.0);
    }
    
    // Apply model matrix then light space matrix
    vec4 worldPos = modelUbo.model * skinnedPos;
    gl_Position = shadow.lightSpaceMatrix * worldPos;
}
