#version 450

// Screen-Space Global Illumination
// Uses raymarching against the depth buffer to compute indirect lighting

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Samplers
layout(set = 2, binding = 0) uniform sampler2D colorTexture;    // Previous frame / lit scene
layout(set = 2, binding = 1) uniform sampler2D depthTexture;    // Depth buffer
layout(set = 2, binding = 2) uniform sampler2D normalTexture;   // World normals (if available)
layout(set = 2, binding = 3) uniform sampler2D noiseTexture;    // Blue noise for sampling

// Uniforms
layout(std140, set = 3, binding = 0) uniform SSGIParams {
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 invViewMatrix;
    mat4 invProjMatrix;
    mat4 prevViewProjMatrix;    // For temporal reprojection
    vec4 cameraPos;             // xyz = position, w = unused
    vec4 screenSize;            // xy = size, zw = 1/size
    float nearPlane;
    float farPlane;
    float intensity;            // GI intensity multiplier
    float maxDistance;          // Maximum ray distance in world units
    int numRays;                // Number of rays per pixel (1-8)
    int numSteps;               // Ray march steps (8-32)
    float thickness;            // Depth comparison thickness
    float frameIndex;           // For temporal jitter
} params;

// Constants
#define PI 3.14159265359
#define MAX_RAYS 8
#define MAX_STEPS 32

// Blue noise pattern for ray jittering
vec2 getBlueNoise(ivec2 pixel) {
    ivec2 noiseSize = textureSize(noiseTexture, 0);
    // Use static noise pattern (no temporal animation) for stability
    ivec2 noiseSample = pixel % noiseSize;
    return texelFetch(noiseTexture, noiseSample, 0).rg;
}

// Reconstruct world position from depth
vec3 getWorldPosition(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    clipPos.y = -clipPos.y;  // Vulkan flip
    vec4 viewPos = params.invProjMatrix * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = params.invViewMatrix * viewPos;
    return worldPos.xyz;
}

// Linearize depth
float linearizeDepth(float depth) {
    return params.nearPlane * params.farPlane / (params.farPlane + depth * (params.nearPlane - params.farPlane));
}

// Project world position to screen UV
vec3 projectToScreen(vec3 worldPos) {
    vec4 clipPos = params.projMatrix * params.viewMatrix * vec4(worldPos, 1.0);
    clipPos.xyz /= clipPos.w;
    clipPos.y = -clipPos.y;  // Vulkan flip
    return vec3(clipPos.xy * 0.5 + 0.5, clipPos.z);
}

// Cosine-weighted hemisphere sampling
vec3 cosineSampleHemisphere(vec2 u, vec3 normal) {
    float phi = 2.0 * PI * u.x;
    float cosTheta = sqrt(1.0 - u.y);
    float sinTheta = sqrt(u.y);
    
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    
    // Create TBN from normal
    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    
    return normalize(tangent * H.x + bitangent * H.y + normal * H.z);
}

// Reconstruct normal from depth (if no G-buffer normal)
vec3 reconstructNormal(vec2 uv, vec3 worldPos) {
    vec2 texelSize = params.screenSize.zw;
    
    float depthL = texture(depthTexture, uv - vec2(texelSize.x, 0.0)).r;
    float depthR = texture(depthTexture, uv + vec2(texelSize.x, 0.0)).r;
    float depthD = texture(depthTexture, uv - vec2(0.0, texelSize.y)).r;
    float depthU = texture(depthTexture, uv + vec2(0.0, texelSize.y)).r;
    
    vec3 posL = getWorldPosition(uv - vec2(texelSize.x, 0.0), depthL);
    vec3 posR = getWorldPosition(uv + vec2(texelSize.x, 0.0), depthR);
    vec3 posD = getWorldPosition(uv - vec2(0.0, texelSize.y), depthD);
    vec3 posU = getWorldPosition(uv + vec2(0.0, texelSize.y), depthU);
    
    vec3 dx = posR - posL;
    vec3 dy = posU - posD;
    
    return normalize(cross(dy, dx));
}

