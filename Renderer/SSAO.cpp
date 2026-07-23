#include "SSAO.hpp"
#include <stdexcept>
#include <array>
#include <random>
#include "../Core/VulkanUtils.hpp"
#include "../Core/EmbeddedShaders.hpp"
#include "../Core/VulkanUtils.hpp"

namespace Cogent {
namespace Renderer {

float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

SSAO::SSAO(GraphicsDevice& device, PipelineCache& pipelineCache, VkExtent2D extent) 
    : m_device(device), m_pipelineCache(pipelineCache), m_extent(extent) {
    init();
}

SSAO::~SSAO() {
    cleanupResources();
}

void SSAO::cleanupResources() {
    auto device = m_device.getDevice();

    if (ssaoRawView) vkDestroyImageView(device, ssaoRawView, nullptr);
    if (ssaoRawImage) vkDestroyImage(device, ssaoRawImage, nullptr);
    if (ssaoRawMemory) vkFreeMemory(device, ssaoRawMemory, nullptr);

    if (ssaoBlurView) vkDestroyImageView(device, ssaoBlurView, nullptr);
    if (ssaoBlurImage) vkDestroyImage(device, ssaoBlurImage, nullptr);
    if (ssaoBlurMemory) vkFreeMemory(device, ssaoBlurMemory, nullptr);

    if (noiseView) vkDestroyImageView(device, noiseView, nullptr);
    if (noiseImage) vkDestroyImage(device, noiseImage, nullptr);
    if (noiseMemory) vkFreeMemory(device, noiseMemory, nullptr);

    if (kernelBuffer) vkDestroyBuffer(device, kernelBuffer, nullptr);
    if (kernelBufferMemory) vkFreeMemory(device, kernelBufferMemory, nullptr);

    if (noiseSampler) vkDestroySampler(device, noiseSampler, nullptr);
    if (defaultSampler) vkDestroySampler(device, defaultSampler, nullptr);

    if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    if (ssaoDescriptorLayout) vkDestroyDescriptorSetLayout(device, ssaoDescriptorLayout, nullptr);
    if (blurDescriptorLayout) vkDestroyDescriptorSetLayout(device, blurDescriptorLayout, nullptr);

    if (ssaoPipelineLayout) vkDestroyPipelineLayout(device, ssaoPipelineLayout, nullptr);
    if (blurPipelineLayout) vkDestroyPipelineLayout(device, blurPipelineLayout, nullptr);
}

void SSAO::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device.getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device.getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    
    // Find memory type
    uint32_t typeIndex = -1;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_device.getPhysicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            typeIndex = i;
            break;
        }
    }
    if (typeIndex == -1) throw std::runtime_error("Failed to find suitable memory type!");
    
    allocInfo.memoryTypeIndex = typeIndex;

    if (vkAllocateMemory(m_device.getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(m_device.getDevice(), buffer, bufferMemory, 0);
}

void SSAO::generateKernel() {
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;

    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0f - 1.0f, 
            randomFloats(generator) * 2.0f - 1.0f, 
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        
        float scale = (float)i / 64.0f;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        
        ssaoKernel.push_back(glm::vec4(sample, 0.0f));
    }
}

