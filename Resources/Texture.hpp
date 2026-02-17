#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "../Core/VulkanUtils.hpp"
#include "Streaming/Streamer.hpp" // For StreamableResource

// Inherit from StreamableResource to support streaming
class Texture : public Cogent::Resources::StreamableResource {
public:
    void load(VkDevice device, VkPhysicalDevice physDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string& filepath);
    void cleanup(VkDevice device);

    // StreamableResource Implementation
    void loadCPU() override;
    void uploadGPU(GraphicsDevice& device) override;
    void unload() override;

    VkImageView getImageView() { return textureImageView; }
    VkSampler getSampler() { return textureSampler; }

    // Helpers for Legacy Load (if needed) or internal use
    void createImage(VkDevice device, VkPhysicalDevice physDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
    void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer);

private:
    VkImage textureImage{VK_NULL_HANDLE};
    VkDeviceMemory textureImageMemory{VK_NULL_HANDLE};
    VkImageView textureImageView{VK_NULL_HANDLE};
    VkSampler textureSampler{VK_NULL_HANDLE};

    uint32_t width, height, mipLevels;
    int texChannels;

    // CPU Data for Streaming
    std::vector<unsigned char> pixelData;
    bool isFallback = false;
};