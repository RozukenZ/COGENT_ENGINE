#version 450

layout (location = 0) out vec4 outColor;

// INPUT: Kita membaca hasil dari G-Buffer (Geometry Pass)
// Set 0, Binding 0: Albedo Texture
// Set 0, Binding 1: Normal Texture
// Set 0, Binding 2: Depth/Position (Nanti)
layout (set = 0, binding = 0) uniform sampler2D samplerAlbedo;
layout (set = 0, binding = 1) uniform sampler2D samplerNormal;

void main() {
    // Cari tahu koordinat layar saat ini (0.0 sampai 1.0)
    vec2 uv = gl_FragCoord.xy / vec2(1280.0, 720.0); // Hardcode resolusi dulu, nanti pakai Uniform

    // 1. BACA DATA DARI G-BUFFER
    vec3 albedo = texture(samplerAlbedo, uv).rgb;
    vec3 normal = texture(samplerNormal, uv).rgb;

    // 2. DEFINISI LAMPU (Directional Light - Matahari)
    // Nanti kita kirim ini dari CPU, sekarang hardcode dulu biar jalan
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5)); // Arah datang sinar
    vec3 lightColor = vec3(1.0, 1.0, 1.0); // Warna putih
    float ambientStrength = 0.1;

    // 3. HITUNG PENCAHAYAAN (Phong Shading Sederhana)
    // Ambient (Cahaya dasar agar tidak gelap gulita)
    vec3 ambient = ambientStrength * albedo;

    // Diffuse (Cahaya yang mengenai permukaan)
    // Hitung sudut antara arah lampu dan arah normal permukaan (Dot Product)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * albedo;

    // 4. HASIL AKHIR
    outColor = vec4(ambient + diffuse, 1.0);
}