#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

// Maximum number of point lights
#define MAX_POINT_LIGHTS 8

// Light uniforms
// SDL_GPU with slot 0 in SDL_PushGPUFragmentUniformData maps to binding 0
// But since vertex shader uses bindings 0 and 1, we use set 3 (fragment-only set)
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
} lights;

// Material properties (can be expanded later)
const vec3 materialDiffuse = vec3(0.8, 0.8, 0.8);
const vec3 materialSpecular = vec3(0.5, 0.5, 0.5);

vec3 calculateDirectionalLight(vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-lights.dirLightDir.xyz);
    float intensity = lights.dirLightDir.w;
    
    // Diffuse (Lambert)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lights.dirLightColor.rgb * materialDiffuse * intensity;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), lights.shininess);
    vec3 specular = spec * lights.dirLightColor.rgb * materialSpecular * intensity;
    
    return diffuse + specular;
}

vec3 calculatePointLight(int index, vec3 normal, vec3 worldPos, vec3 viewDir) {
    vec3 lightPos = lights.pointLightPos[index].xyz;
    float radius = lights.pointLightPos[index].w;
    vec3 lightColor = lights.pointLightColor[index].rgb;
    float intensity = lights.pointLightColor[index].a;
    
    vec3 lightDir = lightPos - worldPos;
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
    float spec = pow(max(dot(normal, halfwayDir), 0.0), lights.shininess);
    vec3 specular = spec * lightColor * materialSpecular * intensity * attenuation;
    
    return diffuse + specular;
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
    
    // Output HDR color (tone mapping and gamma done in post-process)
    outColor = vec4(finalColor, 1.0);
}