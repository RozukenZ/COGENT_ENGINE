#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

class GBuffer {
public:
    struct Attachment {
        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
        VkFormat format;
    };

    // Index attachment untuk shader layout(location = X)
    enum AttachmentIndex {
        ALBEDO = 0,   // Warna & Roughness
        NORMAL = 1,   // World Space Normals
        VELOCITY = 2, // Pergerakan pixel (Penting untuk TAA)
        DEPTH = 3,    // Depth information
        COUNT
    };

    // Initialize G-Buffer
    void init(VkDevice device, VkPhysicalDevice physDevice, uint32_t width, uint32_t height);
    void cleanup(VkDevice device);
    // Getters
    VkRenderPass getRenderPass() { return renderPass; }
    VkFramebuffer getFramebuffer() { return framebuffer; }
    VkImage getAlbedoImage() { return attachments[0].image; }
    VkImage getNormalImage() { return attachments[1].image; }
    VkImageView getAlbedoView() { return attachments[0].view; }
    VkImageView getNormalView() { return attachments[1].view; }

private:
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
    std::vector<Attachment> attachments;

    // Helper untuk membuat attachment secara modular
    void createAttachment(VkDevice device, VkPhysicalDevice physDevice, uint32_t width, uint32_t height, 
                          VkFormat format, VkImageUsageFlags usage, Attachment& attachment);
};