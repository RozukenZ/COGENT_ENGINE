#include "PipelineCache.hpp"
#include <stdexcept>
#include "../Logger.hpp"

void PipelineCache::init(VkDevice newDevice) {
    device = newDevice;

    VkPipelineCacheCreateInfo cacheInfo = {};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(device, &cacheInfo, nullptr, &vulkanPipelineCache) != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan Pipeline Cache!");
    }
}

void PipelineCache::cleanup() {
    for (auto& pair : cachedPipelines) {
        vkDestroyPipeline(device, pair.second, nullptr);
    }
    cachedPipelines.clear();

    if (vulkanPipelineCache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(device, vulkanPipelineCache, nullptr);
        vulkanPipelineCache = VK_NULL_HANDLE;
    }
}

void PipelineCache::defaultPipelineConfigInfo(PipelineConfig& configInfo) {
    configInfo.inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    configInfo.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    configInfo.inputAssembly.primitiveRestartEnable = VK_FALSE;

    configInfo.viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    configInfo.viewportState.viewportCount = 1;
    configInfo.viewportState.pViewports = nullptr;
    configInfo.viewportState.scissorCount = 1;
    configInfo.viewportState.pScissors = nullptr;

    configInfo.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    configInfo.rasterizer.depthClampEnable = VK_FALSE;
    configInfo.rasterizer.rasterizerDiscardEnable = VK_FALSE;
    configInfo.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    configInfo.rasterizer.lineWidth = 1.0f;
    configInfo.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    configInfo.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    configInfo.rasterizer.depthBiasEnable = VK_FALSE;

    configInfo.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    configInfo.multisampling.sampleShadingEnable = VK_FALSE;
    configInfo.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    configInfo.colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    configInfo.colorBlendAttachment.blendEnable = VK_FALSE;

    configInfo.colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    configInfo.colorBlending.logicOpEnable = VK_FALSE;
    configInfo.colorBlending.attachmentCount = 1;
    configInfo.colorBlending.pAttachments = &configInfo.colorBlendAttachment;

    configInfo.depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    configInfo.depthStencil.depthTestEnable = VK_TRUE;
    configInfo.depthStencil.depthWriteEnable = VK_TRUE;
    configInfo.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    configInfo.depthStencil.depthBoundsTestEnable = VK_FALSE;
    configInfo.depthStencil.stencilTestEnable = VK_FALSE;

    configInfo.dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    configInfo.dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    configInfo.dynamicState.pDynamicStates = configInfo.dynamicStateEnables.data();
    configInfo.dynamicState.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
}

VkPipeline PipelineCache::buildGraphicsPipeline(const std::string& name, const PipelineConfig& config) {
    if (cachedPipelines.find(name) != cachedPipelines.end()) {
        return cachedPipelines[name];
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = config.shaderStageCount;
    pipelineInfo.pStages = config.shaderStages;
    pipelineInfo.pVertexInputState = &config.vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &config.inputAssembly;
    pipelineInfo.pViewportState = &config.viewportState;
    pipelineInfo.pRasterizationState = &config.rasterizer;
    pipelineInfo.pMultisampleState = &config.multisampling;
    pipelineInfo.pColorBlendState = &config.colorBlending;
    pipelineInfo.pDepthStencilState = &config.depthStencil;
    pipelineInfo.pDynamicState = &config.dynamicState;
    pipelineInfo.layout = config.pipelineLayout;
    pipelineInfo.renderPass = config.renderPass;
    pipelineInfo.subpass = config.subpass;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, vulkanPipelineCache, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR("Failed to create graphics pipeline: " + name);
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    cachedPipelines[name] = pipeline;
    return pipeline;
}

VkPipeline PipelineCache::buildComputePipeline(const std::string& name, VkPipelineShaderStageCreateInfo computeShaderStage, VkPipelineLayout layout) {
    if (cachedPipelines.find(name) != cachedPipelines.end()) {
        return cachedPipelines[name];
    }

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = computeShaderStage;
    pipelineInfo.layout = layout;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(device, vulkanPipelineCache, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute pipeline: " + name);
        throw std::runtime_error("Failed to create compute pipeline");
    }

    cachedPipelines[name] = pipeline;
    return pipeline;
}

VkPipeline PipelineCache::getPipeline(const std::string& name) {
    if (cachedPipelines.find(name) != cachedPipelines.end()) {
        return cachedPipelines[name];
    }
    return VK_NULL_HANDLE;
}
