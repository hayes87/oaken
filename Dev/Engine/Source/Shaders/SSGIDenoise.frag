#version 450

// SSGI Spatial Denoising - Edge-aware blur
// Uses bilateral filtering to smooth while preserving edges

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Samplers
layout(set = 2, binding = 0) uniform sampler2D giTexture;       // GI to denoise
layout(set = 2, binding = 1) uniform sampler2D depthTexture;    // Depth for edge detection
layout(set = 2, binding = 2) uniform sampler2D normalTexture;   // Normals for edge detection (if available)

// Uniforms
layout(std140, set = 3, binding = 0) uniform DenoiseParams {
    vec4 screenSize;            // xy = size, zw = 1/size
    float depthSigma;           // Depth weight falloff
    float normalSigma;          // Normal weight falloff
    float colorSigma;           // Color weight falloff
    int kernelRadius;           // Filter radius (1-4)
    int passIndex;              // 0 = horizontal, 1 = vertical (separable)
} params;

// 1D Gaussian weights for separable filter
float gaussianWeight(float x, float sigma) {
    return exp(-0.5 * x * x / (sigma * sigma));
}

// Linearize depth
float linearizeDepth(float depth, float near, float far) {
    return near * far / (far + depth * (near - far));
}

void main() {
    vec2 uv = inUV;
    vec2 texelSize = params.screenSize.zw;
    
    // Sample center values
    vec3 centerColor = texture(giTexture, uv).rgb;
    float centerDepth = texture(depthTexture, uv).r;
    
    // Skip sky
    if (centerDepth >= 1.0) {
        outColor = vec4(centerColor, 1.0);
        return;
    }
    
    float centerLinearDepth = linearizeDepth(centerDepth, 0.1, 100.0);
    
    // Direction based on pass (separable bilateral filter)
    vec2 direction = params.passIndex == 0 ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    
    // Accumulate filtered result
    vec3 result = vec3(0.0);
    float totalWeight = 0.0;
    
    int radius = min(params.kernelRadius, 4);
    float spatialSigma = float(radius) * 0.5;
    
    for (int i = -radius; i <= radius; i++) {
        vec2 offset = direction * float(i) * texelSize;
        vec2 sampleUV = uv + offset;
        
        // Sample
        vec3 sampleColor = texture(giTexture, sampleUV).rgb;
        float sampleDepth = texture(depthTexture, sampleUV).r;
        
        // Skip invalid samples
        if (sampleDepth >= 1.0) continue;
        
        float sampleLinearDepth = linearizeDepth(sampleDepth, 0.1, 100.0);
        
        // Spatial weight (Gaussian)
        float spatialWeight = gaussianWeight(float(i), spatialSigma);
        
        // Depth weight (edge-preserving)
        float depthDiff = abs(centerLinearDepth - sampleLinearDepth);
        float depthWeight = gaussianWeight(depthDiff, params.depthSigma);
        
        // Color weight (optional, helps preserve detail)
        float colorDiff = length(centerColor - sampleColor);
        float colorWeight = gaussianWeight(colorDiff, params.colorSigma);
        
        // Combined weight
        float weight = spatialWeight * depthWeight * colorWeight;
        
        result += sampleColor * weight;
        totalWeight += weight;
    }
    
    // Normalize
    if (totalWeight > 0.001) {
        result /= totalWeight;
    } else {
        result = centerColor;
    }
    
    outColor = vec4(result, 1.0);
}
