#include "TAAPass.hpp"
#include <stdexcept>
#include <array>
#include "../Core/VulkanUtils.hpp"
#include "../Core/EmbeddedShaders.hpp"

namespace Cogent {
namespace Renderer {

TAAPass::TAAPass(GraphicsDevice& device, PipelineCache& pipelineCache)
    : m_device(device), m_pipelineCache(pipelineCache) {
}

TAAPass::~TAAPass() {
    cleanupResources();
    if (sampler) vkDestroySampler(m_device.getDevice(), sampler, nullptr);
    if (descriptorPool) vkDestroyDescriptorPool(m_device.getDevice(), descriptorPool, nullptr);
    if (descriptorSetLayout) vkDestroyDescriptorSetLayout(m_device.getDevice(), descriptorSetLayout, nullptr);
    if (pipelineLayout) vkDestroyPipelineLayout(m_device.getDevice(), pipelineLayout, nullptr);
}

void TAAPass::init(VkExtent2D extent) {
    m_extent = extent;

    createDescriptorSets();
    createPipelines();
    createResources(extent);
}

void TAAPass::resize(VkExtent2D newExtent) {
    cleanupResources();
    m_extent = newExtent;
    createResources(newExtent);
}

void TAAPass::cleanupResources() {
    auto device = m_device.getDevice();
    for (int i = 0; i < 2; ++i) {
        if (taaOutputViews[i]) vkDestroyImageView(device, taaOutputViews[i], nullptr);
        if (taaImages[i]) vkDestroyImage(device, taaImages[i], nullptr);
        if (taaMemories[i]) vkFreeMemory(device, taaMemories[i], nullptr);
        
        taaOutputViews[i] = VK_NULL_HANDLE;
        taaImages[i] = VK_NULL_HANDLE;
        taaMemories[i] = VK_NULL_HANDLE;
    }
}

void TAAPass::createResources(VkExtent2D extent) {
    auto device = m_device.getDevice();

    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        if (vkCreateImage(device, &imageInfo, nullptr, &taaImages[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create TAA image");
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, taaImages[i], &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &taaMemories[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate TAA image memory");
        }
        vkBindImageMemory(device, taaImages[i], taaMemories[i], 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = taaImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &taaOutputViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create TAA image view");
        }
        
        // Transition to GENERAL layout immediately
        VkCommandBuffer cmd = m_device.beginSingleTimeCommands();
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = taaImages[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        m_device.endSingleTimeCommands(cmd);
    }
}

void TAAPass::createDescriptorSets() {
    auto device = m_device.getDevice();

    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    // Binding 0: Current Color (Sampled)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: History Color (Sampled)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Velocity (Sampled)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Output Image (Storage)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TAA descriptor set layout");
    }

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 6; // 3 sampled images * 2 frames
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 2; // 1 storage image * 2 frames

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 2;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TAA descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(2, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate TAA descriptor sets");
    }

    // Create Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TAA sampler");
    }
}

void TAAPass::createPipelines() {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(m_device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TAA pipeline layout");
    }

    auto compCode = EmbeddedShaders::GetShader("taa.comp.spv");
    VkShaderModule compModule = VulkanUtils::createShaderModule(m_device.getDevice(), compCode);
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = compModule;
    stageInfo.pName = "main";
    computePipeline = m_pipelineCache.buildComputePipeline("TAA", stageInfo, pipelineLayout);
    vkDestroyShaderModule(m_device.getDevice(), compModule, nullptr);
}

void TAAPass::updateDescriptorSets(VkImageView currentColorView, VkImageView velocityView, VkImageView depthView) {
    auto device = m_device.getDevice();

    for (int i = 0; i < 2; ++i) {
        uint32_t historyIdx = (i + 1) % 2; // For output i, history is historyIdx

        VkDescriptorImageInfo currentColorInfo{};
        currentColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        currentColorInfo.imageView = currentColorView;
        currentColorInfo.sampler = sampler;

        VkDescriptorImageInfo historyColorInfo{};
        historyColorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // Assuming we keep history in GENERAL layout
        historyColorInfo.imageView = taaOutputViews[historyIdx];
        historyColorInfo.sampler = sampler;

        VkDescriptorImageInfo velocityInfo{};
        velocityInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        velocityInfo.imageView = velocityView;
        velocityInfo.sampler = sampler;

        VkDescriptorImageInfo outputInfo{};
        outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputInfo.imageView = taaOutputViews[i];
        outputInfo.sampler = VK_NULL_HANDLE;

        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &currentColorInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &historyColorInfo;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &velocityInfo;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &outputInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void TAAPass::dispatch(VkCommandBuffer cmd, uint32_t width, uint32_t height) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSets[currentFrameIdx], 0, nullptr);

    vkCmdDispatch(cmd, (width + 15) / 16, (height + 15) / 16, 1);
    
    // Switch ping pong
    currentFrameIdx = (currentFrameIdx + 1) % 2;
}

} // namespace Renderer
} // namespace Cogent
