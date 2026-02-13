#pragma once

// Definisi konfigurasi GLM untuk Vulkan
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Struktur Data Kamera Global (View & Projection)
// Diupdate untuk mendukung Animasi (Time) dan TAA (Previous Matrices)
struct CameraUBO {
    // 1. Matriks Utama
    alignas(16) glm::mat4 view;       // Posisi & Arah Kamera saat ini
    alignas(16) glm::mat4 proj;       // Lensa Perspektif saat ini

    // 2. Matriks Frame Sebelumnya (Penting untuk TAA / Motion Blur)
    alignas(16) glm::mat4 prevView;   
    alignas(16) glm::mat4 prevProj;   

    // 3. Data Posisi & Waktu
    // Trik Alignment: vec3 itu 12 byte. Kita tambahkan 'time' (4 byte) tepat setelahnya
    // agar pas menjadi 16 byte (1 baris memori GPU). Sangat efisien!
    alignas(16) glm::vec3 viewPos;    // Posisi mata (12 byte)
    float time;                       // Total waktu berjalan (4 byte) -> Total 16 Byte

    // 4. Data Tambahan
    alignas(16) float deltaTime;      // Waktu antar frame (untuk fisika/animasi speed)
    float padding[3];                 // Padding kosong agar struktur genap (Safety)
};

// Struktur Data Per-Objek (Model Transform)
struct ObjectPushConstant {
    glm::mat4 model;
    alignas(16) glm::vec4 color; // Default color (RGBA) -> Color Wheel control
};

// New UBO for Global Lighting (Sun)
struct GlobalLightUBO {
    alignas(16) glm::vec3 direction; // Lightnbe direction
    alignas(16) glm::vec3 color;     // Light color
    float intensity;                 // Light intensity
};

struct GameObject {
    std::string name;       // Name of the object (for editor)
    glm::mat4 model;        // Transform model (position, rotation, scale)
    glm::vec4 color;        // Colour for editor visualization
    int id;                 // ID Unique for picking
};