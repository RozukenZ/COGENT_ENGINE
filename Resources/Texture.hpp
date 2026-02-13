#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "../Core/VulkanUtils.hpp"

class Texture {
public:
    void load(VkDevice device, VkPhysicalDevice physDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string& filepath);
    void cleanup(VkDevice device);

    VkImageView getImageView() { return textureImageView; }
    VkSampler getSampler() { return textureSampler; }

private:
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;

    uint32_t width, height, mipLevels;

    // Helper functions internal
    void createImage(VkDevice device, VkPhysicalDevice physDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
    void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer);
};