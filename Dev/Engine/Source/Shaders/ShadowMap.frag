#version 450

// Shadow Map Fragment Shader
// For directional lights, we just need depth which is written automatically
// This shader is minimal - the depth buffer does the work

void main() {
    // Depth is written automatically by the rasterizer
    // No color output needed for shadow mapping
}
