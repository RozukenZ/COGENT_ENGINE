#include "RenderGraph.hpp"
#include <iostream>
#include "../../Core/Logger.hpp"
#include "../../Core/Diagnostics/Profiler.hpp"

RenderGraph::RenderGraph(GraphicsDevice& device) : device(device) {}

RenderGraph::~RenderGraph() {
    cleanup();
}

void RenderGraph::cleanup() {
    for (auto& pair : resources) {
        if (pair.second.isTransient) {
            vkDestroyImageView(device.getDevice(), pair.second.view, nullptr);
            vmaDestroyImage(device.getAllocator(), pair.second.image, pair.second.allocation);
        }
    }
    resources.clear();
    passes.clear();
}

void RenderGraph::registerImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkImageLayout initialLayout) {
    RenderGraphResource res{};
    res.name = name;
    res.image = image;
    res.view = view;
    res.format = format;
    res.isTransient = false;
    res.currentLayout = initialLayout;
    res.currentAccess = 0; // Assuming fresh start
    res.currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    
    resources[name] = res;
}

void RenderGraph::createTransientImage(const std::string& name, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage) {
    RenderGraphResource res{};
    res.name = name;
    res.format = format;
    res.isTransient = true;
    res.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    res.currentAccess = 0;
    res.currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = extent;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    // Prefer lazily allocated if available (usually on mobile/tiled architectures)
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

    if (vmaCreateImage(device.getAllocator(), &imageInfo, &allocInfo, &res.image, &res.allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create transient image in RenderGraph");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = res.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &res.view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create transient image view in RenderGraph");
    }

    resources[name] = res;
}

VkImageView RenderGraph::getImageView(const std::string& name) {
    if (resources.find(name) != resources.end()) {
        return resources[name].view;
    }
    return VK_NULL_HANDLE;
}

void RenderGraph::addPass(RenderPassNode node) {
    passes.push_back(node);
}

void RenderGraph::compile() {
    PROFILE_FUNCTION();
    // Basic validation or sorting could go here
    for (auto& pass : passes) {
        if (pass.setup) {
            pass.setup(device);
        }
    }
}

void RenderGraph::execute(VkCommandBuffer cmd, uint32_t imageIndex) {
    PROFILE_FUNCTION();
    for (auto& pass : passes) {
        // 1. Pre-Pass Barriers (Transition Inputs & Outputs)
        // Check Inputs
        for (const auto& input : pass.inputs) {
            if (resources.find(input.name) != resources.end()) {
                insertBarrier(cmd, resources[input.name], input);
            }
        }
        
        // Check Outputs
        for (const auto& output : pass.outputs) {
             if (resources.find(output.name) != resources.end()) {
                insertBarrier(cmd, resources[output.name], output);
            }
        }

        // 2. Execute Pass
        if (pass.execute) {
            pass.execute(cmd, imageIndex);
        }
    }
}

void RenderGraph::insertBarrier(VkCommandBuffer cmd, RenderGraphResource& resource, const RenderPassResourceInfo& target) {
    // Only insert barrier if layout or access changes
    if (resource.currentLayout == target.targetLayout && 
        resource.currentAccess == target.targetAccess &&
        resource.currentStage == target.targetStage) {
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = resource.currentLayout;
    barrier.newLayout = target.targetLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = resource.image;
    
    // Determine Aspect Mask based on format roughly
    if (target.targetLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || 
        target.targetLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = resource.currentAccess;
    barrier.dstAccessMask = target.targetAccess;

    vkCmdPipelineBarrier(
        cmd,
        resource.currentStage,
        target.targetStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // Update State Tracking
    resource.currentLayout = target.targetLayout;
    resource.currentAccess = target.targetAccess;
    resource.currentStage = target.targetStage;
    
    // LOG_INFO("Barrier inserted for " + resource.name + " -> " + std::to_string(target.targetLayout));
}
