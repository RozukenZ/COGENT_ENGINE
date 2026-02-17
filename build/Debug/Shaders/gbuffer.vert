#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

// Output untuk Velocity Calculation
layout(location = 3) out vec4 currentPos;
layout(location = 4) out vec4 prevPos;

// UBO Kamera (Set 0)
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    mat4 prevView; 
    mat4 prevProj; 
} ubo;

layout(push_constant) uniform PushConsts {
    mat4 model;
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    
    // Kirim data standar ke fragment shader
    fragPos = worldPos.xyz;
    fragNormal = mat3(push.model) * inNormal;
    fragTexCoord = inTexCoord;
    
    // 1. Hitung Posisi Frame SEKARANG
    currentPos = ubo.proj * ubo.view * worldPos;
    
    // 2. Hitung Posisi Frame LALU (Untuk TAA)
    // Asumsi model diam (static mesh). Kalau model bergerak, butuh prevModel matrix.
    prevPos = ubo.prevProj * ubo.prevView * worldPos;

    gl_Position = currentPos;
}