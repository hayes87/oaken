#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// SDL_GPU fragment sampler binding: set = 2, binding = 0
layout(set = 2, binding = 0) uniform sampler2D hdrTexture;

// SDL_GPU fragment uniform binding: set = 3, binding = 0
layout(std140, set = 3, binding = 0) uniform ToneMappingParams {
    float exposure;
    float gamma;
    int tonemapOperator; // 0 = Reinhard, 1 = ACES, 2 = Uncharted2
    float _padding;
} params;

// Reinhard tone mapping
vec3 tonemapReinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

// ACES Filmic tone mapping (approximation)
vec3 tonemapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Uncharted 2 tone mapping
vec3 uncharted2Tonemap(vec3 x) {
    const float A = 0.15; // Shoulder Strength
    const float B = 0.50; // Linear Strength
    const float C = 0.10; // Linear Angle
    const float D = 0.20; // Toe Strength
    const float E = 0.02; // Toe Numerator
    const float F = 0.30; // Toe Denominator
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemapUncharted2(vec3 color) {
    const float W = 11.2; // Linear White Point Value
    vec3 curr = uncharted2Tonemap(color);
    vec3 whiteScale = vec3(1.0) / uncharted2Tonemap(vec3(W));
    return curr * whiteScale;
}

void main() {
    vec3 hdrColor = texture(hdrTexture, inUV).rgb;
    
    // Apply exposure
    hdrColor *= params.exposure;
    
    // Apply tone mapping
    vec3 mapped;
    if (params.tonemapOperator == 0) {
        mapped = tonemapReinhard(hdrColor);
    } else if (params.tonemapOperator == 1) {
        mapped = tonemapACES(hdrColor);
    } else {
        mapped = tonemapUncharted2(hdrColor);
    }
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / params.gamma));
    
    outColor = vec4(mapped, 1.0);
}
