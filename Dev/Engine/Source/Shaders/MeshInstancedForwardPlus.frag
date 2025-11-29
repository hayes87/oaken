#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec4 outColor;

// Forward+ constants
#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256
#define MAX_POINT_LIGHTS 1024

// SDL_GPU Fragment Shader Layout (SPIR-V):
// Set 2: Sampled textures, read-only storage textures, read-only storage buffers
// Set 3: Uniform buffers

// Point light structure (matches LightCulling.comp)
struct PointLight {
    vec4 positionRadius;  // xyz = position, w = radius
    vec4 colorIntensity;  // rgb = color, w = intensity
};

// SDL_GPU binding order: Samplers first, then storage buffers
// Shadow map sampler (comparison sampler for PCF)
layout(set = 2, binding = 0) uniform sampler2DShadow shadowMap;

// Light buffer (read-only storage buffer at set 2, binding 1)
layout(std430, set = 2, binding = 1) readonly buffer LightBuffer {
    int numLights;
    int _pad0;
    int _pad1;
    int _pad2;
    PointLight lights[];
} lightBuffer;

// Tile light indices (read-only storage buffer at set 2, binding 2)
layout(std430, set = 2, binding = 2) readonly buffer TileLightIndices {
    uint data[];
} tileLightIndices;

// Light uniforms - for directional light, ambient, camera, screen info, and shadows
layout(std140, set = 3, binding = 0) uniform LightUniforms {
    // Directional light
    vec4 dirLightDir;       // xyz = direction, w = intensity
    vec4 dirLightColor;     // rgb = color, a = unused
    vec4 ambientColor;      // rgb = ambient, a = unused
    
    // Camera position for specular
    vec4 cameraPos;         // xyz = position, w = unused
    
    // Screen info for Forward+
    vec4 screenSize;        // xy = size, zw = 1/size
    
    // Shadow settings
    mat4 lightSpaceMatrix;  // Light's view-projection matrix
    float shadowBias;       // Depth bias
    float shadowNormalBias; // Normal-based bias
    int pcfSamples;         // PCF kernel size (0=hard, 1=3x3, 2=5x5)
    int shadowsEnabled;     // Shadow toggle
    
    float shininess;
    float _pad1;
    float _pad2;
    float _pad3;
} uniforms;

// Material properties (could be per-instance later)
const vec3 materialDiffuse = vec3(0.8);
const vec3 materialSpecular = vec3(0.5);

// Calculate shadow factor using PCF (Percentage Closer Filtering)
float calculateShadow(vec3 fragPosWorld, vec3 normal) {
    if (uniforms.shadowsEnabled == 0) return 1.0;
    
    // Get light direction
    vec3 lightDir = normalize(-uniforms.dirLightDir.xyz);
    float NdotL = dot(normal, lightDir);
    
    // Back-facing surfaces don't need shadow map lookup
    if (NdotL <= 0.0) {
        return 1.0;  // Let diffuse lighting handle darkness
    }
    
    // Transform to light space (no offsets - just raw position)
    vec4 fragPosLightSpace = uniforms.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
    
    // Perspective divide (for ortho this is basically a no-op)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform XY from NDC [-1,1] to texture coords [0,1]
    // Z is already in [0,1] since we use orthoZO projection
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    // Flip Y for Vulkan texture coordinates
    projCoords.y = 1.0 - projCoords.y;
    
    // Check if outside shadow map
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0; // Outside shadow frustum = fully lit
    }
    
    // Use hardware depth bias (applied during shadow map generation)
    // The comparison sampler handles the actual test
    float currentDepth = projCoords.z;
    
    // PCF shadow sampling
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    int samples = uniforms.pcfSamples;
    if (samples == 0) {
        // Hard shadows
        shadow = texture(shadowMap, vec3(projCoords.xy, currentDepth));
    } else {
        // Soft shadows with PCF
        int kernelSize = samples * 2 + 1;
        float numSamples = float(kernelSize * kernelSize);
        
        for (int x = -samples; x <= samples; ++x) {
            for (int y = -samples; y <= samples; ++y) {
                vec2 offset = vec2(x, y) * texelSize;
                shadow += texture(shadowMap, vec3(projCoords.xy + offset, currentDepth));
            }
        }
        shadow /= numSamples;
    }
    
    return shadow;
}

vec3 calculateDirectionalLight(vec3 normal, vec3 viewDir, float shadow) {
    vec3 lightDir = normalize(-uniforms.dirLightDir.xyz);
    float intensity = uniforms.dirLightDir.w;
    
    // Diffuse (Lambert)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * uniforms.dirLightColor.rgb * materialDiffuse * intensity;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uniforms.shininess);
    vec3 specular = spec * uniforms.dirLightColor.rgb * materialSpecular * intensity;
    
    // Apply shadow to diffuse and specular (ambient is not affected)
    return (diffuse + specular) * shadow;
}

vec3 calculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightPos = light.positionRadius.xyz;
    float radius = light.positionRadius.w;
    vec3 lightColor = light.colorIntensity.rgb;
    float intensity = light.colorIntensity.w;
    
    vec3 lightDir = lightPos - fragPos;
    float distance = length(lightDir);
    lightDir = normalize(lightDir);
    
    // Attenuation (smooth falloff)
    float attenuation = 1.0 - smoothstep(0.0, radius, distance);
    attenuation *= attenuation; // Quadratic falloff
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * materialDiffuse * intensity * attenuation;
    
    // Specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uniforms.shininess);
    vec3 specular = spec * lightColor * materialSpecular * intensity * attenuation;
    
    return diffuse + specular;
}

void main() {
    vec3 normal = normalize(inWorldNormal);
    vec3 viewDir = normalize(uniforms.cameraPos.xyz - inWorldPos);
    
    // Ambient (not affected by shadows)
    vec3 ambient = uniforms.ambientColor.rgb * materialDiffuse;
    
    // Calculate shadow for directional light
    float shadow = calculateShadow(inWorldPos, normal);
    
    // Directional light with shadow
    vec3 lighting = calculateDirectionalLight(normal, viewDir, shadow);
    
    // Forward+ path: read lights from tile buffer
    ivec2 screenPos = ivec2(gl_FragCoord.xy);
    ivec2 tileId = screenPos / TILE_SIZE;
    
    // CRITICAL: Use uint for all tile calculations to match compute shader exactly
    uint screenWidthU = uint(uniforms.screenSize.x);
    uint numTilesX = (screenWidthU + uint(TILE_SIZE) - 1u) / uint(TILE_SIZE);
    uint tileIndex = uint(tileId.y) * numTilesX + uint(tileId.x);
    uint tileOffset = tileIndex * (MAX_LIGHTS_PER_TILE + 1);
    
    uint lightCount = tileLightIndices.data[tileOffset];
    lightCount = min(lightCount, MAX_LIGHTS_PER_TILE);
    
    // Read lights from tile
    int totalLights = lightBuffer.numLights;
    for (uint i = 0; i < lightCount; i++) {
        uint lightIndex = tileLightIndices.data[tileOffset + 1 + i];
        if (lightIndex < uint(totalLights)) {
            lighting += calculatePointLight(lightBuffer.lights[lightIndex], normal, inWorldPos, viewDir);
        }
    }
    
    vec3 finalColor = ambient + lighting;
    
    // Apply instance color as a tint
    finalColor *= inColor.rgb;
    
    // Output HDR color (tone mapping and gamma done in post-process)
    outColor = vec4(finalColor, inColor.a);
}
