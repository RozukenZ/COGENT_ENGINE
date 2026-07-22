#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>

struct PipelineConfig {
    VkPipelineShaderStageCreateInfo* shaderStages = nullptr;
    uint32_t shaderStageCount = 0;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    VkPipelineViewportStateCreateInfo viewportState = {};
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
};

class PipelineCache {
public:
    void init(VkDevice device);
    void cleanup();

    // Setup default pipeline configuration for easy building
    static void defaultPipelineConfigInfo(PipelineConfig& configInfo);
    
    // Builds or retrieves a cached graphics pipeline
    VkPipeline buildGraphicsPipeline(const std::string& name, const PipelineConfig& config);
    
    // Builds or retrieves a cached compute pipeline
    VkPipeline buildComputePipeline(const std::string& name, VkPipelineShaderStageCreateInfo computeShaderStage, VkPipelineLayout layout);
    
    VkPipeline getPipeline(const std::string& name);

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPipelineCache vulkanPipelineCache = VK_NULL_HANDLE;

    std::unordered_map<std::string, VkPipeline> cachedPipelines;
};
