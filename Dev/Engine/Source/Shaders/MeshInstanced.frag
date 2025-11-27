#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec4 outColor;

#define MAX_POINT_LIGHTS 8

struct PointLight {
    vec4 position;     // xyz = position, w = range
    vec4 color;        // xyz = color, w = intensity
};

layout(std140, set = 3, binding = 0) uniform LightUniforms {
    vec4 ambientColor;
    vec4 directionalDir;
    vec4 directionalColor;
    vec4 cameraPos;
    PointLight pointLights[MAX_POINT_LIGHTS];
    int numPointLights;
} lights;

// Material properties (could be per-instance later)
const vec3 materialDiffuse = vec3(0.8);
const vec3 materialSpecular = vec3(1.0);
const float shininess = 32.0;

vec3 calculateDirectionalLight(vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-lights.directionalDir.xyz);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lights.directionalColor.rgb * materialDiffuse;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3 specular = spec * lights.directionalColor.rgb * materialSpecular;
    
    return diffuse + specular;
}

vec3 calculatePointLight(int index, vec3 normal, vec3 fragPos, vec3 viewDir) {
    PointLight light = lights.pointLights[index];
    
    vec3 lightPos = light.position.xyz;
    float range = light.position.w;
    vec3 lightColor = light.color.rgb;
    float intensity = light.color.w;
    
    vec3 lightDir = normalize(lightPos - fragPos);
    float distance = length(lightPos - fragPos);
    
    // Attenuation
    float attenuation = 1.0 / (1.0 + (distance / range) * (distance / range));
    attenuation = max(attenuation, 0.0);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * materialDiffuse * intensity;
    
    // Specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3 specular = spec * lightColor * materialSpecular * intensity;
    
    return (diffuse + specular) * attenuation;
}

void main() {
    vec3 normal = normalize(inWorldNormal);
    vec3 viewDir = normalize(lights.cameraPos.xyz - inWorldPos);
    
    // Ambient
    vec3 ambient = lights.ambientColor.rgb * materialDiffuse;
    
    // Directional light
    vec3 lighting = calculateDirectionalLight(normal, viewDir);
    
    // Point lights
    for (int i = 0; i < lights.numPointLights && i < MAX_POINT_LIGHTS; ++i) {
        lighting += calculatePointLight(i, normal, inWorldPos, viewDir);
    }
    
    vec3 finalColor = ambient + lighting;
    
    // Apply instance color as a tint
    finalColor *= inColor.rgb;
    
    // Tone mapping (simple Reinhard)
    finalColor = finalColor / (finalColor + vec3(1.0));
    
    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0 / 2.2));
    
    outColor = vec4(finalColor, inColor.a);
}
