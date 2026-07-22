#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Set 1: G-Buffer Samplers (Matches DeferredLightingPass descriptor layout)
layout(set = 1, binding = 0) uniform sampler2D samplerPosition;
layout(set = 1, binding = 1) uniform sampler2D samplerNormal;
layout(set = 1, binding = 2) uniform sampler2D samplerAlbedo;

// Set 0: Global Data (Camera + Light) - Must match C++ CameraUBO exactly
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;           // offset 0
    mat4 proj;           // offset 64
    mat4 prevView;       // offset 128
    mat4 prevProj;       // offset 192
    vec3 viewPos;        // offset 256 (alignas(16))
    float time;          // offset 268
    float deltaTime;     // offset 272 (alignas(16))
    float _pad1;         // padding[0]
    float _pad2;         // padding[1]
    float _pad3;         // padding[2]
    vec3 lightDirection; // offset 288 (alignas(16))
    float _pad4;         // implicit padding after vec3
    vec3 lightColor;     // offset 304 (alignas(16))
    float lightIntensity;// offset 316
} ubo;

void main() {
    // 1. Retrieve data from G-Buffer
    vec3 WorldPos = texture(samplerPosition, inUV).rgb;
    vec3 Normal = texture(samplerNormal, inUV).rgb;
    vec3 Albedo = texture(samplerAlbedo, inUV).rgb;
    
    // 2. Calculate Lighting (Blinn-Phong)
    vec3 L = normalize(-ubo.lightDirection);
    vec3 V = normalize(ubo.viewPos - WorldPos);
    vec3 H = normalize(L + V);
    vec3 N = normalize(Normal);
    
    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * ubo.lightColor * ubo.lightIntensity;
    
    // Specular
    float spec = pow(max(dot(N, H), 0.0), 32.0); 
    vec3 specular = vec3(0.5) * spec * ubo.lightIntensity; 
    
    // Ambient
    vec3 ambient = vec3(0.1) * Albedo; 
    
    vec3 lighting = (ambient + diffuse + specular) * Albedo;
    
    // Background detection: if normal is zero, it's empty space
    if (length(Normal) < 0.01) {
        outColor = vec4(0.05, 0.05, 0.08, 1.0); // Dark background
    } else {
        outColor = vec4(lighting, 1.0);
    }
}