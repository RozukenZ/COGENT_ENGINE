#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos; // World position for lighting

// Push Constant (Warna dari Color Wheel)
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 baseColor; // <--- Ini warna dari ImGui
} pConst;

layout(location = 0) out vec4 outColor;

// Simple Directional Light (Hardcoded for now, or via UBO)
const vec3 lightDir = normalize(vec3(1.0, -3.0, -1.0)); // Arah Matahari
const vec3 lightColor = vec3(1.0, 1.0, 0.9); // Warna Matahari (agak kuning)

void main() {
    // 1. Ambil warna Texture
    vec4 texColor = texture(texSampler, fragTexCoord);
    
    // 2. Gabungkan dengan Warna Color Wheel
    // Jika tidak ada texture (putih), maka warna baseColor yang akan tampil
    vec4 finalAlbedo = texColor * pConst.baseColor;

    // 3. Simple Diffuse Lighting (Lambertian)
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, -lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // 4. Ambient Light (Agar sisi gelap tidak hitam pekat)
    vec3 ambient = 0.1 * lightColor;

    // Final Result
    vec3 result = (ambient + diffuse) * finalAlbedo.rgb;
    
    outColor = vec4(result, finalAlbedo.a);
}