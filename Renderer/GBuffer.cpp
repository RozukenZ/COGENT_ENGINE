#include "GBuffer.hpp"
#include <stdexcept>
#include "../Core/VulkanUtils.hpp"
#include "../Core/Logger.hpp"

GBuffer::GBuffer(GraphicsDevice& device, uint32_t width, uint32_t height)
    : device(device), width(width), height(height), 
      renderPass(VK_NULL_HANDLE), framebuffer(VK_NULL_HANDLE), sampler(VK_NULL_HANDLE),
      descriptorSetLayout(VK_NULL_HANDLE), descriptorPool(VK_NULL_HANDLE), descriptorSet(VK_NULL_HANDLE) {
    // init() removed: Must be called explicitly after device creation
}

GBuffer::~GBuffer() {
    cleanup();
}

void GBuffer::resize(uint32_t newWidth, uint32_t newHeight) {
    width = newWidth;
    height = newHeight;
    cleanup();
    init();
}

void GBuffer::init() {
    // 1. Create Attachments
    createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, position);
    createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, normal);
    createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, albedo);
    
    // Depth attachment (using GraphicsDevice helper if available, but for now logic is here)
    // Assuming D32_SFLOAT for simplicity, ideally queried from device
    createAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, depth);

    // Populate attachments vector for easy access
    attachments.resize(4);
    attachments[0] = position;
    attachments[1] = normal;
    attachments[2] = albedo;
    attachments[3] = depth;

    // 2. Create Render Pass
    std::array<VkAttachmentDescription, 4> attachments = {};
    
    // Common Color Attachment Setup
    for (int i = 0; i < 3; ++i) {
        attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    attachments[0].format = position.format;
    attachments[1].format = normal.format;
    attachments[2].format = albedo.format;

    // Depth Setup
    attachments[3].format = depth.format;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRefs[3] = {
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };
    VkAttachmentReference depthRef = {3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 3;
    subpass.pColorAttachments = colorRefs;
    subpass.pDepthStencilAttachment = &depthRef;

    // Dependencies
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device.getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer Render Pass");
    }

    // 3. Create Framebuffer
    std::array<VkImageView, 4> attachViews = { position.view, normal.view, albedo.view, depth.view };
    VkFramebufferCreateInfo fbufInfo = {};
    fbufInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufInfo.renderPass = renderPass;
    fbufInfo.attachmentCount = static_cast<uint32_t>(attachViews.size());
    fbufInfo.pAttachments = attachViews.data();
    fbufInfo.width = width;
    fbufInfo.height = height;
    fbufInfo.layers = 1;

    if (vkCreateFramebuffer(device.getDevice(), &fbufInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer Framebuffer");
    }

    // 4. Create Sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    if (vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer Sampler");
    }

    // 5. Descriptor Sets (for Lighting Pass reading)
    // DO LATER or HERE? User wants GBuffer setup.
    // I'll leave descriptor creation for now to keep it focused on the "Buffer" part.
    // Actually, Lighting pass needs to read this.
}

void GBuffer::createAttachment(VkFormat format, VkImageUsageFlags usage, FramebufferAttachment& attachment) {
    attachment.format = format;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;

    LOG_INFO("GBuffer: Creating Attachment with size " + std::to_string(width) + "x" + std::to_string(height));

    if (vkCreateImage(device.getDevice(), &imageInfo, nullptr, &attachment.image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer attachment image");
    }

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device.getDevice(), attachment.image, &memReqs);
    LOG_INFO("GBuffer: Image created. MemReqs Size: " + std::to_string(memReqs.size));

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkResult allocResult = vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &attachment.mem);
    if (allocResult != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate G-Buffer image memory! Result: " + std::to_string(allocResult));
        LOG_ERROR("  TypeIndex: " + std::to_string(allocInfo.memoryTypeIndex));
        LOG_ERROR("  Size: " + std::to_string(allocInfo.allocationSize));
        throw std::runtime_error("Failed to allocate G-Buffer image memory");
    }

    vkBindImageMemory(device.getDevice(), attachment.image, attachment.mem, 0);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = attachment.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = (format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &attachment.view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer image view");
    }
}

void GBuffer::cleanup() {
    VkDevice dev = device.getDevice();
    if (dev == VK_NULL_HANDLE) return;

    vkDestroySampler(dev, sampler, nullptr);
    vkDestroyFramebuffer(dev, framebuffer, nullptr);
    vkDestroyRenderPass(dev, renderPass, nullptr);

    auto destroyAttach = [&](FramebufferAttachment& att) {
        vkDestroyImageView(dev, att.view, nullptr);
        vkDestroyImage(dev, att.image, nullptr);
        vkFreeMemory(dev, att.mem, nullptr);
    };

    destroyAttach(position);
    destroyAttach(normal);
    destroyAttach(albedo);
    destroyAttach(depth);
    
    attachments.clear();
}