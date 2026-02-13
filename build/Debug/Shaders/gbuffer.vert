#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

// Data Kamera (Nanti kita kirim lewat Descriptor Set)
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
} ubo;

// Data Model (Push Constant - Sangat Cepat)
layout(push_constant) uniform PushConsts {
    mat4 model;
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    
    // Hitung Normal di World Space
    fragNormal = mat3(push.model) * inNormal;
    
    fragTexCoord = inTexCoord;
    
    gl_Position = ubo.proj * ubo.view * worldPos;
}