#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Set 1: G-Buffer Samplers (Matches DeferredLightingPass descriptor layout)
layout(set = 1, binding = 0) uniform sampler2D samplerPosition;
layout(set = 1, binding = 1) uniform sampler2D samplerNormal;
layout(set = 1, binding = 2) uniform sampler2D samplerAlbedo;
layout(set = 1, binding = 3) uniform sampler2D samplerMaterial;
layout(set = 1, binding = 5) uniform sampler2D samplerSSAO;

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

const float PI = 3.14159265359;

// PBR Functions
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 WorldPos = texture(samplerPosition, inUV).rgb;
    vec3 Normal = texture(samplerNormal, inUV).rgb;
    vec3 Albedo = texture(samplerAlbedo, inUV).rgb;
    vec4 Material = texture(samplerMaterial, inUV);
    
    float metallic = Material.r;
    float roughness = max(Material.g, 0.05); // Limit minimum roughness to avoid division by zero
    float ao = Material.b;
    
    if (length(Normal) < 0.01) {
        outColor = vec4(0.05, 0.05, 0.08, 1.0);
        return;
    }

    vec3 N = normalize(Normal);
    vec3 V = normalize(ubo.viewPos - WorldPos);
    
    // F0 is the base reflectivity
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, Albedo, metallic);
    
    // Output radiance
    vec3 Lo = vec3(0.0);

    // 1. Directional light (from CameraUBO)
    vec3 L = normalize(-ubo.lightDirection);
    vec3 H = normalize(V + L);
    
    vec3 radiance = ubo.lightColor * ubo.lightIntensity;
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
    
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular     = numerator / denominator;
    
    // kS is equal to Fresnel
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;	
    
    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * Albedo / PI + specular) * radiance * NdotL;
    
    // 2. Clustered Point Lights
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
            vec3 plH = normalize(V + plL);
            
            float atten = clamp(1.0 - (dist * dist) / (radius * radius), 0.0, 1.0);
            atten *= atten;
            
            vec3 plRadiance = light.colorAndIntensity.xyz * light.colorAndIntensity.w * atten;
            
            // Cook-Torrance BRDF
            float plNDF = DistributionGGX(N, plH, roughness);   
            float plG   = GeometrySmith(N, V, plL, roughness);      
            vec3 plF    = fresnelSchlick(max(dot(plH, V), 0.0), F0);       
            
            vec3 plNumerator    = plNDF * plG * plF;
            float plDenominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, plL), 0.0) + 0.0001;
            vec3 plSpecular     = plNumerator / plDenominator;
            
            vec3 plkS = plF;
            vec3 plkD = vec3(1.0) - plkS;
            plkD *= 1.0 - metallic;	
            
            float plNdotL = max(dot(N, plL), 0.0);
            Lo += (plkD * Albedo / PI + plSpecular) * plRadiance * plNdotL;
        }
    }
    
    // Ambient lighting (we can add IBL here later)
    float ssao = texture(samplerSSAO, inUV).r;
    vec3 ambient = vec3(0.03) * Albedo * ao * ssao;
    
    vec3 color = ambient + Lo;
    
    outColor = vec4(color, 1.0);
}