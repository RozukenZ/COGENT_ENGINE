#include "LightCulling.hpp"
#include "../../Core/VulkanUtils.hpp"
#include "../../Core/EmbeddedShaders.hpp"
#include <stdexcept>
#include <cmath>

// Helper for buffer creation without VMA (using existing VulkanUtils logic or simple allocator if needed)
// Wait, we just added VMA to GraphicsDevice! So we should use it.
#include <vk_mem_alloc.h>

LightCulling::LightCulling(GraphicsDevice& device, PipelineCache& pipelineCache)
    : device(device), pipelineCache(pipelineCache), maxLights(0),
      lightBuffer(VK_NULL_HANDLE), gridBuffer(VK_NULL_HANDLE), indexBuffer(VK_NULL_HANDLE),
      uboBuffer(VK_NULL_HANDLE), atomicCounterBuffer(VK_NULL_HANDLE) {
    descriptorAllocator.init(device.getDevice());
    layoutCache.init(device.getDevice());
}

LightCulling::~LightCulling() {
    auto vma = device.getAllocator();
    
    if (lightBuffer != VK_NULL_HANDLE) vmaDestroyBuffer(vma, lightBuffer, lightBufferMemory);
    if (gridBuffer != VK_NULL_HANDLE) vmaDestroyBuffer(vma, gridBuffer, gridBufferMemory);
    if (indexBuffer != VK_NULL_HANDLE) vmaDestroyBuffer(vma, indexBuffer, indexBufferMemory);
    if (uboBuffer != VK_NULL_HANDLE) vmaDestroyBuffer(vma, uboBuffer, uboBufferMemory);
    if (atomicCounterBuffer != VK_NULL_HANDLE) vmaDestroyBuffer(vma, atomicCounterBuffer, atomicCounterBufferMemory);

    vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    
    descriptorAllocator.cleanup();
    layoutCache.cleanup();
}

void LightCulling::init(VkExtent2D screenExtent, uint32_t maxLightsCount) {
    this->extent = screenExtent;
    this->maxLights = maxLightsCount;

    gridDims.x = (extent.width + 15) / 16;
    gridDims.y = (extent.height + 15) / 16;
    gridDims.z = 24;

    createBuffers();
    createDescriptors();
    createPipeline();
}

void LightCulling::createBuffers() {
    auto vma = device.getAllocator();

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    // Light Buffer
    bufferInfo.size = sizeof(PointLight) * maxLights;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    vmaCreateBuffer(vma, &bufferInfo, &allocInfo, &lightBuffer, &lightBufferMemory, nullptr);

    // Grid Buffer
    uint32_t numClusters = gridDims.x * gridDims.y * gridDims.z;
    bufferInfo.size = sizeof(LightGrid) * numClusters;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    allocInfo.flags = 0;
    vmaCreateBuffer(vma, &bufferInfo, &allocInfo, &gridBuffer, &gridBufferMemory, nullptr);

    // Index Buffer
    bufferInfo.size = sizeof(uint32_t) * maxLights * 64; 
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    allocInfo.flags = 0;
    vmaCreateBuffer(vma, &bufferInfo, &allocInfo, &indexBuffer, &indexBufferMemory, nullptr);

    // UBO Buffer
    bufferInfo.size = sizeof(LightCullingUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    vmaCreateBuffer(vma, &bufferInfo, &allocInfo, &uboBuffer, &uboBufferMemory, nullptr);

    // Atomic Buffer
    bufferInfo.size = sizeof(uint32_t);
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocInfo.flags = 0;
    vmaCreateBuffer(vma, &bufferInfo, &allocInfo, &atomicCounterBuffer, &atomicCounterBufferMemory, nullptr);
}

void LightCulling::createDescriptors() {
    VkDescriptorBufferInfo lightInfo{};
    lightInfo.buffer = lightBuffer;
    lightInfo.offset = 0;
    lightInfo.range = sizeof(PointLight) * maxLights;

    VkDescriptorBufferInfo gridInfo{};
    gridInfo.buffer = gridBuffer;
    gridInfo.offset = 0;
    gridInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo indexInfo{};
    indexInfo.buffer = indexBuffer;
    indexInfo.offset = 0;
    indexInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = uboBuffer;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(LightCullingUBO);

    VkDescriptorBufferInfo atomicInfo{};
    atomicInfo.buffer = atomicCounterBuffer;
    atomicInfo.offset = 0;
    atomicInfo.range = sizeof(uint32_t);

    DescriptorBuilder::begin(&layoutCache, &descriptorAllocator)
        .bindBuffer(0, &lightInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bindBuffer(1, &gridInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(2, &indexInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(3, &uboInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bindBuffer(4, &atomicInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(descriptorSet, descriptorSetLayout);
}

void LightCulling::createPipeline() {
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device.getDevice(), &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    auto compCode = EmbeddedShaders::GetShader("Shaders/light_culling.comp.spv");
    VkShaderModule compModule = VulkanUtils::createShaderModule(device.getDevice(), compCode);

    VkPipelineShaderStageCreateInfo compStageInfo{};
    compStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compStageInfo.module = compModule;
    compStageInfo.pName = "main";

    pipeline = pipelineCache.buildComputePipeline("LightCulling", compStageInfo, pipelineLayout);

    vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
}

void LightCulling::updateUBO(const LightCullingUBO& uboData) {
    void* data;
    auto vma = device.getAllocator();
    vmaMapMemory(vma, uboBufferMemory, &data);
    memcpy(data, &uboData, sizeof(LightCullingUBO));
    vmaUnmapMemory(vma, uboBufferMemory);
}

void LightCulling::updateLights(const std::vector<PointLight>& lights) {
    size_t size = std::min(lights.size() * sizeof(PointLight), (size_t)maxLights * sizeof(PointLight));
    if (size > 0) {
        void* data;
        auto vma = device.getAllocator();
        vmaMapMemory(vma, lightBufferMemory, &data);
        memcpy(data, lights.data(), size);
        vmaUnmapMemory(vma, lightBufferMemory);
    }
}

void LightCulling::execute(VkCommandBuffer cmd) {
    // Reset atomic counter
    vkCmdFillBuffer(cmd, atomicCounterBuffer, 0, sizeof(uint32_t), 0);
    
    // Memory barrier to ensure counter is zeroed
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Group count matches Grid Dimensions
    vkCmdDispatch(cmd, gridDims.x, gridDims.y, gridDims.z);
}
