#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Set 1: G-Buffer Samplers (Matches INTERNAL layout from DeferredLightingPass)
layout(set = 1, binding = 0) uniform sampler2D samplerPosition;
layout(set = 1, binding = 1) uniform sampler2D samplerNormal;
layout(set = 1, binding = 2) uniform sampler2D samplerAlbedo;

// Set 0: Global Data (Camera + Light) (Matches GLOBAL layout from CogentEngine)
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 prevView;
    mat4 prevProj;
    vec3 viewPos;
    float time;
    float deltaTime;
    // Padding handled by alignment usually, but explicit matching is safer
    // vec3 is 12 bytes + 4 bytes padding in C++ struct with alignas(16)
    // float padding[3]; 

    // Light Data
    vec3 lightDirection; // alignas(16)
    // float padding
    vec3 lightColor;     // alignas(16)
    float lightIntensity;
} ubo;

void main() {
    // 1. Retrieve data from G-Buffer
    vec3 WorldPos = texture(samplerPosition, inUV).rgb; // GBuffer 0 is Position
    vec3 Normal = texture(samplerNormal, inUV).rgb;     // GBuffer 1 is Normal
    vec3 Albedo = texture(samplerAlbedo, inUV).rgb;     // GBuffer 2 is Albedo
    
    // Check if valid pixel (e.g. not skybox)
    // Position clear was 0.0. If length(WorldPos) < epsilon, it's background?
    // Or alpha channel usage?
    // For now, simple check.
    
    // 2. Calculate Lighting (Simple Blinn-Phong)
    vec3 L = normalize(-ubo.lightDirection); // Direction TO light
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
    vec3 ambient = vec3(0.05) * Albedo; 
    
    vec3 lighting = (ambient + diffuse + specular) * Albedo;
    
    // Determine if background (simple hack for now: if Normal is 0)
    if (length(Normal) < 0.1) {
        // Background color (Light Blue based on swapchain clear?)
        // Or just let it be lit (0 normal might cause issues)
        outColor = vec4(Albedo, 1.0); // Output albedo directly (which is black if cleared to black)
    } else {
        outColor = vec4(lighting, 1.0);
    }
}