#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include "../Core/Graphics/GraphicsDevice.hpp"

class GBuffer {
public:
    struct FramebufferAttachment {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    GBuffer(GraphicsDevice& device, uint32_t width, uint32_t height);
    ~GBuffer();

    void resize(uint32_t width, uint32_t height);

    VkRenderPass getRenderPass() const { return renderPass; }
    VkFramebuffer getFramebuffer() const { return framebuffer; }
    
    // G-Buffer Attachment Accessors
    VkImageView getPositionView() const { return position.view; }
    VkImageView getNormalView() const { return normal.view; }
    VkImageView getAlbedoView() const { return albedo.view; }

    // Compatibility Getters
    VkImageView getPositionImageView() const { return position.view; }
    VkImageView getNormalImageView() const { return normal.view; }
    VkImageView getAlbedoImageView() const { return albedo.view; }
    VkImageView getDepthImageView() const { return depth.view; }
    VkImage getDepthImage() const { return depth.image; } // New Getter
    
    // Low-level access for barriers
    VkImage getPositionImage() const { return position.image; }
    VkImage getNormalImage() const { return normal.image; }
    VkImage getAlbedoImage() const { return albedo.image; }
    
    // Get specific attachment view (0=Pos, 1=Norm, 2=Albedo, 3=Depth)
    VkImageView getImageView(int index) const {
        if (index >= 0 && index < attachments.size()) {
            return attachments[index].view;
        }
        return VK_NULL_HANDLE;
    }
    
    // Get Sampler
    VkSampler getSampler() const { return sampler; } // Added Getter

    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
    VkDescriptorSet getDescriptorSet() const { return descriptorSet; }

private:
    void init();
    void cleanup();
    void createAttachment(VkFormat format, VkImageUsageFlags usage, FramebufferAttachment& attachment);

    GraphicsDevice& device;
    uint32_t width, height;

    VkRenderPass renderPass;
    VkFramebuffer framebuffer;

    // Attachments: Position, Normal, Albedo
    FramebufferAttachment position, normal, albedo, depth;
    std::vector<FramebufferAttachment> attachments; // Added to support getImageView(index)
    VkSampler sampler; // Single sampler for all attachments

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
};