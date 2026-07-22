#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "../Core/Graphics/GraphicsDevice.hpp"
#include "../Core/Graphics/PipelineCache.hpp"

namespace Cogent {
namespace Renderer {

class TAAPass {
public:
    TAAPass(GraphicsDevice& device, PipelineCache& pipelineCache);
    ~TAAPass();

    void init(VkExtent2D extent);
    void resize(VkExtent2D newExtent);

    // Get the final output for the current frame
    VkImageView getOutputView() const { return taaOutputViews[currentFrameIdx]; }
    VkImage getOutputImage() const { return taaImages[currentFrameIdx]; }
    
    // Updates descriptor sets with inputs
    void updateDescriptorSets(VkImageView currentColorView, VkImageView velocityView, VkImageView depthView);
    
    // Dispatches the TAA compute shader
    void dispatch(VkCommandBuffer cmd, uint32_t width, uint32_t height);

private:
    void createResources(VkExtent2D extent);
    void cleanupResources();
    void createPipelines();
    void createDescriptorSets();

    GraphicsDevice& m_device;
    PipelineCache& m_pipelineCache;

    VkExtent2D m_extent;

    // Ping-pong history buffers
    VkImage taaImages[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkDeviceMemory taaMemories[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkImageView taaOutputViews[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

    VkSampler sampler;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSets[2]; // Ping-pong descriptor sets

    VkPipelineLayout pipelineLayout;
    VkPipeline computePipeline;

    uint32_t currentFrameIdx = 0;
};

} // namespace Renderer
} // namespace Cogent
