#include "AutoExposurePass.hpp"
#include <stdexcept>
#include <array>
#include "../../Core/VulkanUtils.hpp"

namespace Cogent::Renderer {

    AutoExposurePass::AutoExposurePass(GraphicsDevice& device) : device(device) {
        createResources();
        createPipeline();
    }

    AutoExposurePass::~AutoExposurePass() {
        vkDestroyBuffer(device.getDevice(), exposureBuffer, nullptr);
        vkFreeMemory(device.getDevice(), exposureMemory, nullptr);
        
        vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device.getDevice(), descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device.getDevice(), descriptorPool, nullptr);
    }

    void AutoExposurePass::createResources() {
        // Single float for Exposure/Luminance
        VkDeviceSize bufferSize = sizeof(float);
        
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(device.getDevice(), &bufferInfo, nullptr, &exposureBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Exposure Buffer!");
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device.getDevice(), exposureBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &exposureMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate Exposure Memory!");
        }
        vkBindBufferMemory(device.getDevice(), exposureBuffer, exposureMemory, 0);
    }

    void AutoExposurePass::createPipeline() {
        // 1. Create Descriptor Set Layout
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        
        if (vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create AutoExposure Descriptor Layout!");
        }

        // 2. Create Descriptor Pool
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = 1;
        
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;
        
        if (vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create AutoExposure Descriptor Pool!");
        }

        // 3. Allocate Descriptor Set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        if (vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS) {
             throw std::runtime_error("Failed to allocate AutoExposure Descriptor Set!");
        }
        
        // 4. Create Pipeline Layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create AutoExposure Pipeline Layout!");
        }

        // 5. Load Shader and Create Pipeline
        auto computeShaderCode = VulkanUtils::readFile("Shaders/AutoExposure.comp.spv");
        VkShaderModule computeShaderModule = VulkanUtils::createShaderModule(device.getDevice(), computeShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = computeShaderModule;
        computeShaderStageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stage = computeShaderStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create AutoExposure Pipeline!");
        }
        vkDestroyShaderModule(device.getDevice(), computeShaderModule, nullptr);
    }

    void AutoExposurePass::execute(VkCommandBuffer cmd, VkImageView inputColor) {
        // Update Descriptor Set
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = inputColor;
        imageInfo.sampler = VK_NULL_HANDLE; 

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = exposureBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(float);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        // Bind & Dispatch
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // We only need 1 thread group because thread (0,0) does the sampling work in our simple shader
        vkCmdDispatch(cmd, 1, 1, 1);
    }
}
