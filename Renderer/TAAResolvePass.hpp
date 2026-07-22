#pragma once
#include <vulkan/vulkan.h>
#include "../Core/Graphics/GraphicsDevice.hpp"

class TAAResolvePass {
public:
    TAAResolvePass(GraphicsDevice& device, VkExtent2D extent);
    ~TAAResolvePass();

    void execute(VkCommandBuffer cmd, VkDescriptorSet inputHistory, VkDescriptorSet inputCurrent);

private:
    GraphicsDevice& device;
    VkExtent2D extent;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    // History Buffer management would be here
};
