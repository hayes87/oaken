#version 450

// Vertex attributes - must match MeshInstanced.vert layout
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inWeights;  // Unused for static meshes
layout(location = 4) in vec4 inJoints;   // Unused for static meshes

// Instance data (per-instance vertex buffer)
layout(location = 5) in mat4 inModel;    // Takes locations 5, 6, 7, 8
layout(location = 9) in vec4 inColor;    // Instance color (unused in depth pass)

// View-Projection uniform - SDL_GPU SPIR-V: vertex uniforms at set 1
layout(std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 proj;
} scene;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    gl_Position = scene.proj * scene.view * worldPos;
}
