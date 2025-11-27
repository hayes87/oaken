#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inWeights;  // Unused for static meshes
layout(location = 4) in vec4 inJoints;   // Unused for static meshes

// Instance data (per-instance vertex buffer)
layout(location = 5) in mat4 inModel;    // Takes locations 5, 6, 7, 8
layout(location = 9) in vec4 inColor;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out vec4 outColor;

// View/Proj uniforms only (model comes from instance data)
layout(std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 proj;
} scene;

void main() {
    // World space position
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    
    gl_Position = scene.proj * scene.view * worldPos;
    outUV = inUV;
    outColor = inColor;
    
    // Transform normal to world space
    mat3 modelNormalMatrix = transpose(inverse(mat3(inModel)));
    outWorldNormal = normalize(modelNormalMatrix * inNormal);
}
