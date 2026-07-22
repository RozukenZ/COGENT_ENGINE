#version 450

// INPUT FROM VERTEX SHADER
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in vec4 currClipPos;
layout(location = 5) in vec4 prevClipPos;

// OUTPUT TO G-BUFFER (Must match GBuffer attachment order: 0=Position, 1=Normal, 2=Albedo, 3=Material, 4=Velocity)
layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outMaterial;
layout(location = 4) out vec4 outVelocity;

// INPUT TEXTURE (SET 1)
layout(set = 1, binding = 0) uniform sampler2D texSampler;

// PUSH CONSTANT (Must match ObjectPushConstant in C++)
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 prevModel;
    vec4 color;
    int id;
    float metallic;
    float roughness;
} pc;

void main() {
    // 1. POSITION: World-space position for Deferred Lighting
    outPosition = vec4(fragPos, 1.0);

    // 2. NORMAL: Normalized world-space normal
    outNormal = vec4(normalize(fragNormal), 1.0);

    // 3. ALBEDO: Object color * texture
    vec4 texColor = texture(texSampler, fragTexCoord);
    outAlbedo = vec4(fragColor * texColor.rgb, texColor.a);
    
    // 4. MATERIAL: R=Metallic, G=Roughness, B=AO, A=Unused
    outMaterial = vec4(pc.metallic, pc.roughness, 1.0, 0.0);

    // 5. VELOCITY: Screen space motion vector
    vec2 currNDC = currClipPos.xy / currClipPos.w;
    vec2 prevNDC = prevClipPos.xy / prevClipPos.w;
    
    // Convert NDC [-1, 1] to UV [0, 1] motion
    vec2 currUV = currNDC * 0.5 + 0.5;
    vec2 prevUV = prevNDC * 0.5 + 0.5;
    
    outVelocity = vec4(currUV - prevUV, 0.0, 0.0);
}