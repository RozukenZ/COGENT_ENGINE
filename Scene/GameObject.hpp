// Contoh isi GameObject.hpp
#pragma once
#include <glm/glm.hpp>
#include <string>

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler angles (Degrees)
    glm::vec3 scale = glm::vec3(1.0f);
    
    // Helper untuk convert ke Matrix (dipakai ImGuizmo & Shader)
    glm::mat4 toMatrix(); 
};

struct GameObject {
    std::string name;
    Transform transform;
    glm::vec4 color = glm::vec4(1.0f); // Default Putih
    int meshID; // 0 = Cube, 1 = Sphere, dst.
};