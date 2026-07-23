#define NOMINMAX
#include "HDRPipeline.hpp"
#include "../Core/Logger.hpp"
#include <cmath>
#include <stdexcept>
#include <array>
#include <algorithm>

namespace Cogent {
namespace Renderer {

HDRPipeline::HDRPipeline(GraphicsDevice& device, Graphics::ShaderSystem& shaderSystem, PipelineCache& pipelineCache)
    : m_device(device), m_shaderSystem(shaderSystem), m_pipelineCache(pipelineCache) {}

HDRPipeline::~HDRPipeline() {
    cleanupResources();
}

void HDRPipeline::cleanupResources() {
    if (hdrRenderPass) vkDestroyRenderPass(m_device.getDevice(), hdrRenderPass, nullptr);
    if (hdrFramebuffer) vkDestroyFramebuffer(m_device.getDevice(), hdrFramebuffer, nullptr);
    if (hdrView) vkDestroyImageView(m_device.getDevice(), hdrView, nullptr);
    if (hdrImage) vmaDestroyImage(m_device.getAllocator(), hdrImage, hdrAlloc);

    if (tonemapRenderPass) vkDestroyRenderPass(m_device.getDevice(), tonemapRenderPass, nullptr);
    if (tonemapFramebuffer) vkDestroyFramebuffer(m_device.getDevice(), tonemapFramebuffer, nullptr);
    if (tonemappedView) vkDestroyImageView(m_device.getDevice(), tonemappedView, nullptr);
    if (tonemappedImage) vmaDestroyImage(m_device.getAllocator(), tonemappedImage, tonemappedAlloc);


    for (auto view : bloomMipViews) {
        if (view) vkDestroyImageView(m_device.getDevice(), view, nullptr);
    }
    bloomMipViews.clear();
    
    if (bloomView) vkDestroyImageView(m_device.getDevice(), bloomView, nullptr);
    if (bloomImage) vmaDestroyImage(m_device.getAllocator(), bloomImage, bloomAlloc);

    if (linearSampler) vkDestroySampler(m_device.getDevice(), linearSampler, nullptr);

    if (descriptorPool) vkDestroyDescriptorPool(m_device.getDevice(), descriptorPool, nullptr);
    if (bloomDescriptorLayout) vkDestroyDescriptorSetLayout(m_device.getDevice(), bloomDescriptorLayout, nullptr);
    if (tonemapDescriptorLayout) vkDestroyDescriptorSetLayout(m_device.getDevice(), tonemapDescriptorLayout, nullptr);
    if (bloomPipelineLayout) vkDestroyPipelineLayout(m_device.getDevice(), bloomPipelineLayout, nullptr);
    if (tonemapPipelineLayout) vkDestroyPipelineLayout(m_device.getDevice(), tonemapPipelineLayout, nullptr);
    
    // Pipelines are managed by PipelineCache
}

void HDRPipeline::init(VkExtent2D extent, VkRenderPass swapchainRenderPass) {
    m_extent = extent;
    createHDRTarget(extent);
    createBloomResources(extent);
    createTonemapTarget(extent);
    createPipelines();
    createDescriptorSets();
}

void HDRPipeline::resize(VkExtent2D newExtent) {
    cleanupResources();
    // Re-create resources (pipeline caches will reuse pipelines)
}

void HDRPipeline::createHDRTarget(VkExtent2D extent) {
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    // Image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(m_device.getAllocator(), &imageInfo, &allocInfo, &hdrImage, &hdrAlloc, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR image");
    }

    // View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = hdrImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &hdrView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR image view");
    }

    // Render Pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device.getDevice(), &renderPassInfo, nullptr, &hdrRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR Render Pass");
    }

    // Framebuffer
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = hdrRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &hdrView;
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_device.getDevice(), &framebufferInfo, nullptr, &hdrFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR Framebuffer");
    }
}

void HDRPipeline::createBloomResources(VkExtent2D extent) {
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    bloomMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) - 1; // leave 1 pixel at smallest
    if (bloomMipLevels < 2) bloomMipLevels = 2; // minimum

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.width / 2, extent.height / 2, 1}; // Half res
    imageInfo.mipLevels = bloomMipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(m_device.getAllocator(), &imageInfo, &allocInfo, &bloomImage, &bloomAlloc, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Bloom image");
    }

    // Full View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = bloomImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = bloomMipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &bloomView);

    // Mip Views
    bloomMipViews.resize(bloomMipLevels);
    for (uint32_t i = 0; i < bloomMipLevels; i++) {
        viewInfo.subresourceRange.baseMipLevel = i;
        viewInfo.subresourceRange.levelCount = 1;
        vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &bloomMipViews[i]);
    }
    
    // Linear Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    vkCreateSampler(m_device.getDevice(), &samplerInfo, nullptr, &linearSampler);
}

