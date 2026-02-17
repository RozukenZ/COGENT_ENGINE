#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "../Core/Types.hpp"
#include "../Core/Math/Frustum.hpp"

struct PrimitiveMesh {
    // Member Data for Generation
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // GPU Buffers (Optional if just using for generation)
    VkBuffer vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory vertexBufferMemory{VK_NULL_HANDLE};
    VkBuffer indexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory indexBufferMemory{VK_NULL_HANDLE};
    uint32_t indexCount = 0;

    // Bounding Volume
    Cogent::Math::AABB aabb;

    // Generation Methods
    void createCube();
    void createSphere(float radius, int sectors, int stacks);
    void createCapsule(float radius, float height, int sectors, int stacks);
    void createCylinder(float radius, float height, int sectors);
    void createTriangle();

    void cleanup(VkDevice device) {
        if (vertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, vertexBuffer, nullptr);
        if (vertexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, vertexBufferMemory, nullptr);
        if (indexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, indexBuffer, nullptr);
        if (indexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, indexBufferMemory, nullptr);
    }
};