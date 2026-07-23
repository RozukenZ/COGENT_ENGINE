#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include "../Core/Graphics/GraphicsDevice.hpp"
#include "../Core/Graphics/ShaderSystem.hpp"
#include "../Core/Graphics/PipelineCache.hpp"

namespace Cogent {
namespace Renderer {

class HDRPipeline {
public:
    HDRPipeline(GraphicsDevice& device, Graphics::ShaderSystem& shaderSystem, PipelineCache& pipelineCache);
    ~HDRPipeline();

    void init(VkExtent2D extent, VkRenderPass swapchainRenderPass);
    void resize(VkExtent2D newExtent);

    // HDR Target properties
    VkImage getHDRImage() const { return hdrImage; }
    VkImageView getHDRView() const { return hdrView; }
    VkFormat getHDRFormat() const { return VK_FORMAT_R16G16B16A16_SFLOAT; }
    VkRenderPass getHDRRenderPass() const { return hdrRenderPass; }
    VkFramebuffer getHDRFramebuffer() const { return hdrFramebuffer; }

    // Tonemapped Output (Final LDR image for Editor)
    VkImage getTonemappedImage() const { return tonemappedImage; }
    VkImageView getTonemappedView() const { return tonemappedView; }
    VkRenderPass getTonemapRenderPass() const { return tonemapRenderPass; }
    VkFramebuffer getTonemapFramebuffer() const { return tonemapFramebuffer; }

    // Execute Bloom Compute Shader
    void executeBloom(VkCommandBuffer cmd);

    // Execute Tonemapping to off-screen buffer
    void executeTonemap(VkCommandBuffer cmd, VkExtent2D extent);

private:
    void createHDRTarget(VkExtent2D extent);
    void createBloomResources(VkExtent2D extent);
    void createTonemapTarget(VkExtent2D extent);
    void createPipelines();
    void createDescriptorSets();
    void cleanupResources();

    GraphicsDevice& m_device;
    Graphics::ShaderSystem& m_shaderSystem;
    PipelineCache& m_pipelineCache;
    VkExtent2D m_extent;

    // HDR Render Target
    VkImage hdrImage = VK_NULL_HANDLE;
    VkImageView hdrView = VK_NULL_HANDLE;
    VmaAllocation hdrAlloc = VK_NULL_HANDLE;
    VkRenderPass hdrRenderPass = VK_NULL_HANDLE;
    VkFramebuffer hdrFramebuffer = VK_NULL_HANDLE;

    // Tonemap Target (Off-screen LDR)
    VkImage tonemappedImage = VK_NULL_HANDLE;
    VkImageView tonemappedView = VK_NULL_HANDLE;
    VmaAllocation tonemappedAlloc = VK_NULL_HANDLE;
    VkRenderPass tonemapRenderPass = VK_NULL_HANDLE;
    VkFramebuffer tonemapFramebuffer = VK_NULL_HANDLE;

    // Bloom Resources
    VkImage bloomImage = VK_NULL_HANDLE;
    VkImageView bloomView = VK_NULL_HANDLE;
    VmaAllocation bloomAlloc = VK_NULL_HANDLE;
    std::vector<VkImageView> bloomMipViews;
    uint32_t bloomMipLevels;

    // Pipelines
    VkPipelineLayout bloomPipelineLayout = VK_NULL_HANDLE;
    VkPipeline bloomDownsamplePipeline = VK_NULL_HANDLE;
    VkPipeline bloomUpsamplePipeline = VK_NULL_HANDLE;

    VkPipelineLayout tonemapPipelineLayout = VK_NULL_HANDLE;
    VkPipeline tonemapPipeline = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorSetLayout bloomDescriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> bloomDownsampleSets;
    std::vector<VkDescriptorSet> bloomUpsampleSets;

    VkDescriptorSetLayout tonemapDescriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSet tonemapDescriptorSet = VK_NULL_HANDLE;

    VkSampler linearSampler = VK_NULL_HANDLE;
};

} // namespace Renderer
} // namespace Cogent
