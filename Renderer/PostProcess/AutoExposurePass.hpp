#pragma once
#include <vulkan/vulkan.h>
#include "../../Core/Graphics/GraphicsDevice.hpp"

namespace Cogent::Renderer {

    class AutoExposurePass {
    public:
        AutoExposurePass(GraphicsDevice& device);
        ~AutoExposurePass();

        void execute(VkCommandBuffer cmd, VkImageView inputColor);

        // Helper to get the buffer containing exposure info (for passing to ToneMapper/ANSR)
        VkBuffer getExposureBuffer() const { return exposureBuffer; }

    private:
        void createResources();
        void createPipeline();
        
        GraphicsDevice& device;
        
        VkBuffer exposureBuffer; // Stores Average Luminance / Exposure Value
        VkDeviceMemory exposureMemory;

        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        VkDescriptorSet descriptorSet;
    };
}
