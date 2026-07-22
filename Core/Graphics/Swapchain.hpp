#pragma once
#include "GraphicsDevice.hpp"
#include <vector>
#include <memory> 

class Swapchain {
public:
    Swapchain(GraphicsDevice& device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    ~Swapchain();

    // Recreate swapchain on resize
    void recreate(uint32_t width, uint32_t height);

    // Getters
    VkSwapchainKHR getHandle() const { return swapchain; }
    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(images.size()); }
    VkImageView getImageView(int index) const { return imageViews[index]; }
    VkFramebuffer getFramebuffer(int index) const { return framebuffers[index]; }
    VkRenderPass getRenderPass() const { return renderPass; } // Swapchain owns the presentation renderpass

    // Frame Operations
    VkResult acquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex);
    VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex, VkSemaphore imageAvailable, VkSemaphore renderFinished, VkFence inFlightFence);

private:
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void cleanup();

    GraphicsDevice& device;
    VkSurfaceKHR surface;
    uint32_t width, height;

    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkFormat imageFormat;
    VkExtent2D extent;
    VkRenderPass renderPass; // Simple pass for presenting to screen (clearing/blitting)
};
