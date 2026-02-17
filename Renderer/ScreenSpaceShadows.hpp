#pragma once
#include <vulkan/vulkan.h>
#include "../Core/Graphics/GraphicsDevice.hpp"

#include <glm/glm.hpp>

class ScreenSpaceShadows {
public:
    ScreenSpaceShadows(GraphicsDevice& device, VkExtent2D extent);
    ~ScreenSpaceShadows();

    void init();
    void execute(VkCommandBuffer cmd, const glm::mat4& view, const glm::mat4& proj, const glm::vec4& lightDir);

    VkImage getOutputImage() const { return image; }
    VkImageView getOutputView() const { return imageView; }
    
    // Links external resources (Depth Buffer, Camera UBO) into internal descriptor set
    void updateDescriptorSets(VkImageView depthView, VkSampler depthSampler, VkBuffer globalUbo);

private:
    void createResources();
    void createPipeline();
    void createDescriptorSetLayout();

    GraphicsDevice& device;
    VkExtent2D extent;

    VkImage image;
    VkDeviceMemory memory;
    VkImageView imageView;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
};
