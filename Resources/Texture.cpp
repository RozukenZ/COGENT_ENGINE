#include "Texture.hpp"
#include <stdexcept>
#include <iostream>

// Library eksternal untuk load gambar
#define STB_IMAGE_IMPLEMENTATION
#include "../Vendor/stb_image.h"

// Note: We need to include GraphicsDevice for the override
#include "../Core/Graphics/GraphicsDevice.hpp"

void Texture::load(VkDevice device, VkPhysicalDevice physDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string& filepath) {
    this->path = filepath;
    loadCPU();
    
    // We can't easily call uploadGPU(GraphicsDevice) here because we have individual Vk handles.
    // But legacy load is synchronous.
    // Let's just replicate the logic or create a temporary GraphicsDevice wrapper is too complex.
    // We will stick to the existing implementation for legacy load but redirect it where possible.
    
    // Actually, for legacy load, let's just use the existing logic but using pixelData member.
    
    // Check if loadCPU failed/succeeded
    if (pixelData.empty()) return;

    // Upload Logic (Inline for now to support legacy call style)
    VkDeviceSize imageSize = width * height * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory);
    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixelData.data(), static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // Free CPU data after upload (optional, keep if we want to re-upload)
    // pixelData.clear(); 
    // pixelData.shrink_to_fit();

    createImage(device, physDevice, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    transitionImageLayout(device, commandPool, graphicsQueue, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer, textureImage, width, height);
    transitionImageLayout(device, commandPool, graphicsQueue, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &textureImageView);

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE; 
    
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);

    state = Cogent::Resources::StreamingState::RESIDENT;
}

void Texture::loadCPU() {
    state = Cogent::Resources::StreamingState::LOADING;
    
    int texWidth, texHeight;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    
    isFallback = false;
    if (!pixels) {
        std::cerr << "WARNING: Gagal memuat texture: " << path << ". Menggunakan fallback." << std::endl;
        texWidth = 1; texHeight = 1; texChannels = 4;
        this->width = 1; this->height = 1; // [FIX] Update member variables!
        pixelData.resize(4);
        pixelData[0] = 255; pixelData[1] = 255; pixelData[2] = 255; pixelData[3] = 255;
        isFallback = true;
    } else {
        width = static_cast<uint32_t>(texWidth);
        height = static_cast<uint32_t>(texHeight);
        VkDeviceSize size = width * height * 4;
        pixelData.resize(size);
        memcpy(pixelData.data(), pixels, size);
        stbi_image_free(pixels);
    }
    
    state = Cogent::Resources::StreamingState::LOADED_CPU;
}

void Texture::uploadGPU(GraphicsDevice& device) {
    if (pixelData.empty()) return;

    state = Cogent::Resources::StreamingState::UPLOADING;

    VkDevice vkDevice = device.getDevice();
    VkPhysicalDevice physDevice = device.getPhysicalDevice();
    VkCommandPool cmdPool = device.getCommandPool();
    VkQueue queue = device.getGraphicsQueue(); // Use graphics queue for now

    // Re-use logic from load() but using public getters
    // Copy-paste logic from load() above essentially
    
    VkDeviceSize imageSize = width * height * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vkDevice, stagingBuffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    vkAllocateMemory(vkDevice, &allocInfo, nullptr, &stagingBufferMemory);
    vkBindBufferMemory(vkDevice, stagingBuffer, stagingBufferMemory, 0);

    void* data;
    vkMapMemory(vkDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixelData.data(), static_cast<size_t>(imageSize));
    vkUnmapMemory(vkDevice, stagingBufferMemory);

    // Free CPU data
    pixelData.clear();
    pixelData.shrink_to_fit();

    createImage(vkDevice, physDevice, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    transitionImageLayout(vkDevice, cmdPool, queue, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(vkDevice, cmdPool, queue, stagingBuffer, textureImage, width, height);
    transitionImageLayout(vkDevice, cmdPool, queue, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(vkDevice, stagingBuffer, nullptr);
    vkFreeMemory(vkDevice, stagingBufferMemory, nullptr);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(vkDevice, &viewInfo, nullptr, &textureImageView);

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE; 
    
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(vkDevice, &samplerInfo, nullptr, &textureSampler);

    state = Cogent::Resources::StreamingState::RESIDENT;
}

void Texture::unload() {
     // We don't have device handle stored in Texture to destroy resources...
     // cleanup(device) takes a device handle. 
     // This is a flaw in the current Texture design for RAII/Automatic unloading.
     // For now, assume manual cleanup or we need to store device handle.
     
     // Since this method override must match base class with no arguments, 
     // we can't easily implement full unload without storing VkDevice.
     
     if (textureImage != VK_NULL_HANDLE) {
         std::cerr << "WARNING: Texture::unload() called but cannot destroy Vulkan resources (Missing VkDevice handle). Use cleanup(device) instead." << std::endl;
         // In a real engine, Texture would hold a pointer to Device or RenderContext.
         // For now, we clear CPU data.
     }
     
     pixelData.clear();
     pixelData.shrink_to_fit();
     state = Cogent::Resources::StreamingState::UNLOADED;
}

void Texture::cleanup(VkDevice device) {
    if (textureSampler != VK_NULL_HANDLE) vkDestroySampler(device, textureSampler, nullptr);
    if (textureImageView != VK_NULL_HANDLE) vkDestroyImageView(device, textureImageView, nullptr);
    if (textureImage != VK_NULL_HANDLE) vkDestroyImage(device, textureImage, nullptr);
    if (textureImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, textureImageMemory, nullptr);
    
    textureSampler = VK_NULL_HANDLE;
    textureImageView = VK_NULL_HANDLE;
    textureImage = VK_NULL_HANDLE;
    textureImageMemory = VK_NULL_HANDLE;
    
    state = Cogent::Resources::StreamingState::UNLOADED;
}

// --- HELPER FUNCTIONS ---

VkCommandBuffer Texture::beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Texture::endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Texture::createImage(VkDevice device, VkPhysicalDevice physDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat Image Texture!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Gagal alokasi memori Texture!");
    }
    vkBindImageMemory(device, image, imageMemory, 0);
}

void Texture::transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("Transisi layout tidak didukung!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
}

void Texture::copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
}