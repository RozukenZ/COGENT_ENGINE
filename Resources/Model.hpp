#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "../Core/Types.hpp" // [FIX] Untuk struct Vertex
#include "PrimitiveMesh.hpp" // [OPSIONAL] Jika loadFromMesh butuh PrimitiveMesh

// Forward declaration biar tidak circular dependency
// Forward declaration biar tidak circular dependency
struct PrimitiveMesh; 

class Model {
public:
    void loadModel(VkDevice device, VkPhysicalDevice physDevice, const std::string& filepath);
    void draw(VkCommandBuffer cmd); 
    void cleanup(VkDevice device);

    // Fungsi load data manual (untuk Primitive Mesh)
    // Note: Kita ganti parameternya jadi vector langsung biar lebih fleksibel dan tidak wajib include PrimitiveMesh.hpp di sini
    void loadFromMesh(VkDevice device, VkPhysicalDevice physDevice, const std::vector<Vertex>& inVertices, const std::vector<uint32_t>& inIndices) {
        this->vertices = inVertices;
        this->indices = inIndices;
        createVertexBuffer(device, physDevice);
        createIndexBuffer(device, physDevice);
    }

private:
    // Data CPU
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Data GPU (Buffer)
    // [FIX] Hanya deklarasikan SEKALI dengan inisialisasi
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;

    // Helper internal
    void createBuffer(VkDevice device, VkPhysicalDevice physDevice, VkDeviceSize size, 
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
                      
    void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice);
    void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice);
};