#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <random>
#include <glm/glm.hpp>

#include "../Core/Graphics/GraphicsDevice.hpp"
#include "../Core/Graphics/PipelineCache.hpp"

namespace Cogent {
namespace Renderer {

class SSAO {
public:
    SSAO(GraphicsDevice& device, PipelineCache& pipelineCache, VkExtent2D extent);
    ~SSAO();

    void init();
    void resize(VkExtent2D newExtent);

    // Getters for integration
    VkImage getSSAOOutputImage() const { return ssaoBlurImage; }
    VkImageView getSSAOOutputView() const { return ssaoBlurView; }
    
    // Updates descriptors with GBuffer views
    void updateDescriptorSets(VkImageView positionView, VkImageView normalView, VkBuffer globalUbo);

    // Execute SSAO Passes
    void executeSSAO(VkCommandBuffer cmd);
    void executeSSAOBlur(VkCommandBuffer cmd);

private:
    void createResources();
    void createNoiseTexture();
    void createPipelines();
    void createDescriptorSets();
    void cleanupResources();
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    // Helper for generating SSAO kernel
    void generateKernel();

    GraphicsDevice& m_device;
    PipelineCache& m_pipelineCache;
    VkExtent2D m_extent;

    // Kernel & Noise
    std::vector<glm::vec4> ssaoKernel; // std140 array of vec4
    VkImage noiseImage = VK_NULL_HANDLE;
    VkDeviceMemory noiseMemory = VK_NULL_HANDLE;
    VkImageView noiseView = VK_NULL_HANDLE;

    // SSAO Raw Target
    VkImage ssaoRawImage = VK_NULL_HANDLE;
    VkDeviceMemory ssaoRawMemory = VK_NULL_HANDLE;
    VkImageView ssaoRawView = VK_NULL_HANDLE;

    // SSAO Blur Target (Final Mask)
    VkImage ssaoBlurImage = VK_NULL_HANDLE;
    VkDeviceMemory ssaoBlurMemory = VK_NULL_HANDLE;
    VkImageView ssaoBlurView = VK_NULL_HANDLE;

    // Pipelines
    VkPipelineLayout ssaoPipelineLayout = VK_NULL_HANDLE;
    VkPipeline ssaoPipeline = VK_NULL_HANDLE;

    VkPipelineLayout blurPipelineLayout = VK_NULL_HANDLE;
    VkPipeline blurPipeline = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorSetLayout ssaoDescriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout blurDescriptorLayout = VK_NULL_HANDLE;
    
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet ssaoDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet blurDescriptorSet = VK_NULL_HANDLE;

    VkSampler noiseSampler = VK_NULL_HANDLE;
    VkSampler defaultSampler = VK_NULL_HANDLE; // For GBuffer & blur sampling

    // Uniform buffer for SSAO Kernel
    VkBuffer kernelBuffer = VK_NULL_HANDLE;
    VkDeviceMemory kernelBufferMemory = VK_NULL_HANDLE;
};

} // namespace Renderer
} // namespace Cogent
