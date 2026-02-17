#pragma once

// Vulkan & Standard Libraries
#include <vulkan/vulkan.h>
#include <vector>
#include <array> 
#include <string>
#include <functional> // Required for std::hash

// GLM Configuration
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL // Required for gtx/hash
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> // [FIX] Required for glm::value_ptr
#include <glm/gtx/hash.hpp> // Automatically handles hash for vec2, vec3, etc.

// ==========================================
// 1. VERTEX STRUCT
// ==========================================
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 texCoord;

    // [FIX] Overload == operator (Required for unordered_map)
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && 
               normal == other.normal && texCoord == other.texCoord;
    }

    // Helper to tell Vulkan how to read this struct from memory
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // Normal
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);

        // TexCoord
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

// ==========================================
// 2. UNIFORM BUFFER OBJECT (Camera Data)
// Updated to support Animation (Time) and TAA (Previous Matrices)
// ==========================================
struct CameraUBO {
    // 1. Main Matrices
    alignas(16) glm::mat4 view;       // Current View Matrix
    alignas(16) glm::mat4 proj;       // Current Projection Matrix

    // 2. Previous Matrices (Critical for TAA / Motion Blur)
    alignas(16) glm::mat4 prevView;   
    alignas(16) glm::mat4 prevProj;   

    // 3. Position & Time Data
    // Alignment trick: vec3 is 12 bytes. We add 'time' (4 bytes) right after
    // to perfectly fit 16 bytes alignment. Efficient!
    alignas(16) glm::vec3 viewPos;    // Eye Position (12 bytes)
    float time;                       // Total time elapsed (4 bytes) -> Total 16 Bytes

    // 4. Extra Data
    alignas(16) float deltaTime;      // Time between frames (physics/anim speed)
    float padding[3];                 // Padding to ensure structure alignment
};

// ==========================================
// 3. PUSH CONSTANT (Per-Object Data)
// ==========================================
struct ObjectPushConstant {
    glm::mat4 model;
    alignas(16) glm::vec4 color; // Default color (RGBA) -> Color Wheel control
    int id; // Selection ID
    int padding[3]; // Padding for 16-byte alignment if needed
};

// ==========================================
// 4. GLOBAL LIGHTING (Sun)
// ==========================================
struct GlobalLightUBO {
    alignas(16) glm::vec3 direction; // Light direction
    alignas(16) glm::vec3 color;     // Light color
    float intensity;                 // Light intensity
};

// ==========================================
// 5. GAME OBJECT (Scene Representation)
// ==========================================
struct GameObject {
    std::string name;       // Name of the object (for editor)
    glm::mat4 model;        // Transform model (position, rotation, scale)
    glm::vec4 color;        // Color for editor visualization
    int id;                 // Unique ID for picking
    int meshID;             // [FIX] Added meshID to identify mesh type (0=Cube, 1=Sphere, etc.)
    
    // Bounding Volume (World Space) - Updated when model matrix changes
    // Using a simple struct or including Frustum header if safely forward declared.
    // To avoid header cycles, we define struct here or use glm::vec3 min/max directly.
    glm::vec3 aabbMin = glm::vec3(-1.0f);
    glm::vec3 aabbMax = glm::vec3(1.0f);

    ObjectPushConstant getPushConstant() const {
        ObjectPushConstant pc{};
        pc.model = model;
        pc.color = color;
        return pc;
    }
};

// ==========================================
// 6. HASH SPECIALIZATION
// ==========================================
namespace std {
    // Note: hash<glm::vec3> and hash<glm::vec2> are now handled by <glm/gtx/hash.hpp>
    // We only need to define the hash for our custom Vertex struct.

    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            // Combine hashes using bit shifting and XOR
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}