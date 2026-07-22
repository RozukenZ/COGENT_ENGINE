#include "ShadowPass.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../Core/VulkanUtils.hpp"
#include <stdexcept>

ShadowPass::ShadowPass(GraphicsDevice& device) : device(device) {
    init();
}

ShadowPass::~ShadowPass() {
    vkDestroyDescriptorSetLayout(device.getDevice(), descriptorSetLayout, nullptr);
    vkDestroySampler(device.getDevice(), sampler, nullptr);
    vkDestroyImageView(device.getDevice(), shadowImageView, nullptr);
    vkDestroyImage(device.getDevice(), shadowImage, nullptr);
    vkFreeMemory(device.getDevice(), shadowMemory, nullptr);
    
    for (auto& cascade : cascades) {
        vkDestroyImageView(device.getDevice(), cascade.view, nullptr);
        vkDestroyFramebuffer(device.getDevice(), cascade.framebuffer, nullptr);
    }
    
    vkDestroyRenderPass(device.getDevice(), renderPass, nullptr);
}

void ShadowPass::init() {
    createRenderPass();
    createResources();
    createDescriptorSetLayout();
}

void ShadowPass::createRenderPass() {
    VkAttachmentDescription attachment{};
    attachment.format = VK_FORMAT_D32_SFLOAT;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &attachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device.getDevice(), &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Shadow Render Pass!");
    }
}

void ShadowPass::createResources() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = SHADOW_MAP_DIM;
    imageInfo.extent.height = SHADOW_MAP_DIM;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = CASCADE_COUNT;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device.getDevice(), &imageInfo, nullptr, &shadowImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Shadow Image!");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device.getDevice(), shadowImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &shadowMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Shadow Memory!");
    }

    vkBindImageMemory(device.getDevice(), shadowImage, shadowMemory, 0);

    // Full View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = shadowImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = CASCADE_COUNT;

    if (vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &shadowImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Shadow Image View!");
    }

    // Per-Cascade Views & Framebuffers
    for (int i = 0; i < CASCADE_COUNT; ++i) {
        VkImageViewCreateInfo cvInfo = viewInfo;
        cvInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; // Render to layer? 
        // Actually usually we use 2D view for framebuffer attachment
        cvInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; 
        cvInfo.subresourceRange.baseArrayLayer = i;
        cvInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device.getDevice(), &cvInfo, nullptr, &cascades[i].view) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create Shadow Cascade View!");
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &cascades[i].view;
        fbInfo.width = SHADOW_MAP_DIM;
        fbInfo.height = SHADOW_MAP_DIM;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device.getDevice(), &fbInfo, nullptr, &cascades[i].framebuffer) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create Shadow Cascade Framebuffer!");
        }
    }

    // Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_TRUE; 
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // PCF

    vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &sampler);
}

void ShadowPass::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;
    
    vkCreateDescriptorSetLayout(device.getDevice(), &info, nullptr, &descriptorSetLayout);
}

