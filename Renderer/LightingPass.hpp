#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <iostream>
#include "../Core/VulkanUtils.hpp"

class LightingPass {
public:
    void init(VkDevice device, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout, VkExtent2D extent);
    void cleanup(VkDevice device);
    
    VkPipeline getPipeline() { return pipeline; }
    VkPipelineLayout getPipelineLayout() { return pipelineLayout; }

private:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    static std::vector<char> readFile(const std::string& filename);
};