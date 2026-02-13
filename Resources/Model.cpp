#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "Model.hpp"
#include <iostream>
#include <unordered_map>
#include "../Core/VulkanUtils.hpp"

// Helper untuk hashing vertex agar bisa disimpan di unordered_map
namespace std {
    template<> struct hash<glm::vec3> {
        size_t operator()(glm::vec3 const& v) const {
            size_t h = 0;
            h ^= hash<float>()(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= hash<float>()(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= hash<float>()(v.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    template<> struct hash<glm::vec2> {
        size_t operator()(glm::vec2 const& v) const {
            size_t h = 0;
            h ^= hash<float>()(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= hash<float>()(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

// Operator == diperlukan untuk unordered_map
bool operator==(const Vertex& lhs, const Vertex& rhs) {
    return lhs.pos == rhs.pos && lhs.normal == rhs.normal && lhs.texCoord == rhs.texCoord;
}

void Model::loadModel(VkDevice device, VkPhysicalDevice physDevice, const std::string& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // 1. Baca File .OBJ
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        throw std::runtime_error("Gagal memuat model: " + warn + err);
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    // 2. Konversi Data OBJ ke Format Vertex Kita
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // Ambil Posisi
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // Ambil Normal (Jika ada)
            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            // Ambil TexCoord (Jika ada)
            if (index.texcoord_index >= 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip V karena Vulkan koordinatnya terbalik
                };
            }

            // Deduplikasi Vertex (Optimasi Memori)
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    vertexCount = static_cast<uint32_t>(vertices.size());
    indexCount = static_cast<uint32_t>(indices.size());

    // 3. Upload ke GPU (Vertex Buffer)
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    
    // Note: Di engine production, kita pakai Staging Buffer. 
    // Untuk sekarang kita pakai Host Visible dulu biar simpel, nanti dioptimize.
    createBuffer(device, physDevice, bufferSize, 
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 vertexBuffer, vertexBufferMemory);

    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, vertexBufferMemory);

    // 4. Upload ke GPU (Index Buffer)
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
    createBuffer(device, physDevice, indexBufferSize, 
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 indexBuffer, indexBufferMemory);

    void* indexData;
    vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &indexData);
    memcpy(indexData, indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(device, indexBufferMemory);

    std::cout << "Model loaded: " << filepath << " (Vertices: " << vertexCount << ")" << std::endl;
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
    allocInfo.memoryTypeIndex = findMemoryType(physDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}