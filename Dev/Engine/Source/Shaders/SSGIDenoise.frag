#version 450

// SSGI Spatial Denoising - Edge-aware bilateral blur
// Aggressive filtering for low-resolution SSGI with edge preservation

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

// Gaussian weight
float gaussianWeight(float x, float sigma) {
    return exp(-0.5 * x * x / (sigma * sigma));
}

// Linearize depth for better edge detection
float linearizeDepth(float depth, float near, float far) {
    return near * far / (far + depth * (near - far));
}

void main() {
    vec2 uv = inUV;
    vec2 texelSize = params.screenSize.zw;
    
    // Sample center values
    vec3 centerColor = texture(giTexture, uv).rgb;
    float centerDepth = texture(depthTexture, uv).r;
    
    // Skip sky pixels
    if (centerDepth >= 0.9999) {
        outColor = vec4(centerColor, 1.0);
        return;
    }
    
    float centerLinearDepth = linearizeDepth(centerDepth, 0.1, 100.0);
    
    // Use larger kernel for lower resolution (more aggressive blur)
    int radius = clamp(params.kernelRadius, 2, 6);
    float spatialSigma = float(radius) * 0.6;
    
    // Direction for separable filter
    vec2 direction = params.passIndex == 0 ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    
    // Accumulate weighted samples
    vec3 result = vec3(0.0);
    float totalWeight = 0.0;
    
    // Adaptive depth threshold based on distance
    float adaptiveDepthSigma = params.depthSigma * (1.0 + centerLinearDepth * 0.02);
    
    for (int i = -radius; i <= radius; i++) {
        vec2 offset = direction * float(i) * texelSize * 1.5;  // Slightly larger steps
        vec2 sampleUV = clamp(uv + offset, vec2(0.001), vec2(0.999));
        
        vec3 sampleColor = texture(giTexture, sampleUV).rgb;
        float sampleDepth = texture(depthTexture, sampleUV).r;
        
        // Skip sky
        if (sampleDepth >= 0.9999) continue;
        
        float sampleLinearDepth = linearizeDepth(sampleDepth, 0.1, 100.0);
        
        // Spatial weight (Gaussian falloff with distance)
        float spatialWeight = gaussianWeight(float(i), spatialSigma);
        
        // Depth weight - preserve edges at depth discontinuities
        float depthDiff = abs(centerLinearDepth - sampleLinearDepth);
        float relativeDepthDiff = depthDiff / max(centerLinearDepth, 0.1);
        float depthWeight = exp(-relativeDepthDiff * relativeDepthDiff * 50.0 / (adaptiveDepthSigma * adaptiveDepthSigma));
        
        // Luminance-based color weight to preserve bright highlights
        float centerLum = dot(centerColor, vec3(0.299, 0.587, 0.114));
        float sampleLum = dot(sampleColor, vec3(0.299, 0.587, 0.114));
        float lumDiff = abs(centerLum - sampleLum);
        float colorWeight = gaussianWeight(lumDiff, params.colorSigma * (1.0 + centerLum));
        
        // Final weight
        float weight = spatialWeight * depthWeight * colorWeight;
        weight = max(weight, 0.0);
        
        result += sampleColor * weight;
        totalWeight += weight;
    }
    
    // Normalize result
    if (totalWeight > 0.0001) {
        result /= totalWeight;
    } else {
        result = centerColor;
    }
    
    // Subtle sharpening pass to recover some detail lost in blur
    if (params.passIndex == 1) {
        vec3 sharpened = result * 1.1 - centerColor * 0.1;
        result = max(sharpened, vec3(0.0));
    }
    
    outColor = vec4(result, 1.0);
}
