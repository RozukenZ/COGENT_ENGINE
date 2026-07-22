#version 450

// INPUT FROM VERTEX SHADER
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragTexCoord;

// OUTPUT TO G-BUFFER (Must match GBuffer attachment order: 0=Position, 1=Normal, 2=Albedo)
layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

// INPUT TEXTURE (SET 1)
layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    // 1. POSITION: World-space position for Deferred Lighting
    outPosition = vec4(fragPos, 1.0);

    // 2. NORMAL: Normalized world-space normal
    outNormal = vec4(normalize(fragNormal), 1.0);

    // 3. ALBEDO: Object color * texture
    vec4 texColor = texture(texSampler, fragTexCoord);
    outAlbedo = vec4(fragColor * texColor.rgb, texColor.a);
}