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
    // Convert float indices to int (clamped to valid range)
    ivec4 boneIds = clamp(ivec4(inBoneIndices), 0, 255);
    
    // Build skin matrix from weighted bone matrices (same algorithm as Mesh.vert)
    mat4 skinMatrix = inBoneWeights.x * skin.boneMatrices[boneIds.x] +
                      inBoneWeights.y * skin.boneMatrices[boneIds.y] +
                      inBoneWeights.z * skin.boneMatrices[boneIds.z] +
                      inBoneWeights.w * skin.boneMatrices[boneIds.w];
    
    // Apply skinning to position
    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    
    // Apply model matrix then light space matrix
    vec4 worldPos = modelUbo.model * skinnedPos;
    gl_Position = shadow.lightSpaceMatrix * worldPos;
}
