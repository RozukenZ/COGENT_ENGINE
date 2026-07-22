#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include "../Core/Graphics/GraphicsDevice.hpp"

class ShadowPass {
public:
    static const int CASCADE_COUNT = 3;
    static const int SHADOW_MAP_DIM = 2048;

    struct Cascade {
        float splitDepth;
        glm::mat4 viewProjMatrix;
        VkImageView view;
        VkFramebuffer framebuffer;
    };

    ShadowPass(GraphicsDevice& device);
    ~ShadowPass();

    void init();
    void updateCascades(const glm::mat4& cameraView, const glm::mat4& cameraProj, const glm::vec3& lightDir);
    
    VkRenderPass getRenderPass() const { return renderPass; }
    const std::array<Cascade, CASCADE_COUNT>& getCascades() const { return cascades; }
    VkSampler getSampler() const { return sampler; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

private:
    void createRenderPass();
    void createResources();
    void createDescriptorSetLayout();

    GraphicsDevice& device;
    VkRenderPass renderPass;
    VkSampler sampler;
    
    // Depth Array Texture
    VkImage shadowImage;
    VkDeviceMemory shadowMemory;
    VkImageView shadowImageView; // Full array view

    std::array<Cascade, CASCADE_COUNT> cascades;
    VkDescriptorSetLayout descriptorSetLayout;
};
