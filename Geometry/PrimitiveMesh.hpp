#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "../Core/Types.hpp"
#include "../Core/Math/Frustum.hpp"

struct PrimitiveMesh {
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;

    // Bounding Volume
    Cogent::Math::AABB aabb;

    void cleanup(VkDevice device) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
    }
};