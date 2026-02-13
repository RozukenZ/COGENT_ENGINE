#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// OUTPUT KE G-BUFFER (Sesuai urutan di GBuffer.cpp)
layout(location = 0) out vec4 outAlbedo;   // Warna
layout(location = 1) out vec4 outNormal;   // Normal Geometry
layout(location = 2) out vec2 outVelocity; // Velocity (Untuk TAA nanti)

void main() {
    // 1. Albedo (Sementara kita kasih warna merah bata dulu buat test)
    outAlbedo = vec4(1.0, 0.2, 0.2, 1.0); 

    // 2. Normal (Encode dari range -1..1 ke 0..1)
    outNormal = vec4(normalize(fragNormal), 1.0);

    // 3. Velocity (Placeholder 0 dulu untuk Phase 1)
    outVelocity = vec2(0.0, 0.0);
}