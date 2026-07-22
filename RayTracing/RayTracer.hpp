#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <functional> // [FIX] Required for std::function
#include <glm/glm.hpp>
#include "../Core/Types.hpp"
#include "../Core/Camera.hpp"

// Struktur data yang dikirim ke Compute Shader
struct RayTracingUniform {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::vec4 lightPos;
    glm::vec4 objectColor;
    float time;
};

// Primitive Sphere untuk Ray Tracing (Sederhana)
struct Sphere {
    glm::vec3 center;
    float radius;
    glm::vec3 color;
    float padding;
};

class RayTracer {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, VkExtent2D extent);
    void cleanup(VkDevice device);
    void render(VkCommandBuffer cmd, VkDescriptorSet targetImageDescriptor, Camera& camera, float time);
    
    VkDescriptorSet getOutputDescriptorSet() { return descriptorSet; }

private:
   
    VkDevice device;
    VkExtent2D extent;
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    
    // Resources
    VkImage storageImage;
    VkDeviceMemory storageImageMemory;
    VkImageView storageImageView;
    VkSampler sampler;

    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;

    VkBuffer sphereBuffer;
    VkDeviceMemory sphereBufferMemory;

    void createStorageImage(VkPhysicalDevice physicalDevice);
    void createUniformBuffer(VkPhysicalDevice physicalDevice);
    void createSphereBuffer(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);
    void createDescriptors();
    void createPipeline();
    
    // Helpers
    void createBuffer(VkPhysicalDevice physDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void oneTimeSubmit(VkCommandPool commandPool, VkQueue queue, std::function<void(VkCommandBuffer)> func);
};
