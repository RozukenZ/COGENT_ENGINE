#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "../Resources/Model.hpp" // Untuk akses Vertex Structure

class RenderPipeline {
public:
    void init(VkDevice device, VkRenderPass renderPass, VkExtent2D extent, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
    void cleanup(VkDevice device);

    VkPipeline getPipeline() { return pipeline; }
    VkPipelineLayout getPipelineLayout() { return pipelineLayout; }

private:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    static std::vector<char> readFile(const std::string& filename);
};