void SSAO::createNoiseTexture() {
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    std::vector<glm::vec4> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0f - 1.0f, 
            randomFloats(generator) * 2.0f - 1.0f, 
            0.0f); 
        ssaoNoise.push_back(glm::vec4(noise, 0.0f));
    }

    VkDeviceSize imageSize = 16 * sizeof(glm::vec4);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device.getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, ssaoNoise.data(), (size_t)imageSize);
    vkUnmapMemory(m_device.getDevice(), stagingBufferMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = 4;
    imageInfo.extent.height = 4;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device.getDevice(), &imageInfo, nullptr, &noiseImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSAO Noise image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device.getDevice(), noiseImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    
    // Find memory type
    uint32_t typeIndex = -1;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_device.getPhysicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            typeIndex = i;
            break;
        }
    }
    allocInfo.memoryTypeIndex = typeIndex;

    if (vkAllocateMemory(m_device.getDevice(), &allocInfo, nullptr, &noiseMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }
    vkBindImageMemory(m_device.getDevice(), noiseImage, noiseMemory, 0);

    // Transition and copy
    VkCommandBuffer cmd = m_device.beginSingleTimeCommands();
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = noiseImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {4, 4, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, noiseImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    m_device.endSingleTimeCommands(cmd);

    vkDestroyBuffer(m_device.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_device.getDevice(), stagingBufferMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = noiseImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &noiseView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view!");
    }
}

void SSAO::createResources() {
    // 1. Generate Kernel and Noise
    generateKernel();
    createNoiseTexture();

    // 2. Create SSAO Raw and Blur Images
    VkFormat format = VK_FORMAT_R8_UNORM;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_extent.width;
    imageInfo.extent.height = m_extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // SSAO Raw
    vkCreateImage(m_device.getDevice(), &imageInfo, nullptr, &ssaoRawImage);
    // Allocate memory...
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device.getDevice(), ssaoRawImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    uint32_t typeIndex = -1;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_device.getPhysicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            typeIndex = i;
            break;
        }
    }
    allocInfo.memoryTypeIndex = typeIndex;
    vkAllocateMemory(m_device.getDevice(), &allocInfo, nullptr, &ssaoRawMemory);
    vkBindImageMemory(m_device.getDevice(), ssaoRawImage, ssaoRawMemory, 0);

    // SSAO Blur
    vkCreateImage(m_device.getDevice(), &imageInfo, nullptr, &ssaoBlurImage);
    vkAllocateMemory(m_device.getDevice(), &allocInfo, nullptr, &ssaoBlurMemory);
    vkBindImageMemory(m_device.getDevice(), ssaoBlurImage, ssaoBlurMemory, 0);

    // Image Views
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = ssaoRawImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &ssaoRawView);

    viewInfo.image = ssaoBlurImage;
    vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &ssaoBlurView);

    // Samplers
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(m_device.getDevice(), &samplerInfo, nullptr, &noiseSampler);

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(m_device.getDevice(), &samplerInfo, nullptr, &defaultSampler);

    // 3. Kernel Uniform Buffer
    VkDeviceSize bufferSize = sizeof(glm::vec4) * 64;
    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, kernelBuffer, kernelBufferMemory);

    void* data;
    vkMapMemory(m_device.getDevice(), kernelBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, ssaoKernel.data(), (size_t)bufferSize);
    vkUnmapMemory(m_device.getDevice(), kernelBufferMemory);
}

void SSAO::createPipelines() {
    // Pipeline Layouts and Descriptor Set Layouts
    
    // SSAO Descriptor Set Layout
    std::array<VkDescriptorSetLayoutBinding, 6> ssaoBindings{};
    ssaoBindings[0].binding = 0; // Position
    ssaoBindings[0].descriptorCount = 1;
    ssaoBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ssaoBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    ssaoBindings[1].binding = 1; // Normal
    ssaoBindings[1].descriptorCount = 1;
    ssaoBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ssaoBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    ssaoBindings[2].binding = 2; // Noise
    ssaoBindings[2].descriptorCount = 1;
    ssaoBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ssaoBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    ssaoBindings[3].binding = 3; // Kernel UBO
    ssaoBindings[3].descriptorCount = 1;
    ssaoBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ssaoBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    ssaoBindings[4].binding = 4; // Output Image
    ssaoBindings[4].descriptorCount = 1;
    ssaoBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    ssaoBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    ssaoBindings[5].binding = 5; // Global UBO
    ssaoBindings[5].descriptorCount = 1;
    ssaoBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ssaoBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo ssaoLayoutInfo{};
    ssaoLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ssaoLayoutInfo.bindingCount = static_cast<uint32_t>(ssaoBindings.size());
    ssaoLayoutInfo.pBindings = ssaoBindings.data();
    vkCreateDescriptorSetLayout(m_device.getDevice(), &ssaoLayoutInfo, nullptr, &ssaoDescriptorLayout);

    // Blur Descriptor Set Layout
    std::array<VkDescriptorSetLayoutBinding, 2> blurBindings{};
    blurBindings[0].binding = 0; // SSAO Raw Image
    blurBindings[0].descriptorCount = 1;
    blurBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    blurBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    blurBindings[1].binding = 1; // Output Blurred Image
    blurBindings[1].descriptorCount = 1;
    blurBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    blurBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo blurLayoutInfo{};
    blurLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    blurLayoutInfo.bindingCount = static_cast<uint32_t>(blurBindings.size());
    blurLayoutInfo.pBindings = blurBindings.data();
    vkCreateDescriptorSetLayout(m_device.getDevice(), &blurLayoutInfo, nullptr, &blurDescriptorLayout);
}

