#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "GraphicsDevice.hpp"

#include <vk_mem_alloc.h>

// Resource State Tracking
struct RenderGraphResource {
    std::string name;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    
    // For Transient / Graph-Managed Resources
    bool isTransient = false;
    VmaAllocation allocation = VK_NULL_HANDLE;
    
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags currentAccess = 0;
    VkPipelineStageFlags currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
};

struct RenderPassResourceInfo {
    std::string name;
    VkImageLayout targetLayout;
    VkAccessFlags targetAccess;
    VkPipelineStageFlags targetStage;
};

struct RenderPassNode {
    std::string name;
    std::vector<RenderPassResourceInfo> inputs;
    std::vector<RenderPassResourceInfo> outputs;
    
    std::function<void(VkCommandBuffer, uint32_t)> execute;
    std::function<void(GraphicsDevice&)> setup;
};

class RenderGraph {
public:
    RenderGraph(GraphicsDevice& device);
    ~RenderGraph();
    
    // registration
    void registerImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    void createTransientImage(const std::string& name, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage);
    
    void addPass(RenderPassNode node);
    void compile(); 
    void execute(VkCommandBuffer cmd, uint32_t imageIndex);
    void cleanup();
    
    VkImageView getImageView(const std::string& name);

private:
    void insertBarrier(VkCommandBuffer cmd, RenderGraphResource& resource, const RenderPassResourceInfo& target);

    GraphicsDevice& device;
    std::vector<RenderPassNode> passes;
    std::unordered_map<std::string, RenderGraphResource> resources;
};
