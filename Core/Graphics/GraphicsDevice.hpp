#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <stdexcept>
#include <iostream>

struct QueueFamilyIndices; // Forward declaration

class GraphicsDevice {
public:
    GraphicsDevice(bool enableValidationLayers = true);
    ~GraphicsDevice();

    void init(VkSurfaceKHR surface); // Requires surface to pick suitable GPU
    void cleanup();

    VkInstance getInstance() const { return instance; }
    VkDevice getDevice() const { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }
    VkCommandPool getCommandPool() const { return commandPool; }

    // Helper functions
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
    // Static helpers for device selection
    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    void createInstance();
    void pickPhysicalDevice(VkSurfaceKHR surface);
    void createLogicalDevice(VkSurfaceKHR surface);
    void createCommandPool();

    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkCommandPool commandPool;

    bool enableValidationLayers;
};