void SSAO::createDescriptorSets() {
    // Descriptor Pool
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 5;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 2; // Kernel + Global UBO
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[2].descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 2;
    vkCreateDescriptorPool(m_device.getDevice(), &poolInfo, nullptr, &descriptorPool);

    // Allocate Sets
    std::array<VkDescriptorSetLayout, 2> layouts = {ssaoDescriptorLayout, blurDescriptorLayout};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts.data();

    std::array<VkDescriptorSet, 2> sets;
    vkAllocateDescriptorSets(m_device.getDevice(), &allocInfo, sets.data());
    ssaoDescriptorSet = sets[0];
    blurDescriptorSet = sets[1];
}

void SSAO::init() {
    createResources();
    createPipelines();
    createDescriptorSets();
    
    // Create actual compute pipelines using PipelineCache
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &ssaoDescriptorLayout;
    
    // Push constant for screen dimensions
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::vec2);
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pcRange;

    vkCreatePipelineLayout(m_device.getDevice(), &pipelineLayoutInfo, nullptr, &ssaoPipelineLayout);

    pipelineLayoutInfo.pSetLayouts = &blurDescriptorLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    vkCreatePipelineLayout(m_device.getDevice(), &pipelineLayoutInfo, nullptr, &blurPipelineLayout);

    // Create shader modules and pipeline for SSAO
    auto ssaoCode = EmbeddedShaders::GetShader("ssao.comp.spv");
    VkShaderModule ssaoModule = VulkanUtils::createShaderModule(m_device.getDevice(), ssaoCode);
    VkPipelineShaderStageCreateInfo ssaoStageInfo{};
    ssaoStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ssaoStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ssaoStageInfo.module = ssaoModule;
    ssaoStageInfo.pName = "main";
    ssaoPipeline = m_pipelineCache.buildComputePipeline("SSAO", ssaoStageInfo, ssaoPipelineLayout);
    vkDestroyShaderModule(m_device.getDevice(), ssaoModule, nullptr);

    // Create shader modules and pipeline for Blur
    auto blurCode = EmbeddedShaders::GetShader("ssao_blur.comp.spv");
    VkShaderModule blurModule = VulkanUtils::createShaderModule(m_device.getDevice(), blurCode);
    VkPipelineShaderStageCreateInfo blurStageInfo{};
    blurStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    blurStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    blurStageInfo.module = blurModule;
    blurStageInfo.pName = "main";
    blurPipeline = m_pipelineCache.buildComputePipeline("SSAO_Blur", blurStageInfo, blurPipelineLayout);
    vkDestroyShaderModule(m_device.getDevice(), blurModule, nullptr);
}

