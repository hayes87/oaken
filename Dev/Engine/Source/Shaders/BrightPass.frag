#version 450

// Bright Pass Shader - Extracts bright pixels for bloom
// Only pixels above the threshold contribute to bloom

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// SDL_GPU fragment sampler binding: set = 2, binding = 0
layout(set = 2, binding = 0) uniform sampler2D hdrTexture;

// SDL_GPU fragment uniform binding: set = 3, binding = 0
layout(std140, set = 3, binding = 0) uniform BrightPassParams {
    float threshold;      // Brightness threshold (default: 1.0)
    float softThreshold;  // Soft knee threshold (0 = hard, 1 = soft)
    float _padding1;
    float _padding2;
} params;

void main() {
    vec3 color = texture(hdrTexture, inUV).rgb;
    
    // Calculate luminance using perceptual weights
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // Soft threshold with knee
    float knee = params.threshold * params.softThreshold;
    float soft = brightness - params.threshold + knee;
    soft = clamp(soft / (2.0 * knee + 0.0001), 0.0, 1.0);
    soft = soft * soft;
    
    // Contribution factor - blend between hard and soft threshold
    float contribution = max(soft, step(params.threshold, brightness));
    contribution *= max(0.0, brightness - params.threshold);
    
    // Extract bloom color
    vec3 bloomColor = color * contribution;
    
    outColor = vec4(bloomColor, 1.0);
}
