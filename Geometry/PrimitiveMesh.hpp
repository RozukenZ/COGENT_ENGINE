#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "../Core/Types.hpp"

class PrimitiveMesh {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Konstanta PI
    const float PI = 3.14159265359f;

    // Fungsi pembuat bentuk
    void createSphere(float radius, int sectors, int stacks);
    void createCube();
    void createTriangle();
    void createCylinder(float radius, float height, int segments);
    void createCapsule(float radius, float height, int sectors, int stacks);
};