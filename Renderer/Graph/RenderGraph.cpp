#include "RenderGraph.hpp"
#include <iostream>
#include "../../Core/Logger.hpp"

RenderGraph::RenderGraph(GraphicsDevice& device) : device(device) {}

void RenderGraph::registerImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkImageLayout initialLayout) {
    RenderGraphResource res{};
    res.name = name;
    res.image = image;
    res.view = view;
    res.format = format;
    res.currentLayout = initialLayout;
    res.currentAccess = 0; // Assuming fresh start
    res.currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    
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
    // Basic validation or sorting could go here
    for (auto& pass : passes) {
        if (pass.setup) {
            pass.setup(device);
        }
    }
}

void RenderGraph::execute(VkCommandBuffer cmd) {
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
        // Debug Marker
        // vkCmdDebugMarkerBeginEXT... (if supported)
        
        if (pass.execute) {
            pass.execute(cmd);
        }
        
        // vkCmdDebugMarkerEndEXT...
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