void SSAO::updateDescriptorSets(VkImageView positionView, VkImageView normalView, VkBuffer globalUbo) {
    // Update SSAO descriptor set
    VkDescriptorImageInfo posInfo{};
    posInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    posInfo.imageView = positionView;
    posInfo.sampler = defaultSampler;

    VkDescriptorImageInfo normInfo{};
    normInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    normInfo.imageView = normalView;
    normInfo.sampler = defaultSampler;

    VkDescriptorImageInfo noiseInfo{};
    noiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    noiseInfo.imageView = noiseView;
    noiseInfo.sampler = noiseSampler;

    VkDescriptorBufferInfo kernelInfo{};
    kernelInfo.buffer = kernelBuffer;
    kernelInfo.offset = 0;
    kernelInfo.range = sizeof(glm::vec4) * 64;

    VkDescriptorImageInfo ssaoRawImageInfo{};
    ssaoRawImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    ssaoRawImageInfo.imageView = ssaoRawView;

    VkDescriptorBufferInfo globalUboInfo{};
    globalUboInfo.buffer = globalUbo;
    globalUboInfo.offset = 0;
    globalUboInfo.range = VK_WHOLE_SIZE;

    std::array<VkWriteDescriptorSet, 6> ssaoWrites{};

    ssaoWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssaoWrites[0].dstSet = ssaoDescriptorSet;
    ssaoWrites[0].dstBinding = 0;
    ssaoWrites[0].dstArrayElement = 0;
    ssaoWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ssaoWrites[0].descriptorCount = 1;
    ssaoWrites[0].pImageInfo = &posInfo;

    ssaoWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssaoWrites[1].dstSet = ssaoDescriptorSet;
    ssaoWrites[1].dstBinding = 1;
    ssaoWrites[1].dstArrayElement = 0;
    ssaoWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ssaoWrites[1].descriptorCount = 1;
    ssaoWrites[1].pImageInfo = &normInfo;

    ssaoWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssaoWrites[2].dstSet = ssaoDescriptorSet;
    ssaoWrites[2].dstBinding = 2;
    ssaoWrites[2].dstArrayElement = 0;
    ssaoWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ssaoWrites[2].descriptorCount = 1;
    ssaoWrites[2].pImageInfo = &noiseInfo;

    ssaoWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssaoWrites[3].dstSet = ssaoDescriptorSet;
    ssaoWrites[3].dstBinding = 3;
    ssaoWrites[3].dstArrayElement = 0;
    ssaoWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ssaoWrites[3].descriptorCount = 1;
    ssaoWrites[3].pBufferInfo = &kernelInfo;

    ssaoWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssaoWrites[4].dstSet = ssaoDescriptorSet;
    ssaoWrites[4].dstBinding = 4;
    ssaoWrites[4].dstArrayElement = 0;
    ssaoWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    ssaoWrites[4].descriptorCount = 1;
    ssaoWrites[4].pImageInfo = &ssaoRawImageInfo;

    ssaoWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ssaoWrites[5].dstSet = ssaoDescriptorSet;
    ssaoWrites[5].dstBinding = 5;
    ssaoWrites[5].dstArrayElement = 0;
    ssaoWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ssaoWrites[5].descriptorCount = 1;
    ssaoWrites[5].pBufferInfo = &globalUboInfo;

    vkUpdateDescriptorSets(m_device.getDevice(), static_cast<uint32_t>(ssaoWrites.size()), ssaoWrites.data(), 0, nullptr);

    // Update Blur descriptor set
    VkDescriptorImageInfo rawImageInfo{};
    rawImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    rawImageInfo.imageView = ssaoRawView;
    rawImageInfo.sampler = defaultSampler;

    VkDescriptorImageInfo blurImageInfo{};
    blurImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    blurImageInfo.imageView = ssaoBlurView;

    std::array<VkWriteDescriptorSet, 2> blurWrites{};

    blurWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    blurWrites[0].dstSet = blurDescriptorSet;
    blurWrites[0].dstBinding = 0;
    blurWrites[0].dstArrayElement = 0;
    blurWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    blurWrites[0].descriptorCount = 1;
    blurWrites[0].pImageInfo = &rawImageInfo;

    blurWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    blurWrites[1].dstSet = blurDescriptorSet;
    blurWrites[1].dstBinding = 1;
    blurWrites[1].dstArrayElement = 0;
    blurWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    blurWrites[1].descriptorCount = 1;
    blurWrites[1].pImageInfo = &blurImageInfo;

    vkUpdateDescriptorSets(m_device.getDevice(), static_cast<uint32_t>(blurWrites.size()), blurWrites.data(), 0, nullptr);
}

void SSAO::resize(VkExtent2D newExtent) {
    // Resize logic
}

void SSAO::executeSSAO(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ssaoPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ssaoPipelineLayout, 0, 1, &ssaoDescriptorSet, 0, nullptr);
    
    glm::vec2 screenDim(m_extent.width, m_extent.height);
    vkCmdPushConstants(cmd, ssaoPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::vec2), &screenDim);

    vkCmdDispatch(cmd, (m_extent.width + 15) / 16, (m_extent.height + 15) / 16, 1);
}

void SSAO::executeSSAOBlur(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipelineLayout, 0, 1, &blurDescriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, (m_extent.width + 15) / 16, (m_extent.height + 15) / 16, 1);
}

} // namespace Renderer
} // namespace Cogent