void HDRPipeline::createTonemapTarget(VkExtent2D extent) {
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    // Image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(m_device.getAllocator(), &imageInfo, &allocInfo, &tonemappedImage, &tonemappedAlloc, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Tonemapped image");
    }

    // View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tonemappedImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &tonemappedView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Tonemapped image view");
    }

    // Render Pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependencies[2];
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies;

    if (vkCreateRenderPass(m_device.getDevice(), &renderPassInfo, nullptr, &tonemapRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Tonemap Render Pass");
    }

    // Framebuffer
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = tonemapRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &tonemappedView;
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_device.getDevice(), &framebufferInfo, nullptr, &tonemapFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Tonemap Framebuffer");
    }
}

void HDRPipeline::createPipelines() {
    // 1. Bloom Compute Layout
    VkDescriptorSetLayoutBinding inputImageBinding{};
    inputImageBinding.binding = 0;
    inputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    inputImageBinding.descriptorCount = 1;
    inputImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding outputImageBinding{};
    outputImageBinding.binding = 1;
    outputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageBinding.descriptorCount = 1;
    outputImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bloomBindings = {inputImageBinding, outputImageBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bloomBindings.size());
    layoutInfo.pBindings = bloomBindings.data();

    vkCreateDescriptorSetLayout(m_device.getDevice(), &layoutInfo, nullptr, &bloomDescriptorLayout);

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(float) * 4; // inverse resolution, threshold, etc

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &bloomDescriptorLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pcRange;
    vkCreatePipelineLayout(m_device.getDevice(), &pipelineLayoutInfo, nullptr, &bloomPipelineLayout);

    // Bloom Pipelines
    VkPipelineShaderStageCreateInfo downStageInfo{};
    downStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    downStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    downStageInfo.module = m_shaderSystem.getShaderModule("Shaders/bloom_downsample.comp");
    downStageInfo.pName = "main";

    // Bloom Pipelines using PipelineCache
    bloomDownsamplePipeline = m_pipelineCache.buildComputePipeline("BloomDownsample", downStageInfo, bloomPipelineLayout);

    VkPipelineShaderStageCreateInfo upStageInfo = downStageInfo;
    upStageInfo.module = m_shaderSystem.getShaderModule("Shaders/bloom_upsample.comp");
    bloomUpsamplePipeline = m_pipelineCache.buildComputePipeline("BloomUpsample", upStageInfo, bloomPipelineLayout);

    // 2. Tonemap Graphics Layout
    VkDescriptorSetLayoutBinding hdrTexBinding{};
    hdrTexBinding.binding = 0;
    hdrTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    hdrTexBinding.descriptorCount = 1;
    hdrTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bloomTexBinding{};
    bloomTexBinding.binding = 1;
    bloomTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bloomTexBinding.descriptorCount = 1;
    bloomTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> tonemapBindings = {hdrTexBinding, bloomTexBinding};
    VkDescriptorSetLayoutCreateInfo toneLayoutInfo{};
    toneLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    toneLayoutInfo.bindingCount = static_cast<uint32_t>(tonemapBindings.size());
    toneLayoutInfo.pBindings = tonemapBindings.data();
    vkCreateDescriptorSetLayout(m_device.getDevice(), &toneLayoutInfo, nullptr, &tonemapDescriptorLayout);

    VkPipelineLayoutCreateInfo tonePipelineLayoutInfo{};
    tonePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    tonePipelineLayoutInfo.setLayoutCount = 1;
    tonePipelineLayoutInfo.pSetLayouts = &tonemapDescriptorLayout;
    
    // exposure push constant
    VkPushConstantRange tonePcRange{};
    tonePcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    tonePcRange.offset = 0;
    tonePcRange.size = sizeof(float) * 2; // exposure, gamma
    tonePipelineLayoutInfo.pushConstantRangeCount = 1;
    tonePipelineLayoutInfo.pPushConstantRanges = &tonePcRange;

    vkCreatePipelineLayout(m_device.getDevice(), &tonePipelineLayoutInfo, nullptr, &tonemapPipelineLayout);

    // Pipeline using PipelineCache
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = m_shaderSystem.getShaderModule("Shaders/fullscreen.vert");
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = m_shaderSystem.getShaderModule("Shaders/tonemap.frag");
    fragShaderStageInfo.pName = "main";

    PipelineConfig config;
    PipelineCache::defaultPipelineConfigInfo(config);
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    config.shaderStages = shaderStages;
    config.shaderStageCount = 2;
    
    config.viewportState.viewportCount = 1;
    config.viewportState.pViewports = nullptr; // dynamic
    config.viewportState.scissorCount = 1;
    config.viewportState.pScissors = nullptr; // dynamic
    
    config.rasterizer.cullMode = VK_CULL_MODE_NONE;
    
    // [FIX 3] Disable depth test for fullscreen tonemap (no depth attachment)
    config.depthStencil.depthTestEnable = VK_FALSE;
    config.depthStencil.depthWriteEnable = VK_FALSE;
    
    config.dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    config.dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    config.dynamicState.dynamicStateCount = static_cast<uint32_t>(config.dynamicStateEnables.size());
    config.dynamicState.pDynamicStates = config.dynamicStateEnables.data();

    config.pipelineLayout = tonemapPipelineLayout;
    config.renderPass = tonemapRenderPass;
    
    tonemapPipeline = m_pipelineCache.buildGraphicsPipeline("TonemapPipeline", config);
}

