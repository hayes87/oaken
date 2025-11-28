#version 450

// Bloom Composite Shader
// Combines bloom texture with original HDR scene (before tone mapping)

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// SDL_GPU fragment sampler bindings: set = 2
layout(set = 2, binding = 0) uniform sampler2D sceneTexture;  // Original HDR scene
layout(set = 2, binding = 1) uniform sampler2D bloomTexture;  // Blurred bloom

// SDL_GPU fragment uniform binding: set = 3, binding = 0
layout(std140, set = 3, binding = 0) uniform BloomParams {
    float intensity;    // Bloom intensity (default: 0.5)
    float _padding1;
    float _padding2;
    float _padding3;
} params;

void main() {
    vec3 sceneColor = texture(sceneTexture, inUV).rgb;
    vec3 bloomColor = texture(bloomTexture, inUV).rgb;
    
    // Additive blend
    vec3 result = sceneColor + bloomColor * params.intensity;
    
    outColor = vec4(result, 1.0);
}
