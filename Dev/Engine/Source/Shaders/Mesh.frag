#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

// Maximum number of point lights
#define MAX_POINT_LIGHTS 8

// Shadow map sampler (set 2, binding 0 for samplers)
layout(set = 2, binding = 0) uniform sampler2DShadow shadowMap;

// Light uniforms (set 3, binding 0 for uniform buffers)
layout(std140, set = 3, binding = 0) uniform LightUniforms {
    // Directional light
    vec4 dirLightDir;       // xyz = direction, w = intensity
    vec4 dirLightColor;     // rgb = color, a = unused
    vec4 ambientColor;      // rgb = ambient, a = unused
    
    // Camera position for specular
    vec4 cameraPos;         // xyz = position, w = unused
    
    // Point lights
    vec4 pointLightPos[MAX_POINT_LIGHTS];       // xyz = position, w = radius
    vec4 pointLightColor[MAX_POINT_LIGHTS];     // rgb = color, a = intensity
    
    int numPointLights;
    float shininess;
    vec2 _padding;
    
    // Shadow parameters
    mat4 lightSpaceMatrix;
    float shadowBias;
    float shadowNormalBias;
    int pcfSamples;
    int shadowsEnabled;
} lights;

// Material properties
const vec3 materialDiffuse = vec3(0.8, 0.8, 0.8);
const vec3 materialSpecular = vec3(0.5, 0.5, 0.5);

// Calculate shadow factor
float calculateShadow(vec3 fragPosWorld, vec3 normal) {
    if (lights.shadowsEnabled == 0) return 1.0;
    
    // Get light direction for bias calculation
    vec3 lightDir = normalize(-lights.dirLightDir.xyz);
    float NdotL = dot(normal, lightDir);
    
    // If surface is facing away from light, it's in self-shadow (no shadow map needed)
    if (NdotL <= 0.0) {
        return 1.0;  // Let diffuse lighting handle the darkness
    }
    
    // Transform to light space (no offsets)
    vec4 fragPosLightSpace = lights.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform XY from NDC [-1,1] to texture coords [0,1]
    // Z is already in [0,1] since we use orthoZO projection
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.y = 1.0 - projCoords.y;
    
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;
    }
    
    // Hardware depth bias applied during shadow map generation
    float currentDepth = projCoords.z;
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    int samples = lights.pcfSamples;
    if (samples == 0) {
        shadow = texture(shadowMap, vec3(projCoords.xy, currentDepth));
    } else {
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
    vec3 lightDir = normalize(-lights.dirLightDir.xyz);
    float intensity = lights.dirLightDir.w;
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lights.dirLightColor.rgb * materialDiffuse * intensity;
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), lights.shininess);
    vec3 specular = spec * lights.dirLightColor.rgb * materialSpecular * intensity;
    
    return (diffuse + specular) * shadow;
}

vec3 calculatePointLight(int index, vec3 normal, vec3 worldPos, vec3 viewDir) {
    vec3 lightPos = lights.pointLightPos[index].xyz;
    float radius = lights.pointLightPos[index].w;
    vec3 lightColor = lights.pointLightColor[index].rgb;
    float intensity = lights.pointLightColor[index].a;
    
    vec3 lightDir = lightPos - worldPos;
    float distance = length(lightDir);
    lightDir = normalize(lightDir);
    
    float attenuation = 1.0 - smoothstep(0.0, radius, distance);
    attenuation *= attenuation;
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * materialDiffuse * intensity * attenuation;
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), lights.shininess);
    vec3 specular = spec * lightColor * materialSpecular * intensity * attenuation;
    
    return diffuse + specular;
}

void main() {
    vec3 normal = normalize(inWorldNormal);
    vec3 viewDir = normalize(lights.cameraPos.xyz - inWorldPos);
    
    float shadow = calculateShadow(inWorldPos, normal);
    
    vec3 ambient = lights.ambientColor.rgb * materialDiffuse;
    vec3 lighting = calculateDirectionalLight(normal, viewDir, shadow);
    
    // Debug: show if point lights are being received
    // if (lights.numPointLights > 0) {
    //     outColor = vec4(1.0, 0.0, 0.0, 1.0);  // Red = has point lights
    //     return;
    // }
    
    for (int i = 0; i < lights.numPointLights && i < MAX_POINT_LIGHTS; ++i) {
        lighting += calculatePointLight(i, normal, inWorldPos, viewDir);
    }
    
    // Debug: visualize point light contribution
    // vec3 pointLightContrib = vec3(0.0);
    // for (int i = 0; i < lights.numPointLights && i < MAX_POINT_LIGHTS; ++i) {
    //     pointLightContrib += calculatePointLight(i, normal, inWorldPos, viewDir);
    // }
    // outColor = vec4(pointLightContrib, 1.0);
    // return;
    
    vec3 finalColor = ambient + lighting;
    outColor = vec4(finalColor, 1.0);
}
