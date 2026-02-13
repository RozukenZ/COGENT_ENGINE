#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

// Pastikan struct Vertex ini sama dengan yang ada di Types.hpp atau main.cpp
// Jika sudah ada di Types.hpp, include file tersebut dan hapus definisi ini agar tidak redefinition error.
#ifndef VERTEX_STRUCT_DEFINED
#define VERTEX_STRUCT_DEFINED
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 texCoord;
};
#endif

class PrimitiveMesh {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const float PI = 3.14159265359f;

    // ==========================================
    // 1. SPHERE (Bola)
    // ==========================================
    void createSphere(float radius, int sectors, int stacks) {
        vertices.clear(); indices.clear();
        float x, y, z, xy;                              
        float nx, ny, nz, lengthInv = 1.0f / radius;    
        float s, t;                                     

        for(int i = 0; i <= stacks; ++i) {
            float stackAngle = PI / 2 - i * PI / stacks;        
            xy = radius * cosf(stackAngle);             
            z = radius * sinf(stackAngle);              

            for(int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * 2 * PI / sectors;           

                x = xy * cosf(sectorAngle);             
                y = xy * sinf(sectorAngle);             
                
                nx = x * lengthInv;
                ny = y * lengthInv;
                nz = z * lengthInv;

                s = (float)j / sectors;
                t = (float)i / stacks;

                vertices.push_back({{x, y, z}, {1.0f, 1.0f, 1.0f}, {nx, ny, nz}, {s, t}});
            }
        }

        for(int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1);     
            int k2 = k1 + sectors + 1;      

            for(int j = 0; j < sectors; ++j, ++k1, ++k2) {
                if(i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }
                if(i != (stacks - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }
    }

    // ==========================================
    // 2. CUBE (Kotak)
    // ==========================================
    void createCube() {
        vertices.clear(); indices.clear();

        // Cube butuh 24 vertex (4 per sisi * 6 sisi) agar lighting (normal) benar di sudut tajam
        // Format: Pos, Color, Normal, UV
        
        // Front Face
        vertices.push_back({{-0.5f, -0.5f,  0.5f}, {1,1,1}, {0, 0, 1}, {0, 1}}); // 0
        vertices.push_back({{ 0.5f, -0.5f,  0.5f}, {1,1,1}, {0, 0, 1}, {1, 1}}); // 1
        vertices.push_back({{ 0.5f,  0.5f,  0.5f}, {1,1,1}, {0, 0, 1}, {1, 0}}); // 2
        vertices.push_back({{-0.5f,  0.5f,  0.5f}, {1,1,1}, {0, 0, 1}, {0, 0}}); // 3
        
        // Back Face
        vertices.push_back({{ 0.5f, -0.5f, -0.5f}, {1,1,1}, {0, 0, -1}, {0, 1}}); // 4
        vertices.push_back({{-0.5f, -0.5f, -0.5f}, {1,1,1}, {0, 0, -1}, {1, 1}}); // 5
        vertices.push_back({{-0.5f,  0.5f, -0.5f}, {1,1,1}, {0, 0, -1}, {1, 0}}); // 6
        vertices.push_back({{ 0.5f,  0.5f, -0.5f}, {1,1,1}, {0, 0, -1}, {0, 0}}); // 7

        // Top Face
        vertices.push_back({{-0.5f,  0.5f,  0.5f}, {1,1,1}, {0, 1, 0}, {0, 1}}); // 8
        vertices.push_back({{ 0.5f,  0.5f,  0.5f}, {1,1,1}, {0, 1, 0}, {1, 1}}); // 9
        vertices.push_back({{ 0.5f,  0.5f, -0.5f}, {1,1,1}, {0, 1, 0}, {1, 0}}); // 10
        vertices.push_back({{-0.5f,  0.5f, -0.5f}, {1,1,1}, {0, 1, 0}, {0, 0}}); // 11

        // Bottom Face
        vertices.push_back({{-0.5f, -0.5f, -0.5f}, {1,1,1}, {0, -1, 0}, {0, 1}}); // 12
        vertices.push_back({{ 0.5f, -0.5f, -0.5f}, {1,1,1}, {0, -1, 0}, {1, 1}}); // 13
        vertices.push_back({{ 0.5f, -0.5f,  0.5f}, {1,1,1}, {0, -1, 0}, {1, 0}}); // 14
        vertices.push_back({{-0.5f, -0.5f,  0.5f}, {1,1,1}, {0, -1, 0}, {0, 0}}); // 15

        // Right Face
        vertices.push_back({{ 0.5f, -0.5f,  0.5f}, {1,1,1}, {1, 0, 0}, {0, 1}}); // 16
        vertices.push_back({{ 0.5f, -0.5f, -0.5f}, {1,1,1}, {1, 0, 0}, {1, 1}}); // 17
        vertices.push_back({{ 0.5f,  0.5f, -0.5f}, {1,1,1}, {1, 0, 0}, {1, 0}}); // 18
        vertices.push_back({{ 0.5f,  0.5f,  0.5f}, {1,1,1}, {1, 0, 0}, {0, 0}}); // 19

        // Left Face
        vertices.push_back({{-0.5f, -0.5f, -0.5f}, {1,1,1}, {-1, 0, 0}, {0, 1}}); // 20
        vertices.push_back({{-0.5f, -0.5f,  0.5f}, {1,1,1}, {-1, 0, 0}, {1, 1}}); // 21
        vertices.push_back({{-0.5f,  0.5f,  0.5f}, {1,1,1}, {-1, 0, 0}, {1, 0}}); // 22
        vertices.push_back({{-0.5f,  0.5f, -0.5f}, {1,1,1}, {-1, 0, 0}, {0, 0}}); // 23

        // Indices (Setiap sisi ada 2 segitiga)
        uint32_t cubeIndices[] = {
            0, 1, 2, 2, 3, 0,       // Front
            4, 5, 6, 6, 7, 4,       // Back
            8, 9, 10, 10, 11, 8,    // Top
            12, 13, 14, 14, 15, 12, // Bottom
            16, 17, 18, 18, 19, 16, // Right
            20, 21, 22, 22, 23, 20  // Left
        };

        for (uint32_t i : cubeIndices) indices.push_back(i);
    }

    // ==========================================
    // 3. TRIANGLE (Segitiga)
    // ==========================================
    void createTriangle() {
        vertices.clear(); indices.clear();
        
        // Segitiga sama sisi sederhana
        vertices.push_back({{-0.5f, -0.5f, 0.0f}, {1,1,1}, {0, 0, 1}, {0.0f, 1.0f}}); // Kiri Bawah
        vertices.push_back({{ 0.5f, -0.5f, 0.0f}, {1,1,1}, {0, 0, 1}, {1.0f, 1.0f}}); // Kanan Bawah
        vertices.push_back({{ 0.0f,  0.5f, 0.0f}, {1,1,1}, {0, 0, 1}, {0.5f, 0.0f}}); // Atas Tengah

        indices = {0, 1, 2};
    }

    // ==========================================
    // 4. CYLINDER (Tabung)
    // ==========================================
    void createCylinder(float radius = 0.5f, float height = 1.0f, int segments = 32) {
        vertices.clear(); indices.clear();
        
        // 1. Generate Body Vertices
        for (int i = 0; i <= segments; i++) {
            float theta = (float)i / (float)segments * 2.0f * PI;
            float x = radius * cosf(theta);
            float z = radius * sinf(theta);
            float u = (float)i / (float)segments;

            // Normal untuk sisi samping mengarah keluar dari pusat
            glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z));

            // Top vertex (Side)
            vertices.push_back({{x, height/2, z}, {1,1,1}, normal, {u, 0.0f}});
            // Bottom vertex (Side)
            vertices.push_back({{x, -height/2, z}, {1,1,1}, normal, {u, 1.0f}});
        }

        // Indices Body (Zig-zag strip)
        for (int i = 0; i < segments; i++) {
            int top1 = i * 2;
            int bottom1 = i * 2 + 1;
            int top2 = (i + 1) * 2;
            int bottom2 = (i + 1) * 2 + 1;

            indices.push_back(bottom1); indices.push_back(top1); indices.push_back(top2);
            indices.push_back(bottom1); indices.push_back(top2); indices.push_back(bottom2);
        }

        // 2. Caps (Tutup Atas & Bawah)
        int baseIndex = (int)vertices.size();
        
        // Center points
        vertices.push_back({{0, height/2, 0}, {1,1,1}, {0,1,0}, {0.5f, 0.5f}}); // Top Center
        vertices.push_back({{0, -height/2, 0}, {1,1,1}, {0,-1,0}, {0.5f, 0.5f}}); // Bottom Center
        int topCenterIdx = baseIndex;
        int botCenterIdx = baseIndex + 1;

        // Cap Circle Vertices
        for (int i = 0; i <= segments; i++) {
            float theta = (float)i / (float)segments * 2.0f * PI;
            float x = radius * cosf(theta);
            float z = radius * sinf(theta);
            float u = (x / radius + 1) * 0.5f;
            float v = (z / radius + 1) * 0.5f;

            // Top Cap Edge
            vertices.push_back({{x, height/2, z}, {1,1,1}, {0,1,0}, {u, v}});
            // Bottom Cap Edge
            vertices.push_back({{x, -height/2, z}, {1,1,1}, {0,-1,0}, {u, v}});
        }

        // Indices Caps
        int circleStart = baseIndex + 2;
        for (int i = 0; i < segments; i++) {
            int currentTop = circleStart + i * 2;
            int nextTop = circleStart + (i + 1) * 2;
            
            int currentBot = circleStart + i * 2 + 1;
            int nextBot = circleStart + (i + 1) * 2 + 1;

            // Top Triangle
            indices.push_back(topCenterIdx);
            indices.push_back(nextTop);
            indices.push_back(currentTop);

            // Bottom Triangle
            indices.push_back(botCenterIdx);
            indices.push_back(currentBot);
            indices.push_back(nextBot);
        }
    }

