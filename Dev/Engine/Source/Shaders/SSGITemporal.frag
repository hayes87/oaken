#version 450

// SSGI Temporal Accumulation & Denoising
// Combines current frame GI with previous frames for stability

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Samplers
layout(set = 2, binding = 0) uniform sampler2D currentGI;       // Current frame SSGI
layout(set = 2, binding = 1) uniform sampler2D historyGI;       // Previous frame accumulated GI
layout(set = 2, binding = 2) uniform sampler2D depthTexture;    // Current depth
layout(set = 2, binding = 3) uniform sampler2D velocityTexture; // Motion vectors (if available)

// Uniforms
layout(std140, set = 3, binding = 0) uniform TemporalParams {
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 invViewMatrix;
    mat4 invProjMatrix;
    mat4 prevViewProjMatrix;
    vec4 screenSize;            // xy = size, zw = 1/size
    float temporalBlend;        // Blend factor (0.9 = 90% history)
    float depthThreshold;       // Depth rejection threshold
    float normalThreshold;      // Normal rejection threshold
    int useVelocity;            // Whether velocity buffer is available
} params;

// Reconstruct world position from depth
vec3 getWorldPosition(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    clipPos.y = -clipPos.y;
    vec4 viewPos = params.invProjMatrix * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = params.invViewMatrix * viewPos;
    return worldPos.xyz;
}

// Reproject to previous frame UV
vec2 reproject(vec3 worldPos) {
    vec4 prevClip = params.prevViewProjMatrix * vec4(worldPos, 1.0);
    prevClip.xyz /= prevClip.w;
    prevClip.y = -prevClip.y;
    return prevClip.xy * 0.5 + 0.5;
}

// AABB clipping for color clamping (reduces ghosting)
vec3 clipAABB(vec3 color, vec3 minColor, vec3 maxColor) {
    vec3 center = (minColor + maxColor) * 0.5;
    vec3 extent = (maxColor - minColor) * 0.5 + 0.001;
    
    vec3 offset = color - center;
    vec3 unit = offset / extent;
    float maxUnit = max(abs(unit.x), max(abs(unit.y), abs(unit.z)));
    
    if (maxUnit > 1.0) {
        return center + offset / maxUnit;
    }
    return color;
}

// Compute neighborhood color bounds for variance clipping
void computeNeighborhoodBounds(vec2 uv, out vec3 minColor, out vec3 maxColor, out vec3 avgColor) {
    vec2 texelSize = params.screenSize.zw;
    
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    
    // 3x3 neighborhood
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec3 c = texture(currentGI, uv + vec2(x, y) * texelSize).rgb;
            m1 += c;
            m2 += c * c;
        }
    }
    
    m1 /= 9.0;
    m2 /= 9.0;
    
    vec3 variance = sqrt(max(m2 - m1 * m1, vec3(0.0)));
    
    // Variance clipping with gamma = 1.0 for sharper result
    float gamma = 1.0;
    minColor = m1 - variance * gamma;
    maxColor = m1 + variance * gamma;
    avgColor = m1;
}

void main() {
    vec2 uv = inUV;
    
    // Sample current frame GI
    vec3 currentColor = texture(currentGI, uv).rgb;
    
    // Sample depth
    float depth = texture(depthTexture, uv).r;
    
    // Skip sky
    if (depth >= 1.0) {
        outColor = vec4(currentColor, 1.0);
        return;
    }
    
    // Get world position
    vec3 worldPos = getWorldPosition(uv, depth);
    
    // Reproject to find history UV
    vec2 historyUV;
    if (params.useVelocity > 0) {
        vec2 velocity = texture(velocityTexture, uv).rg;
        historyUV = uv - velocity;
    } else {
        historyUV = reproject(worldPos);
    }
    
    // Check if reprojected UV is valid
    bool validHistory = historyUV.x >= 0.0 && historyUV.x <= 1.0 &&
                        historyUV.y >= 0.0 && historyUV.y <= 1.0;
    
    if (!validHistory) {
        outColor = vec4(currentColor, 1.0);
        return;
    }
    
    // Sample history
    vec3 historyColor = texture(historyGI, historyUV).rgb;
    
    // Compute neighborhood bounds for variance clipping
    vec3 minColor, maxColor, avgColor;
    computeNeighborhoodBounds(uv, minColor, maxColor, avgColor);
    
    // Clip history to neighborhood AABB to reduce ghosting
    historyColor = clipAABB(historyColor, minColor, maxColor);
    
    // Blend current and history
    float blend = params.temporalBlend;
    
    // Reduce blend weight if history seems invalid
    float colorDiff = length(historyColor - avgColor) / max(length(avgColor), 0.001);
    if (colorDiff > 1.0) {
        blend *= 0.5;  // Trust history less when it differs significantly
    }
    
    vec3 result = mix(currentColor, historyColor, blend);
    
    outColor = vec4(result, 1.0);
}
