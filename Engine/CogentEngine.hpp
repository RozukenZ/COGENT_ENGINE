#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Library Matematika (GLM)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <memory>
#include <optional>
#include <chrono>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

// Include modul buatan kita
#include "../RayTracing/RayTracer.hpp"
#include "../Renderer/GBuffer.hpp"
#include "../Resources/Model.hpp"
#include "../Renderer/RenderPipeline.hpp"
#include "../Core/Types.hpp"
#include "../Core/VulkanUtils.hpp"
#include "../Renderer/LightingPass.hpp"
#include "../Resources/Texture.hpp"
#include "../Core/Camera.hpp"
#include "../Editor/EditorUI.hpp"
#include "../Geometry/PrimitiveMesh.hpp"
#include "../Core/Logger.hpp"

// New Systems
#include "../Core/Memory/LinearAllocator.hpp"
#include "../Renderer/Graph/RenderGraph.hpp"
#include "../Renderer/Visibility/VisibilitySystem.hpp"
#include "../Core/Threading/JobSystem.hpp"
#include "../Core/Graphics/GraphicsDevice.hpp"
#include "../Renderer/DeferredLightingPass.hpp"
#include "../Renderer/ScreenSpaceShadows.hpp"

// QueueFamilyIndices struct is defined in GraphicsDevice.hpp

class CogentEngine {
public:
    CogentEngine();
    void run();

private:
    void initWindow();
    void initVulkan();
    void initResources();
    void mainLoop();
    void cleanup();
    void drawFrame();
    void updateCamera();
    
    // Core Vulkan Helpers
    void createSurface();
    void createDescriptorSetLayout();
    void createCommandBuffer();
    void createSyncObjects();
    void createSwapchain();
    void createSwapchainImageViews();
    void recreateSwapchain();
    
    bool isDeviceSuitable(VkPhysicalDevice device);
    // QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device); // Removed, use GraphicsDevice::findQueueFamilies
    
    // Resource Helpers
    void createUniformBuffer();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer();
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    
    // Texture Helpers
    void createTextureSampler();
    void createTextureDescriptors();
    
    // Lighting Helpers
    void createLightingRenderPass();
    void createSwapchainFramebuffers();
    void createLightingDescriptors();
    
    // Rendering Helpers
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    
    // Game Logic Helpers
    void spawnObject(int meshID, glm::vec3 position);
    
    // Static Callbacks
    static void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    // --- Member Variables (Previously Globals) ---
    
    GLFWwindow* window;
    
    // Subsystems
    GraphicsDevice graphicsDevice; // Main Graphics Device

    // Vulkan Handles (Legacy/Direct Access) - Consider removing if GraphicsDevice covers all
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue presentQueue;
    
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    
    // Rendering Subsystems
    GBuffer gBuffer;
    Model myModel;
    RenderPipeline gBufferPipeline;
    RayTracer rayTracer;
    std::unique_ptr<DeferredLightingPass> deferredLightingPass;
    std::unique_ptr<ScreenSpaceShadows> screenSpaceShadows;
    VkRenderPass lightingRenderPass;
    
    // Resources
    Texture myTexture;
    Texture whiteTexture;
    VkSampler textureSampler;
    
    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout;      // UBO Camera
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    
    VkDescriptorSetLayout textureDescriptorLayout;  // Material Textures
    VkDescriptorPool textureDescriptorPool;
    VkDescriptorSet textureDescriptorSet;
    
    VkDescriptorSetLayout lightingDescriptorLayout; // G-Buffer Input
    VkDescriptorPool lightingDescriptorPool;
    VkDescriptorSet lightingDescriptorSet;
    
    VkDescriptorPool imguiPool;
    VkDescriptorSet sceneDescriptorSet = VK_NULL_HANDLE;
    
    // Uniform Buffers
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped;
    
    // Systems
    std::unique_ptr<Cogent::Memory::LinearAllocator> frameAllocator;
    std::unique_ptr<RenderGraph> renderGraph; // Fixed namespace
    std::unique_ptr<Cogent::Renderer::VisibilitySystem> visibilitySystem;
    std::unique_ptr<Cogent::Resources::Streamer> streamer;
    
    // Scene Data
    std::vector<GameObject> gameObjects;
    std::vector<const GameObject*> visibleObjects; // Results from culling
    // Camera mainCamera; // Removed private duplicate
    
    // Game State
    AppState currentState = AppState::LOADING;
    EditorUI editorUI;
    
    std::vector<Model> meshes; 
    int selectedObjectIndex = -1;
    ObjectPushConstant selectedObject{};
    
    // Input State
    bool showCursor = false;
    bool lastCtrlState = false;
    bool firstMouse = true;
    float lastX = 1920 / 2.0f;
    float lastY = 1080 / 2.0f;
    
    // Time
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    
    // Viewport
    glm::vec2 renderingViewportSize = {1920, 1080};
    bool framebufferResized = false;
    
    // Constants
    // Constants
    static constexpr uint32_t WIDTH = 1920;
    static constexpr uint32_t HEIGHT = 1080;
    
public:
    Camera mainCamera{glm::vec3(2.0f, 2.0f, 2.0f)}; // Public for callback access if needed, or use friend/accessor
};
