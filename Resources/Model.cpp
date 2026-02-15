#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "Model.hpp"
#include <iostream>
#include <unordered_map>
#include "../Core/VulkanUtils.hpp"
#include "../Core/Types.hpp" // Hashes are already defined here!

// [FIX] REMOVED the 'namespace std { hash... }' block entirely.
// It is now inside Types.hpp, so we don't need it here.

// [FIX] REMOVED 'bool operator=='
// It is now inside the Vertex struct in Types.hpp.

void Model::loadModel(VkDevice device, VkPhysicalDevice physDevice, const std::string& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Load file .obj
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        throw std::runtime_error("Failed to load model: " + warn + err);
    }

    // Temporary container
    std::vector<Vertex> localVertices;
    std::vector<uint32_t> localIndices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    // Loop over shapes
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // Position
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // Normal
            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            // TexCoord
            if (index.texcoord_index >= 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip V
                };
            }

            // Color (Default Putih)
            vertex.color = {1.0f, 1.0f, 1.0f};

            // Deduplikasi Vertex
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(localVertices.size());
                localVertices.push_back(vertex);
            }
            localIndices.push_back(uniqueVertices[vertex]);
        }
    }

    // Simpan ke member class
    this->vertices = localVertices;
    this->indices = localIndices;

    // Buat Buffer Vulkan
    createVertexBuffer(device, physDevice);
    createIndexBuffer(device, physDevice);

    std::cout << "Model loaded: " << filepath << " (Vertices: " << vertices.size() << ")" << std::endl;
}

// [NEW] Implementation of createVertexBuffer to keep code clean
void Model::createVertexBuffer(VkDevice device, VkPhysicalDevice physDevice) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // Create Staging Buffer (Host Visible)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physDevice, bufferSize, 
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 stagingBuffer, stagingBufferMemory);

    // Map and Copy
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // Create Vertex Buffer (Device Local - Fast)
    createBuffer(device, physDevice, bufferSize, 
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                 vertexBuffer, vertexBufferMemory);

    // Copy from Staging to Vertex Buffer
    // (You need a copyBuffer helper, or for now just use Host Visible for simplicity if copyBuffer is missing)
    // For simplicity in this fix, I will assume you are using the previous Host Visible logic directly:
    
    // [FALLBACK SIMPLE VERSION IF copyBuffer IS MISSING]
    // Re-create as HOST_VISIBLE for now to avoid complexity
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    createBuffer(device, physDevice, bufferSize, 
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 vertexBuffer, vertexBufferMemory);
    
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, vertexBufferMemory);
}

void Model::createIndexBuffer(VkDevice device, VkPhysicalDevice physDevice) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    
    createBuffer(device, physDevice, bufferSize, 
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 indexBuffer, indexBufferMemory);

    void* data;
    vkMapMemory(device, indexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, indexBufferMemory);
    
    indexCount = static_cast<uint32_t>(indices.size());
}

void Model::draw(VkCommandBuffer cmd) {
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

void Model::cleanup(VkDevice device) {
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
}

void Model::createBuffer(VkDevice device, VkPhysicalDevice physDevice, VkDeviceSize size, 
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                         VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    
    // Assuming findMemoryType is defined in VulkanUtils or copied here
    // If external, include header. If inside Model, ensure declaration exists.
    allocInfo.memoryTypeIndex = findMemoryType(physDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}
