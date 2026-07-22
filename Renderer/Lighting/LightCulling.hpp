#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include "../Core/Graphics/GraphicsDevice.hpp"
#include "../Core/Graphics/DescriptorManager.hpp"
#include "../Core/Graphics/PipelineCache.hpp"

struct PointLight {
    glm::vec4 positionAndRadius;
    glm::vec4 colorAndIntensity;
};

struct LightGrid {
    uint32_t offset;
    uint32_t count;
};

struct LightCullingUBO {
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 inverseProjection;
    glm::uvec4 gridDimensions; // xyz: dims, w: total light count
    glm::vec2 screenDimensions;
    float zNear;
    float zFar;
};

class LightCulling {
public:
    LightCulling(GraphicsDevice& device, PipelineCache& pipelineCache);
    ~LightCulling();

    void init(VkExtent2D screenExtent, uint32_t maxLights);
    void updateUBO(const LightCullingUBO& uboData);
    void updateLights(const std::vector<PointLight>& lights);
    
    void execute(VkCommandBuffer cmd);
    
    VkDescriptorSet getDescriptorSet() const { return descriptorSet; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

private:
    void createBuffers();
    void createDescriptors();
    void createPipeline();

    GraphicsDevice& device;
    PipelineCache& pipelineCache;

    uint32_t maxLights;
    VkExtent2D extent;
    glm::uvec3 gridDims;

    // Buffers
    VkBuffer lightBuffer;
    VmaAllocation lightBufferMemory;
    
    VkBuffer gridBuffer;
    VmaAllocation gridBufferMemory;
    
    VkBuffer indexBuffer;
    VmaAllocation indexBufferMemory;
    
    VkBuffer uboBuffer;
    VmaAllocation uboBufferMemory;
    
    VkBuffer atomicCounterBuffer;
    VmaAllocation atomicCounterBufferMemory;

    // Descriptors
    DescriptorAllocator descriptorAllocator;
    DescriptorLayoutCache layoutCache;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;

    // Pipeline
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
};
