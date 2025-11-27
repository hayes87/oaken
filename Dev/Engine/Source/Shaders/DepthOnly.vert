#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Instance attributes (for instanced rendering)
layout(location = 3) in mat4 inModel;  // Takes locations 3, 4, 5, 6
layout(location = 7) in vec4 inColor;  // Instance color (unused in depth pass)

// View-Projection uniform
layout(std140, set = 2, binding = 0) uniform ViewProjection {
    mat4 view;
    mat4 proj;
} vp;

void main() {
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    gl_Position = vp.proj * vp.view * worldPos;
}