void HDRPipeline::createDescriptorSets() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100}
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 100;
    vkCreateDescriptorPool(m_device.getDevice(), &poolInfo, nullptr, &descriptorPool);

    // Allocate Tonemap
    VkDescriptorSetAllocateInfo toneAllocInfo{};
    toneAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    toneAllocInfo.descriptorPool = descriptorPool;
    toneAllocInfo.descriptorSetCount = 1;
    toneAllocInfo.pSetLayouts = &tonemapDescriptorLayout;
    vkAllocateDescriptorSets(m_device.getDevice(), &toneAllocInfo, &tonemapDescriptorSet);

    // Update Tonemap
    VkDescriptorImageInfo hdrImageInfo{};
    hdrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrImageInfo.imageView = hdrView;
    hdrImageInfo.sampler = linearSampler;

    VkDescriptorImageInfo bloomImageInfo{};
    bloomImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bloomImageInfo.imageView = bloomView;
    bloomImageInfo.sampler = linearSampler;

    std::array<VkWriteDescriptorSet, 2> toneWrites{};
    toneWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    toneWrites[0].dstSet = tonemapDescriptorSet;
    toneWrites[0].dstBinding = 0;
    toneWrites[0].dstArrayElement = 0;
    toneWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    toneWrites[0].descriptorCount = 1;
    toneWrites[0].pImageInfo = &hdrImageInfo;

    toneWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    toneWrites[1].dstSet = tonemapDescriptorSet;
    toneWrites[1].dstBinding = 1;
    toneWrites[1].dstArrayElement = 0;
    toneWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    toneWrites[1].descriptorCount = 1;
    toneWrites[1].pImageInfo = &bloomImageInfo;
    vkUpdateDescriptorSets(m_device.getDevice(), static_cast<uint32_t>(toneWrites.size()), toneWrites.data(), 0, nullptr);

    // Allocate Bloom sets
    // Downsample passes: 
    // Pass 0: read HDR_Color, write bloom_mip0
    // Pass i: read bloom_mip(i-1), write bloom_mip(i)
    bloomDownsampleSets.resize(bloomMipLevels);
    for(uint32_t i=0; i<bloomMipLevels; i++) {
        VkDescriptorSetAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = descriptorPool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &bloomDescriptorLayout;
        vkAllocateDescriptorSets(m_device.getDevice(), &alloc, &bloomDownsampleSets[i]);

        VkDescriptorImageInfo inInfo{};
        inInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // or GENERAL
        inInfo.imageView = (i == 0) ? hdrView : bloomMipViews[i-1];
        inInfo.sampler = linearSampler;

        VkDescriptorImageInfo outInfo{};
        outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outInfo.imageView = bloomMipViews[i];

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = bloomDownsampleSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &inInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = bloomDownsampleSets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &outInfo;
        vkUpdateDescriptorSets(m_device.getDevice(), 2, writes.data(), 0, nullptr);
    }

    // Upsample passes:
    // Pass i: read bloom_mip(i), write bloom_mip(i-1) (with additive blending in compute)
    bloomUpsampleSets.resize(bloomMipLevels - 1);
    for(int i = bloomMipLevels - 1; i > 0; i--) {
        int targetMip = i - 1;
        VkDescriptorSetAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = descriptorPool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &bloomDescriptorLayout;
        vkAllocateDescriptorSets(m_device.getDevice(), &alloc, &bloomUpsampleSets[targetMip]);

        VkDescriptorImageInfo inInfo{};
        inInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        inInfo.imageView = bloomMipViews[i];
        inInfo.sampler = linearSampler;

        VkDescriptorImageInfo outInfo{};
        outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outInfo.imageView = bloomMipViews[targetMip];

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = bloomUpsampleSets[targetMip];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &inInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = bloomUpsampleSets[targetMip];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &outInfo;
        vkUpdateDescriptorSets(m_device.getDevice(), 2, writes.data(), 0, nullptr);
    }
}

