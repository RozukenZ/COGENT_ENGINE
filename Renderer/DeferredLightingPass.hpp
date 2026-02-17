#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "../Core/Graphics/GraphicsDevice.hpp"
#include "../GBuffer.hpp"

class DeferredLightingPass {
public:
    DeferredLightingPass(GraphicsDevice& device, VkRenderPass renderPass, VkExtent2D extent);
    ~DeferredLightingPass();

    // Sets up the pipeline and descriptors
    void init();
    
    // Updates descriptors with G-Buffer views
    void updateDescriptorSets(const GBuffer& gbuffer);

    void execute(VkCommandBuffer cmd, VkDescriptorSet sceneGlobalDescSet);

private:
    void createDescriptorSetLayout();
    void createPipeline(VkRenderPass renderPass);
    void createDescriptorPool();

    GraphicsDevice& device;
    VkExtent2D extent;
    VkRenderPass renderPass; // The RenderPass this pipeline runs inside (usually Swapchain pass or separate Color pass)

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkDescriptorSetLayout descriptorSetLayout; // G-Buffer Binding
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
};
