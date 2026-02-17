#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include "../../Core/Graphics/GraphicsDevice.hpp"
#include "../../Core/Graphics/Swapchain.hpp"

namespace Cogent::Renderer {

    class ANSRPass {
    public:
        ANSRPass(GraphicsDevice& device, VkExtent2D renderExtent, VkExtent2D displayExtent);
        ~ANSRPass();

        // Called when window resizes or render scale changes
        void resize(VkExtent2D renderExtent, VkExtent2D displayExtent);

        // Record the Upscaling Dispatch
        void execute(VkCommandBuffer cmd, VkImageView inputColor, VkImageView inputMotion, VkImageView inputDepth);

        VkImage getOutputImage() const { return displayImage; }
        VkImageView getOutputView() const { return displayView; }

    private:
        void createResources();
        void createPipeline();
        void createDescriptorSetLayout();
        void updateDescriptorSets(VkImageView inputColor, VkImageView inputMotion, VkImageView inputDepth);

        GraphicsDevice& device;
        VkExtent2D renderExtent;  // Low Res
        VkExtent2D displayExtent; // High Res (Target)

        // Resources
        VkImage displayImage; // Final Output
        VkDeviceMemory displayMemory;
        VkImageView displayView;

        // History Buffer (Previous Frame High Res)
        VkImage historyImage;
        VkDeviceMemory historyMemory;
        VkImageView historyView;

        // Pipeline
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        VkDescriptorSet descriptorSet;

        bool firstFrame = true;
    };
}
