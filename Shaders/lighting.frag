#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Set 1: G-Buffer Samplers (Matches DeferredLightingPass descriptor layout)
layout(set = 1, binding = 0) uniform sampler2D samplerPosition;
layout(set = 1, binding = 1) uniform sampler2D samplerNormal;
layout(set = 1, binding = 2) uniform sampler2D samplerAlbedo;

// Clustered Lighting SSBOs (Set 2 from LightCulling)
struct PointLight {
    vec4 positionAndRadius;
    vec4 colorAndIntensity;
};

struct LightGrid {
    uint offset;
    uint count;
};

layout(std140, set = 2, binding = 0) readonly buffer LightBuffer {
    PointLight lights[];
};

layout(std430, set = 2, binding = 1) readonly buffer GridBuffer {
    LightGrid grids[];
};

layout(std430, set = 2, binding = 2) readonly buffer IndexBuffer {
    uint globalIndices[];
};

// Set 0: Global Data
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 prevView;
    mat4 prevProj;
    vec3 viewPos;
    float time;
    float deltaTime;
    float _pad1;
    float _pad2;
    float _pad3;
    vec3 lightDirection;
    float _pad4;
    vec3 lightColor;
    float lightIntensity;
    
    // Cluster info
    uvec4 gridDimensions;
    vec2 screenDimensions;
    float zNear;
    float zFar;
} ubo;

void main() {
    vec3 WorldPos = texture(samplerPosition, inUV).rgb;
    vec3 Normal = texture(samplerNormal, inUV).rgb;
    vec3 Albedo = texture(samplerAlbedo, inUV).rgb;
    
    if (length(Normal) < 0.01) {
        outColor = vec4(0.05, 0.05, 0.08, 1.0);
        return;
    }

    vec3 N = normalize(Normal);
    vec3 V = normalize(ubo.viewPos - WorldPos);
    
    // Base ambient
    vec3 ambient = vec3(0.02) * Albedo; 
    vec3 totalLighting = ambient;

    // Directional light (from CameraUBO)
    vec3 L = normalize(-ubo.lightDirection);
    vec3 H = normalize(L + V);
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * ubo.lightColor * ubo.lightIntensity * Albedo;
    float spec = pow(max(dot(N, H), 0.0), 32.0); 
    vec3 specular = vec3(0.5) * spec * ubo.lightIntensity;
    totalLighting += diffuse + specular;
    
    // Clustered Point Lights
    vec4 viewPos = ubo.view * vec4(WorldPos, 1.0);
    vec3 viewPos3 = viewPos.xyz / viewPos.w;
    
    vec2 fragCoord = gl_FragCoord.xy;
    uint sliceZ = uint(max(0.0, log(viewPos3.z / ubo.zNear) / log(ubo.zFar / ubo.zNear) * ubo.gridDimensions.z));
    sliceZ = min(sliceZ, ubo.gridDimensions.z - 1);
    
    vec2 tileSize = ubo.screenDimensions / vec2(ubo.gridDimensions.xy);
    uvec2 tileXY = uvec2(fragCoord / tileSize);
    
    uint clusterIndex = tileXY.x +
                        tileXY.y * ubo.gridDimensions.x +
                        sliceZ * (ubo.gridDimensions.x * ubo.gridDimensions.y);
                        
    LightGrid grid = grids[clusterIndex];
    
    for (uint i = 0; i < grid.count; ++i) {
        uint lightIndex = globalIndices[grid.offset + i];
        PointLight light = lights[lightIndex];
        
        vec3 plL = light.positionAndRadius.xyz - WorldPos;
        float dist = length(plL);
        float radius = light.positionAndRadius.w;
        
        if (dist < radius) {
            plL = normalize(plL);
            vec3 plH = normalize(plL + V);
            
            float atten = clamp(1.0 - (dist * dist) / (radius * radius), 0.0, 1.0);
            atten *= atten;
            
            float plDiff = max(dot(N, plL), 0.0);
            vec3 plDiffuse = plDiff * light.colorAndIntensity.xyz * light.colorAndIntensity.w * Albedo * atten;
            
            float plSpec = pow(max(dot(N, plH), 0.0), 32.0); 
            vec3 plSpecular = vec3(0.5) * plSpec * light.colorAndIntensity.w * atten;
            
            totalLighting += plDiffuse + plSpecular;
        }
    }
    
    outColor = vec4(totalLighting, 1.0);
}