// Screen-space ray march
bool traceRay(vec3 origin, vec3 direction, out vec2 hitUV, out float hitDepth) {
    // Project ray to screen space for efficient marching
    vec3 startScreen = projectToScreen(origin);
    vec3 endWorld = origin + direction * params.maxDistance;
    vec3 endScreen = projectToScreen(endWorld);
    
    vec3 rayScreen = endScreen - startScreen;
    
    // Clip to screen bounds
    vec2 screenMin = vec2(0.001);
    vec2 screenMax = vec2(0.999);
    
    float tMin = 0.0;
    float tMax = 1.0;
    
    // X clipping
    if (abs(rayScreen.x) > 0.0001) {
        float t1 = (screenMin.x - startScreen.x) / rayScreen.x;
        float t2 = (screenMax.x - startScreen.x) / rayScreen.x;
        tMin = max(tMin, min(t1, t2));
        tMax = min(tMax, max(t1, t2));
    }
    
    // Y clipping
    if (abs(rayScreen.y) > 0.0001) {
        float t1 = (screenMin.y - startScreen.y) / rayScreen.y;
        float t2 = (screenMax.y - startScreen.y) / rayScreen.y;
        tMin = max(tMin, min(t1, t2));
        tMax = min(tMax, max(t1, t2));
    }
    
    if (tMin >= tMax) return false;
    
    // March along ray
    int numSteps = min(params.numSteps, MAX_STEPS);
    float stepSize = (tMax - tMin) / float(numSteps);
    
    float t = tMin;
    vec3 prevPos = startScreen;
    
    for (int i = 0; i < numSteps; i++) {
        t += stepSize;
        vec3 currPos = startScreen + rayScreen * t;
        
        // Check screen bounds
        if (currPos.x < 0.0 || currPos.x > 1.0 || currPos.y < 0.0 || currPos.y > 1.0) {
            break;
        }
        
        // Sample depth at current position
        float sceneDepth = texture(depthTexture, currPos.xy).r;
        
        // Check for intersection
        float depthDiff = currPos.z - sceneDepth;
        
        if (depthDiff > 0.0 && depthDiff < params.thickness) {
            // Binary refinement for more accurate hit
            float lo = t - stepSize;
            float hi = t;
            
            for (int j = 0; j < 4; j++) {
                float mid = (lo + hi) * 0.5;
                vec3 midPos = startScreen + rayScreen * mid;
                float midDepth = texture(depthTexture, midPos.xy).r;
                float midDiff = midPos.z - midDepth;
                
                if (midDiff > 0.0) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            
            vec3 hitPos = startScreen + rayScreen * ((lo + hi) * 0.5);
            hitUV = hitPos.xy;
            hitDepth = hitPos.z;
            return true;
        }
        
        prevPos = currPos;
    }
    
    return false;
}

void main() {
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    vec2 uv = inUV;
    
    // Sample depth at current pixel
    float depth = texture(depthTexture, uv).r;
    
    // Skip sky/background
    if (depth >= 1.0) {
        outColor = vec4(0.0);
        return;
    }
    
    // Reconstruct world position and normal
    vec3 worldPos = getWorldPosition(uv, depth);
    vec3 normal = reconstructNormal(uv, worldPos);
    
    // Get blue noise for ray jittering
    vec2 noise = getBlueNoise(pixelCoord);
    
    // Sample the average scene color for fallback ambient
    // This provides stability when rays miss
    vec3 avgSceneColor = vec3(0.0);
    avgSceneColor += texture(colorTexture, vec2(0.25, 0.25)).rgb;
    avgSceneColor += texture(colorTexture, vec2(0.75, 0.25)).rgb;
    avgSceneColor += texture(colorTexture, vec2(0.25, 0.75)).rgb;
    avgSceneColor += texture(colorTexture, vec2(0.75, 0.75)).rgb;
    avgSceneColor += texture(colorTexture, vec2(0.5, 0.5)).rgb;
    avgSceneColor /= 5.0;
    avgSceneColor *= 0.15; // Subtle ambient contribution
    
    // Accumulate indirect lighting
    vec3 indirectLight = vec3(0.0);
    float hitCount = 0.0;
    
    int numRays = min(params.numRays, MAX_RAYS);
    
    for (int i = 0; i < numRays; i++) {
        // Generate sample direction using cosine-weighted hemisphere
        float angle = (float(i) + noise.x) / float(numRays) * 2.0 * PI;
        vec2 sampleNoise = vec2(
            fract(noise.x + float(i) * 0.618033988749),  // Golden ratio
            fract(noise.y + float(i) * 0.381966011250)
        );
        
        vec3 rayDir = cosineSampleHemisphere(sampleNoise, normal);
        
        // Offset origin slightly to avoid self-intersection
        vec3 rayOrigin = worldPos + normal * 0.05;
        
        vec2 hitUV;
        float hitDepth;
        
        if (traceRay(rayOrigin, rayDir, hitUV, hitDepth)) {
            // Sample color at hit point
            vec3 hitColor = texture(colorTexture, hitUV).rgb;
            
            // Distance falloff
            vec3 hitWorldPos = getWorldPosition(hitUV, hitDepth);
            float dist = length(hitWorldPos - worldPos);
            float falloff = 1.0 / (1.0 + dist * dist * 0.1);
            
            // Clamp hit color to prevent fireflies
            hitColor = min(hitColor, vec3(4.0));
            
            indirectLight += hitColor * falloff;
            hitCount += 1.0;
        }
    }
    
    // Calculate hit ratio for blending with fallback
    float hitRatio = hitCount / float(numRays);
    
    // Normalize hit contribution
    if (hitCount > 0.5) {
        indirectLight /= hitCount;
    }
    
    // Blend between ray-traced GI and fallback ambient based on hit ratio
    // This prevents abrupt changes when rays miss
    vec3 finalGI = mix(avgSceneColor, indirectLight, hitRatio);
    
    // Apply intensity
    finalGI *= params.intensity;
    
    // Output GI contribution
    outColor = vec4(finalGI, 1.0);
}
