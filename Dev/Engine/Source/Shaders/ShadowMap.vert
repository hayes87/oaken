#version 450

// Shadow Map Vertex Shader
// Renders scene from light's perspective to create depth map

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inBoneWeights;
layout(location = 4) in vec4 inBoneIndices;  // COMPACT joint indices (matches Mesh.vert)

// Per-instance data - must match MeshInstanced.vert layout exactly
layout(location = 5) in mat4 inModel;    // Takes locations 5, 6, 7, 8
layout(location = 9) in vec4 inInstanceColor;

// Light space matrix (from light's POV)
layout(std140, set = 1, binding = 0) uniform ShadowUniforms {
    mat4 lightSpaceMatrix;
} shadow;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    gl_Position = shadow.lightSpaceMatrix * worldPos;
}
