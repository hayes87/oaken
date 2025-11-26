#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inDebug;

layout(location = 0) out vec4 outColor;

void main() {
    // Normal-based coloring (proper shading)
    vec3 normalColor = normalize(inNormal) * 0.5 + 0.5;
    outColor = vec4(normalColor, 1.0);
}
