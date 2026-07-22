#pragma once
#include <vulkan/vulkan.h>
#include "../Core/Graphics/GraphicsDevice.hpp"

class DepthPrePass {
public:
    DepthPrePass(GraphicsDevice& device, VkExtent2D extent);
    ~DepthPrePass();

    VkRenderPass getRenderPass() const { return renderPass; }
    void execute(VkCommandBuffer cmd);

private:
    void createRenderPass();

    GraphicsDevice& device;
    VkExtent2D extent;
    VkRenderPass renderPass;
};

// ... Implementation ...
// Note: Depth Pre-Pass usually shares the SAME depth buffer as the G-Buffer or Main Pass.
// So we don't own the image, we just define the pass compatible with it.
