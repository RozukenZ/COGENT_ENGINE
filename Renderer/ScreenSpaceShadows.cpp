#include "ScreenSpaceShadows.hpp"
#include <stdexcept>
#include <array>
#include "../Core/VulkanUtils.hpp"

ScreenSpaceShadows::ScreenSpaceShadows(GraphicsDevice& device, VkExtent2D extent) 
    : device(device), extent(extent) {
    init();
}

ScreenSpaceShadows::~ScreenSpaceShadows() {
    vkDestroyImageView(device.getDevice(), imageView, nullptr);
    vkDestroyImage(device.getDevice(), image, nullptr);
    vkFreeMemory(device.getDevice(), memory, nullptr);
    
    vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
    vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device.getDevice(), descriptorSetLayout, nullptr);
}

void ScreenSpaceShadows::init() {
    createResources();
    createDescriptorSetLayout();
    createPipeline();
}

void ScreenSpaceShadows::createResources() {
    // 8-bit UNORM is enough for a shadow mask (0.0 - 1.0)
    VkFormat format = VK_FORMAT_R8_UNORM; 

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; 
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device.getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSS Image!");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device.getDevice(), image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate SSS Memory!");
    }

    vkBindImageMemory(device.getDevice(), image, memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSS View!");
    }

    // Create Descriptor Pool
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSS Descriptor Pool!");
    }
}

void ScreenSpaceShadows::createDescriptorSetLayout() {
    // Inputs:
    // 0: G-Buffer Depth (Sampled)
    // 1: Output Image (Storage)
    // 2: Global UBO (Camera Info)
    
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorCount = 1;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device.getDevice(), &info, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
         throw std::runtime_error("Failed to create SSS Descriptor Layout!");
    }
}

void ScreenSpaceShadows::updateDescriptorSets(VkImageView depthView, VkSampler depthSampler, VkBuffer globalUbo) {
    if (descriptorSet == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }; 
        
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool; 
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &descriptorSet);
    }

    std::array<VkWriteDescriptorSet, 3> writes{};

    VkDescriptorImageInfo depthInfo{};
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthInfo.imageView = depthView;
    depthInfo.sampler = depthSampler;
    
    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputInfo.imageView = imageView;
    
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = globalUbo;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0; // Depth
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; 
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &depthInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1; // Output
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &outputInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = descriptorSet;
    writes[2].dstBinding = 2; // UBO
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device.getDevice(), (uint32_t)writes.size(), writes.data(), 0, nullptr);
}

// ... createPipeline ...

void ScreenSpaceShadows::execute(VkCommandBuffer cmd, const glm::mat4& view, const glm::mat4& proj, const glm::vec4& lightDir) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    
    // Bind internal descriptor set (updated via updateDescriptorSets)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    struct PushConstants {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 lightDir;
    } pc;
    pc.view = view;
    pc.proj = proj;
    pc.lightDir = lightDir;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);
    
    // Dispatch (align to 16x16 workgroup size)
    uint32_t groupX = (extent.width + 15) / 16;
    uint32_t groupY = (extent.height + 15) / 16;
    vkCmdDispatch(cmd, groupX, groupY, 1);
}