void HDRPipeline::executeBloom(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomDownsamplePipeline);
    
    // Helper lambda for barrier
    auto imageBarrier = [](VkCommandBuffer cb, VkImage image, uint32_t mipLevel, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = oldLayout;
        b.newLayout = newLayout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.baseMipLevel = mipLevel;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.baseArrayLayer = 0;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = srcAccess;
        b.dstAccessMask = dstAccess;
        vkCmdPipelineBarrier(cb, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    };

    // Transition all bloom mips to GENERAL
    for(uint32_t i=0; i<bloomMipLevels; i++) {
        imageBarrier(cmd, bloomImage, i, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    uint32_t mipWidth = m_extent.width / 2;
    uint32_t mipHeight = m_extent.height / 2;

    for (uint32_t i = 0; i < bloomMipLevels; i++) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomPipelineLayout, 0, 1, &bloomDownsampleSets[i], 0, nullptr);
        
        float invRes[4] = {1.0f / mipWidth, 1.0f / mipHeight, 0.0f, 0.0f}; // First float2 is inv resolution, maybe use [2] for threshold on first pass
        if (i == 0) invRes[2] = 1.0f; // threshold for pass 0
        vkCmdPushConstants(cmd, bloomPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)*4, invRes);
        
        vkCmdDispatch(cmd, (mipWidth + 7)/8, (mipHeight + 7)/8, 1);

        // Transition current mip to READ_ONLY for next pass
        imageBarrier(cmd, bloomImage, i, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        mipWidth = std::max(1u, mipWidth / 2);
        mipHeight = std::max(1u, mipHeight / 2);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomUpsamplePipeline);

    for (int i = bloomMipLevels - 1; i > 0; i--) {
        int targetMip = i - 1;
        mipWidth = std::max(1u, (m_extent.width / 2) >> targetMip);
        mipHeight = std::max(1u, (m_extent.height / 2) >> targetMip);

        // Transition target to GENERAL for writing (it was READ_ONLY from downsample)
        imageBarrier(cmd, bloomImage, targetMip, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomPipelineLayout, 0, 1, &bloomUpsampleSets[targetMip], 0, nullptr);
        
        float pc[4] = {0.005f, 0.0f, 0.0f, 0.0f}; // filter radius
        vkCmdPushConstants(cmd, bloomPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)*4, pc);
        
        vkCmdDispatch(cmd, (mipWidth + 7)/8, (mipHeight + 7)/8, 1);

        // Transition target back to READ_ONLY
        imageBarrier(cmd, bloomImage, targetMip, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
}

void HDRPipeline::executeTonemap(VkCommandBuffer cmd, VkExtent2D extent) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = tonemapRenderPass;
    renderPassInfo.framebuffer = tonemapFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    // [FIX 5] Magenta clear for debugging — if viewport is magenta, tonemap pass runs but shader doesn't write
    VkClearValue clearColor = {};
    clearColor.color = {{1.0f, 0.0f, 1.0f, 1.0f}}; 
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewportFullscreen{};
    viewportFullscreen.width = (float)extent.width;
    viewportFullscreen.height = (float)extent.height;
    viewportFullscreen.minDepth = 0.0f;
    viewportFullscreen.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewportFullscreen);

    VkRect2D scissorFullscreen{};
    scissorFullscreen.offset = {0, 0};
    scissorFullscreen.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissorFullscreen);
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipelineLayout, 0, 1, &tonemapDescriptorSet, 0, nullptr);
    
    float exposure[2] = {1.0f, 2.2f};
    vkCmdPushConstants(cmd, tonemapPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*2, exposure);
    
    vkCmdDraw(cmd, 3, 1, 0, 0); // Triangle for fullscreen quad
    
    vkCmdEndRenderPass(cmd);
}

} // namespace Renderer
} // namespace Cogent
