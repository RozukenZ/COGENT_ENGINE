#include "ANSRPass.hpp"
#include <stdexcept>
#include <array>
#include "../../Core/VulkanUtils.hpp"
#include "../../Core/Logger.hpp"

namespace Cogent::Renderer {

    ANSRPass::ANSRPass(GraphicsDevice& device, VkExtent2D renderExtent, VkExtent2D displayExtent)
        : device(device), renderExtent(renderExtent), displayExtent(displayExtent) {
        createDescriptorSetLayout();
        createResources();
        createPipeline();
    }

    ANSRPass::~ANSRPass() {
        vkDestroyImageView(device.getDevice(), displayView, nullptr);
        vkDestroyImage(device.getDevice(), displayImage, nullptr);
        vkFreeMemory(device.getDevice(), displayMemory, nullptr);

        vkDestroyImageView(device.getDevice(), historyView, nullptr);
        vkDestroyImage(device.getDevice(), historyImage, nullptr);
        vkFreeMemory(device.getDevice(), historyMemory, nullptr);

        vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device.getDevice(), descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device.getDevice(), descriptorPool, nullptr);
    }

    void ANSRPass::resize(VkExtent2D newRenderExtent, VkExtent2D newDisplayExtent) {
        vkDeviceWaitIdle(device.getDevice());
        renderExtent = newRenderExtent;
        displayExtent = newDisplayExtent;
        
        // Recreate resources
        vkDestroyImageView(device.getDevice(), displayView, nullptr);
        vkDestroyImage(device.getDevice(), displayImage, nullptr);
        vkFreeMemory(device.getDevice(), displayMemory, nullptr);

        vkDestroyImageView(device.getDevice(), historyView, nullptr);
        vkDestroyImage(device.getDevice(), historyImage, nullptr);
        vkFreeMemory(device.getDevice(), historyMemory, nullptr);

        createResources();
        // Trigger descriptor update next frame
        firstFrame = true; 
    }

    void ANSRPass::createResources() {
        // Create Display Image (STORAGE | SAMPLED | TRANSFER_SRC)
        // High Res Output
        VulkanUtils::createImage(device.getDevice(), device.getPhysicalDevice(),
            displayExtent.width, displayExtent.height,
            VK_FORMAT_B8G8R8A8_UNORM, // Standard Swapchain format-ish
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            displayImage, displayMemory);

        displayView = VulkanUtils::createImageView(device.getDevice(), displayImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

        // Create History Image (Same format)
        VulkanUtils::createImage(device.getDevice(), device.getPhysicalDevice(),
            displayExtent.width, displayExtent.height,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            historyImage, historyMemory);

        historyView = VulkanUtils::createImageView(device.getDevice(), historyImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        
        // Clear History to black initially
        // (In real engine, use a command buffer to clear)
    }

    void ANSRPass::createDescriptorSetLayout() {
        std::array<VkDescriptorSetLayoutBinding, 5> bindings{};
        
        // 0: Input Color (Low Res)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 1: Input Motion (Low Res)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 2: Input Depth (Low Res)
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        // 3: History (High Res - Read)
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // 4: Output (High Res - Write)
        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ANSR Descriptor Layout!");
        }

        // Pool
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = 4;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;

        vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &descriptorPool);

        // Allocate Set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &descriptorSet);
    }

    void ANSRPass::createPipeline() {
        auto computeShaderCode = VulkanUtils::readFile("Shaders/ANSR.comp.spv");

        VkShaderModule computeShaderModule = VulkanUtils::createShaderModule(device.getDevice(), computeShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = computeShaderModule;
        computeShaderStageInfo.pName = "main";

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ANSR Pipeline Layout!");
        }

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stage = computeShaderStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ANSR Compute Pipeline!");
        }

        vkDestroyShaderModule(device.getDevice(), computeShaderModule, nullptr);
    }

    void ANSRPass::updateDescriptorSets(VkImageView inputColor, VkImageView inputMotion, VkImageView inputDepth) {
         VkDescriptorImageInfo colorInfo{};
         colorInfo.imageView = inputColor; 
         colorInfo.sampler = VK_NULL_HANDLE; // Immutable or separate sampler if shader needs it

         VkDescriptorImageInfo motionInfo{};
         motionInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         motionInfo.imageView = inputMotion;
         motionInfo.sampler = VK_NULL_HANDLE;

         VkDescriptorImageInfo depthInfo{};
         depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         depthInfo.imageView = inputDepth;
         depthInfo.sampler = VK_NULL_HANDLE;
         
         VkDescriptorImageInfo historyInfo{};
         historyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // or GENERAL
         historyInfo.imageView = historyView;
         historyInfo.sampler = VK_NULL_HANDLE;
         
         VkDescriptorImageInfo outputInfo{};
         outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
         outputInfo.imageView = displayView;

         std::array<VkWriteDescriptorSet, 5> descriptorWrites{};
         
         // 0: Input Color
         descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         descriptorWrites[0].dstSet = descriptorSet;
         descriptorWrites[0].dstBinding = 0;
         descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         descriptorWrites[0].descriptorCount = 1;
         descriptorWrites[0].pImageInfo = &colorInfo;

         // 1: Motion
         descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         descriptorWrites[1].dstSet = descriptorSet;
         descriptorWrites[1].dstBinding = 1;
         descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         descriptorWrites[1].descriptorCount = 1;
         descriptorWrites[1].pImageInfo = &motionInfo;
         
         // 2: Depth
         descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         descriptorWrites[2].dstSet = descriptorSet;
         descriptorWrites[2].dstBinding = 2;
         descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         descriptorWrites[2].descriptorCount = 1;
         descriptorWrites[2].pImageInfo = &depthInfo;
         
         // 3: History (Read)
         descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         descriptorWrites[3].dstSet = descriptorSet;
         descriptorWrites[3].dstBinding = 3;
         descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         descriptorWrites[3].descriptorCount = 1;
         descriptorWrites[3].pImageInfo = &historyInfo;
         
         // 4: Output (Write)
         descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         descriptorWrites[4].dstSet = descriptorSet;
         descriptorWrites[4].dstBinding = 4;
         descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
         descriptorWrites[4].descriptorCount = 1;
         descriptorWrites[4].pImageInfo = &outputInfo;

         vkUpdateDescriptorSets(device.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    void ANSRPass::execute(VkCommandBuffer cmd, VkImageView inputColor, VkImageView inputMotion, VkImageView inputDepth) {
        // Ensure Output Image is in GENERAL layout for Compute Write
        VulkanUtils::transitionImageLayout(cmd, displayImage, VK_FORMAT_B8G8R8A8_UNORM, 
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 
            0, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            
         // Ensure History Image is in SHADER_READ_ONLY layout (if it wasn't already)
         // ... (Assuming handled by previous frame copy or init)

        updateDescriptorSets(inputColor, inputMotion, inputDepth);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // Dispatch (Global Size / Local Size)
        uint32_t groupX = (displayExtent.width + 15) / 16;
        uint32_t groupY = (displayExtent.height + 15) / 16;
        vkCmdDispatch(cmd, groupX, groupY, 1);
        
        // Post-Barrier: Output Write -> Transfer Read (to copy to history)
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // Prepare for History Copy
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.image = displayImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Copy Display -> History for next frame
        // Transition History to Transfer DST
        VulkanUtils::transitionImageLayout(cmd, historyImage, VK_FORMAT_B8G8R8A8_UNORM, 
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
             VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.extent.width = displayExtent.width;
        copyRegion.extent.height = displayExtent.height;
        copyRegion.extent.depth = 1;

        vkCmdCopyImage(cmd, displayImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       historyImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &copyRegion);
                       
        // Transition History back to Shader Read
        VulkanUtils::transitionImageLayout(cmd, historyImage, VK_FORMAT_B8G8R8A8_UNORM, 
             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
             VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT); // For next frame or next pass
    }

}
