#version 450

// INPUT DARI VERTEX SHADER
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// [PENTING] Kita harus menangkap output lokasi 3 & 4 dari Vertex Shader
layout(location = 3) in vec4 currentPos; // Posisi Clip Space Frame Sekarang
layout(location = 4) in vec4 prevPos;    // Posisi Clip Space Frame Lalu

// OUTPUT KE G-BUFFER
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec2 outVelocity;

// INPUT TEXTURE (SET 1)
layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    // 1. ALBEDO: Ambil warna dari texture (Phase 2)
    // Jika texture belum dimuat sempurna, kadang bisa hitam/crash, pastikan descriptor valid
    outAlbedo = texture(texSampler, fragTexCoord);

    // 2. NORMAL: Normalisasi vector normal
    outNormal = vec4(normalize(fragNormal), 1.0);

    // 3. VELOCITY: Hitung pergerakan pixel (Phase 1 - TAA)
    
    // A. Ubah dari Clip Space ke NDC (Perspective Divide) -> Range [-1, 1]
    vec2 a = (currentPos.xy / currentPos.w);
    vec2 b = (prevPos.xy / prevPos.w);

    // B. Ubah dari NDC ke UV Space -> Range [0, 1]
    // Rumus: uv = ndc * 0.5 + 0.5
    vec2 uvCurrent = a * 0.5 + 0.5;
    vec2 uvPrev    = b * 0.5 + 0.5;

    // C. Hitung Delta (Seberapa jauh pixel geser dari frame lalu)
    outVelocity = uvCurrent - uvPrev;
}