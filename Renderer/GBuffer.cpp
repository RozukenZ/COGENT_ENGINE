#include "GBuffer.hpp"
#include <stdexcept>
#include <array>
#include "../Core/VulkanUtils.hpp"

void GBuffer::init(VkDevice device, VkPhysicalDevice physDevice, uint32_t width, uint32_t height) {
    // [CRITICAL FIX] Resize vector immediately to prevent "Vector subscript out of range"
    // This ensures slots 0, 1, 2, 3 exist before we access them.
    attachments.resize(COUNT);

    // 1. SETUP ATTACHMENT FORMATS
    // Albedo (Color): 8-bit is sufficient for standard color
    attachments[ALBEDO].format = VK_FORMAT_R8G8B8A8_UNORM;
    
    // Normal (Geometry): 16-bit float for high precision lighting calculations
    attachments[NORMAL].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    
    // Velocity (Motion): 16-bit float for accurate TAA (Phase 1 Goal)
    attachments[VELOCITY].format = VK_FORMAT_R16G16_SFLOAT;
    
    // Depth (Distance): 32-bit float for high precision depth testing
    attachments[DEPTH].format = VK_FORMAT_D32_SFLOAT;

    // 2. CREATE IMAGES (TEXTURES) ON GPU
    for (int i = 0; i < COUNT; i++) {
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT; // Must be readable by the Lighting Pass shader
        
        if (i == DEPTH) {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        createAttachment(device, physDevice, width, height, attachments[i].format, usage, attachments[i]);
    }

    // 3. DEFINE RENDER PASS (Rendering Rules)
    std::vector<VkAttachmentDescription> attDescs(COUNT);
    std::vector<VkAttachmentReference> colorRefs; 
    VkAttachmentReference depthRef{};

    for (int i = 0; i < COUNT; i++) {
        attDescs[i].format = attachments[i].format;
        attDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear screen every frame
        attDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store result for lighting pass
        attDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        
        // Automatic Layout Transitions
        if (i == DEPTH) {
            attDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // Ready for next pass
            
            // Setup Depth Reference
            depthRef.attachment = i;
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        } else {
            attDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            // Note: We transition to COLOR_ATTACHMENT_OPTIMAL here. 
            // The Main Loop Barrier will later transition it to SHADER_READ_ONLY_OPTIMAL.
            attDescs[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 
            
            // Setup Color Reference
            VkAttachmentReference ref{};
            ref.attachment = i;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorRefs.push_back(ref);
        }
    }

    // Subpass Definition
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data(); // Bind Albedo, Normal, Velocity
    subpass.pDepthStencilAttachment = &depthRef;  // Bind Depth

    // Dependencies (Synchronization)
    std::array<VkSubpassDependency, 2> dependencies;
    
    // Transition In (Start of Render Pass)
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Transition Out (End of Render Pass)
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create Render Pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attDescs.size());
    renderPassInfo.pAttachments = attDescs.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer Render Pass!");
    }

    // 4. CREATE FRAMEBUFFER (Combine all attachments)
    std::vector<VkImageView> attachmentViews;
    for(int i=0; i<COUNT; i++) {
        attachmentViews.push_back(attachments[i].view);
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
    framebufferInfo.pAttachments = attachmentViews.data();
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer Framebuffer!");
    }
}

void GBuffer::cleanup(VkDevice device) {
    // 1. Destroy Framebuffer first
    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }

    // 2. Destroy Render Pass
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    // 3. Clean up Resources for each Attachment
    for (auto& att : attachments) {
        // Destroy View
        if (att.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, att.view, nullptr);
            att.view = VK_NULL_HANDLE;
        }

        // Destroy Image Object
        if (att.image != VK_NULL_HANDLE) {
            vkDestroyImage(device, att.image, nullptr);
            att.image = VK_NULL_HANDLE;
        }

        // Free VRAM
        if (att.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, att.memory, nullptr);
            att.memory = VK_NULL_HANDLE;
        }
    }
    
    // Clear the vector so it can be re-initialized if needed
    attachments.clear();
}

void GBuffer::createAttachment(VkDevice device, VkPhysicalDevice physDevice, uint32_t width, uint32_t height, 
                               VkFormat format, VkImageUsageFlags usage, Attachment& attachment) {
    attachment.format = format;

    // 1. Create Image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &attachment.image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer Image!");
    }

    // 2. Allocate Memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, attachment.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &attachment.memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate G-Buffer memory!");
    }

    vkBindImageMemory(device, attachment.image, attachment.memory, 0);

    // 3. Create Image View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = attachment.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    
    // Determine aspect mask based on usage
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &attachment.view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer Image View!");
    }
}