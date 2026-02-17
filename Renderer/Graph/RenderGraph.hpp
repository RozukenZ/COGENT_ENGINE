#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "GraphicsDevice.hpp"

// Resource State Tracking
struct RenderGraphResource {
    std::string name;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    
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
    
    std::function<void(VkCommandBuffer)> execute;
    std::function<void(GraphicsDevice&)> setup;
};

class RenderGraph {
public:
    RenderGraph(GraphicsDevice& device);
    
    // registration
    void registerImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    
    void addPass(RenderPassNode node);
    void compile(); 
    void execute(VkCommandBuffer cmd);
    
    VkImageView getImageView(const std::string& name);

private:
    void insertBarrier(VkCommandBuffer cmd, RenderGraphResource& resource, const RenderPassResourceInfo& target);

    GraphicsDevice& device;
    std::vector<RenderPassNode> passes;
    std::unordered_map<std::string, RenderGraphResource> resources;
};
