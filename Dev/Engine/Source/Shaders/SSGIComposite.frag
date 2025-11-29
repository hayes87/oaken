#version 450

// SSGI Composite - Adds GI contribution to scene
// Uses additive blending, so scene color is already in framebuffer

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Samplers
layout(set = 2, binding = 0) uniform sampler2D giTexture;       // Denoised SSGI
layout(set = 2, binding = 1) uniform sampler2D giTexture2;      // Padding (unused)

// Uniforms
layout(std140, set = 3, binding = 0) uniform CompositeParams {
    float giIntensity;          // GI strength multiplier
    float aoStrength;           // How much to darken occluded areas
    int debugMode;              // 0=composite, 1=GI only, 2=scene only
    int _pad0;
} params;

void main() {
    vec2 uv = inUV;
    
    vec3 giRGB = texture(giTexture, uv).rgb;
    
    // Debug modes
    if (params.debugMode == 1) {
        // GI only - output GI directly
        outColor = vec4(giRGB * params.giIntensity, 1.0);
        return;
    } else if (params.debugMode == 2) {
        // Scene only - discard (keep framebuffer)
        discard;
    }
    
    // Normal mode: Output just the GI contribution
    // The scene is already in the framebuffer, we add GI via additive blending
    outColor = vec4(giRGB * params.giIntensity, 1.0);
}