    // ==========================================
    // 5. CAPSULE (Kapsul)
    // ==========================================
    void createCapsule(float radius = 0.5f, float height = 1.0f, int sectors = 32, int stacks = 16) {
        vertices.clear(); indices.clear();
        
        float cylinderHeight = height - 2 * radius;
        if (cylinderHeight < 0) cylinderHeight = 0; // Safety

        float halfH = cylinderHeight * 0.5f;
        int halfStacks = stacks / 2;

        // Loop Stacks (Dari bawah ke atas)
        for(int i = 0; i <= stacks; ++i) {
            // Mapping i ke sudut bola (phi)
            // 0 -> PI (Bawah -> Atas)
            float phi = (float)i / stacks * PI; 
            
            // Trik Kapsul: Geser posisi Y (Offset)
            // Jika i < setengah, geser ke bawah. Jika i > setengah, geser ke atas.
            float yOffset = (i <= halfStacks) ? -halfH : halfH;
            
            // Hitung posisi di bola unit
            float xy = radius * sinf(phi);
            float y = radius * -cosf(phi); // -cos karena phi 0 ada di bawah (-y)

            // Posisi vertex final
            float finalY = y + yOffset;

            // Loop Sectors (Lingkaran horizontal)
            for(int j = 0; j <= sectors; ++j) {
                float theta = j * 2 * PI / sectors;
                
                float x = xy * cosf(theta);
                float z = xy * sinf(theta);

                // Normal (berdasarkan pusat bola imajiner di offset masing2)
                glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
                
                // UV
                float u = (float)j / sectors;
                float v = (float)i / stacks;

                vertices.push_back({{x, finalY, z}, {1,1,1}, normal, {u, v}});
            }
        }

        // Indices Generation (Sama seperti Sphere)
        for(int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1);     
            int k2 = k1 + sectors + 1;      

            for(int j = 0; j < sectors; ++j, ++k1, ++k2) {
                if(i != 0) {
                    indices.push_back(k1); indices.push_back(k2); indices.push_back(k1 + 1);
                }
                if(i != (stacks - 1)) {
                    indices.push_back(k1 + 1); indices.push_back(k2); indices.push_back(k2 + 1);
                }
            }
        }
    }
};