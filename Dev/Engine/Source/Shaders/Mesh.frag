#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main() {
    // Debug: Color by normal
    outColor = vec4(normalize(inNormal) * 0.5 + 0.5, 1.0);
}
