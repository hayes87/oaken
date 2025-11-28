#version 450

// Separable Gaussian Blur Shader for Bloom
// Uses a 9-tap blur kernel with configurable direction

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// SDL_GPU fragment sampler binding: set = 2, binding = 0
layout(set = 2, binding = 0) uniform sampler2D inputTexture;

// SDL_GPU fragment uniform binding: set = 3, binding = 0
layout(std140, set = 3, binding = 0) uniform BlurParams {
    vec2 direction;    // (1,0) for horizontal, (0,1) for vertical
    vec2 texelSize;    // 1.0 / textureSize
} params;

// 9-tap Gaussian kernel (sigma ~= 3.0)
const float weights[5] = float[](
    0.227027,   // center
    0.1945946,  // offset 1
    0.1216216,  // offset 2
    0.054054,   // offset 3
    0.016216    // offset 4
);

void main() {
    vec2 offset = params.direction * params.texelSize;
    
    // Sample center
    vec3 result = texture(inputTexture, inUV).rgb * weights[0];
    
    // Sample positive and negative offsets
    for (int i = 1; i < 5; i++) {
        vec2 sampleOffset = offset * float(i);
        result += texture(inputTexture, inUV + sampleOffset).rgb * weights[i];
        result += texture(inputTexture, inUV - sampleOffset).rgb * weights[i];
    }
    
    outColor = vec4(result, 1.0);
}