void ShadowPass::updateCascades(const glm::mat4& cameraView, const glm::mat4& cameraProj, const glm::vec3& lightDir) {
    float cascadeSplits[CASCADE_COUNT];

    float nearClip = 0.1f;
    float farClip = 1000.0f;
    float clipRange = farClip - nearClip;

    float minZ = nearClip;
    float maxZ = nearClip + clipRange;

    float range = maxZ - minZ;
    float ratio = maxZ / minZ;

    // Calculate split depths based on practical split scheme (mix of uniform and log)
    // For simplicity, using a hardcoded logarithmic-ish blend or pure log
    // Standard approach: lambda * log + (1-lambda) * uniform
    float lambda = 0.95f; 

    for (uint32_t i = 0; i < CASCADE_COUNT; i++) {
        float p = (i + 1) / static_cast<float>(CASCADE_COUNT);
        float log = minZ * std::pow(ratio, p);
        float uniform = minZ + range * p;
        float d = lambda * log + (1 - lambda) * uniform;
        cascadeSplits[i] = (d - nearClip) / clipRange; // Normalize to 0..1 range if needed, or keep View Space Z
        
        // Actually, let's store View Space Z split for the shader
        cascades[i].splitDepth = d; 
    }

    // Calculate Ortho Matrices
    float lastSplitDist = 0.0f;

    for (uint32_t i = 0; i < CASCADE_COUNT; i++) {
        float splitDist = cascades[i].splitDepth;

        // Frustum corners in World Space
        glm::vec3 frustumCorners[8] = {
            {-1.0f,  1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            {-1.0f, -1.0f, 0.0f},

            {-1.0f,  1.0f, 1.0f},
            { 1.0f,  1.0f, 1.0f},
            { 1.0f, -1.0f, 1.0f},
            {-1.0f, -1.0f, 1.0f},
        };

        // Project frustum corners into World Space
        glm::mat4 invCam = glm::inverse(cameraProj * cameraView);
        for (uint32_t j = 0; j < 8; j++) {
            glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
            frustumCorners[j] = invCorner / invCorner.w;
        }

        // We need the corners for the CURRENT cascade slice.
        // The inverse projection gave us the FULL frustum corners.
        // We need to interpolate these corners based on splitDist.
        // Corner 0-3 are Near Plane, 4-7 are Far Plane.

        for (uint32_t j = 0; j < 4; j++) {
            glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
            
            // cascades[i].splitDepth is View Space Z positive? 
            // Standard perspective projection usually maps near to 0, far to 1 in Z. 
            // My calculation above produced View Space Z. 
            // Need to normalized it?
            // Wait, invCam takes NDC (-1..1 XY, 0..1 Z) to World.
            // Split Distance calculation above returns View Space Z (e.g. 10.0, 50.0).
            // We need to convert that to 0..1 range or directly interpolate logic.
            
            // Normalized split distance (0..1)
            float splitDistNormalized = (splitDist - nearClip) / clipRange;
            float lastSplitDistNormalized = (lastSplitDist - nearClip) / clipRange;
            if (i == 0) lastSplitDistNormalized = 0.0f; // Force near plane start

            frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDistNormalized);
            frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDistNormalized);
        }

        // Calculate center of this frustum slice
        glm::vec3 center = glm::vec3(0.0f);
        for (uint32_t j = 0; j < 8; j++) {
            center += frustumCorners[j];
        }
        center /= 8.0f;

        // Create Light View Matrix
        // Looking at center from light direction
        // Radius to ensure we cover everything while rotating
        float radius = 0.0f;
        for (uint32_t j = 0; j < 8; j++) {
            float distance = glm::length(frustumCorners[j] - center);
            radius = glm::max(radius, distance);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        glm::vec3 maxExtents = glm::vec3(radius);
        glm::vec3 minExtents = -maxExtents;

        // Light Matrix
        // Ideally we snap the center to texel units to avoid shimmering
        
        // Simple View Matrix first
        // Position light far away
        glm::vec3 lightPos = center - glm::normalize(lightDir) * -minExtents.z;
        glm::mat4 lightView = glm::lookAt(center - glm::normalize(lightDir) * radius, center, glm::vec3(0.0f, 1.0f, 0.0f));

        // Let's refine for stable cascades
        // 1. Project center to light space
        // 2. Snap to texel size
        // 3. Move center back
        
        // Texel size in world units
        float texelsPerUnit = SHADOW_MAP_DIM / (radius * 2.0f);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(texelsPerUnit));
        
        // We can just build the ortho matrix directly with snapped sizing
        // But the shimmering happens because the projection moves sub-texel.
        
        // Alternative: Build light view looking at (0,0,0), then ortho bounding box logic.
        // Let's stick to standard "Look At Center" but Snap Center.
        
        // Simplified Stable Cascade:
        // Create view matrix
        lightView = glm::lookAt(glm::vec3(0.0f), glm::normalize(lightDir), glm::vec3(0.0f, 1.0f, 0.0f)); // Look at origin from light
        
        // Transform frustum corners to Light Space
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        float minZCS = std::numeric_limits<float>::max();
        float maxZCS = std::numeric_limits<float>::lowest();
        
        for (uint32_t j = 0; j < 8; j++) {
            glm::vec4 trf = lightView * glm::vec4(frustumCorners[j], 1.0f);
            minX = std::min(minX, trf.x);
            maxX = std::max(maxX, trf.x);
            minY = std::min(minY, trf.y);
            maxY = std::max(maxY, trf.y);
            minZCS = std::min(minZCS, trf.z);
            maxZCS = std::max(maxZCS, trf.z);
        }

        // Snap min/max to texel size
        // Pixel size in world units 
        // We assume the ortho width/height will be (maxX - minX) etc.
        // To stabilize, we fix the width/height to the worst case (diagonal of frustum) or just snap the bounds.
        // Let's snap bounds.
        
        float unitPerTexel = (maxX - minX) / SHADOW_MAP_DIM; // This changes as frustum changes size?
        // Better: Use sphere radius for constant size
        // Use 'radius' calculated before (bounding sphere of frustum slice)
        
        texelsPerUnit = SHADOW_MAP_DIM / (radius * 2.0f); // Fix resolution density
        
        // Re-calculate view based on snapped center
        // Or just apply snap to the projection bounds?
        
        // Let's use the Bounding Sphere approach for absolute stability dealing with rotation
        // Center has been calculated.
        
        lightView = glm::lookAt(center - glm::normalize(lightDir) * radius, center, glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Shadow Matrix
        // P * V
        
        // Quantize the view origin to texels?
        // Only needed if the light/camera moves.
        // Let's do Bounding Box approach with snapping.
        
        // Recalculate light view based on simple direction
        lightView = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f) + glm::normalize(lightDir), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Find AABB in Light Space
        minX = std::numeric_limits<float>::max(); maxX = std::numeric_limits<float>::lowest();
        minY = std::numeric_limits<float>::max(); maxY = std::numeric_limits<float>::lowest();
        minZCS = std::numeric_limits<float>::max(); maxZCS = std::numeric_limits<float>::lowest();
        
        for (uint32_t j = 0; j < 8; j++) {
             glm::vec4 v = lightView * glm::vec4(frustumCorners[j], 1.0f);
             minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
             minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
             minZCS = std::min(minZCS, v.z); maxZCS = std::max(maxZCS, v.z);
        }
        
        // Snap
        float worldUnitsPerTexel = (maxX - minX) / SHADOW_MAP_DIM;
        minX = std::floor(minX / worldUnitsPerTexel) * worldUnitsPerTexel;
        maxX = std::floor(maxX / worldUnitsPerTexel) * worldUnitsPerTexel;
        minY = std::floor(minY / worldUnitsPerTexel) * worldUnitsPerTexel;
        maxY = std::floor(maxY / worldUnitsPerTexel) * worldUnitsPerTexel;

        // Ortho
        // Z range needs to cover casters in front of the frustum too
        float zMult = 10.0f;
        if (minZCS < 0) minZCS *= zMult; else minZCS /= zMult;
        if (maxZCS < 0) maxZCS /= zMult; else maxZCS *= zMult;
        
        glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZCS - 100.0f, maxZCS + 100.0f); // Add buffer
        lightProj[1][1] *= -1; // Vulkan flip Y

        cascades[i].viewProjMatrix = lightProj * lightView;

        lastSplitDist = splitDist;
    }
}
