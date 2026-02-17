#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D samplerPosition;
layout(binding = 1) uniform sampler2D samplerNormal;
layout(binding = 2) uniform sampler2D samplerAlbedo;

layout(binding = 3) uniform LightUBO {
    vec4 lightPos;
    vec4 lightColor;
    vec4 viewPos;
} ubo;

void main() {
    // 1. Retrieve data from G-Buffer
    vec3 FragPos = texture(samplerPosition, inUV).rgb;
    vec3 Normal = texture(samplerNormal, inUV).rgb;
    vec3 Albedo = texture(samplerAlbedo, inUV).rgb;
    
    // 2. Calculate Lighting (Simple Phong/PBR)
    vec3 lightDir = normalize(ubo.lightPos.xyz - FragPos);
    vec3 viewDir = normalize(ubo.viewPos.xyz - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    
    // Diffuse
    float diff = max(dot(Normal, lightDir), 0.0);
    vec3 diffuse = diff * ubo.lightColor.rgb;
    
    // Specular
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), 32.0); // Hardcoded shiness
    vec3 specular = vec3(0.5) * spec; // Specular intensity
    
    // Ambient
    vec3 ambient = vec3(0.1) * Albedo; // Simple ambient
    
    vec3 lighting = (ambient + diffuse + specular) * Albedo;
    
    outColor = vec4(lighting, 1.0);
}