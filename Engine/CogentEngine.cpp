#include "CogentEngine.hpp"
#include <set>
#include <algorithm>
#include <fstream>
#include "../Resources/ResourceManager.hpp"

// Statically link the callback for GLFW
void CogentEngine::mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    auto app = reinterpret_cast<CogentEngine*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    // [New] Only rotate if Right Mouse Button is held
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
        app->firstMouse = true; // Reset delta when not holding right click
        app->lastX = static_cast<float>(xposIn);
        app->lastY = static_cast<float>(yposIn);
        return; 
    }

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (app->firstMouse) {
        app->lastX = xpos;
        app->lastY = ypos;
        app->firstMouse = false;
    }

    float xoffset = xpos - app->lastX;
    float yoffset = app->lastY - ypos; 

    app->lastX = xpos;
    app->lastY = ypos;

    app->mainCamera.processMouseMovement(xoffset, yoffset);
}

// Constructor
CogentEngine::CogentEngine() 
    : graphicsDevice(true), 
      gBuffer(graphicsDevice, WIDTH, HEIGHT) 
{
    // Initialize other members if needed
}

void CogentEngine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<CogentEngine*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void CogentEngine::run() {
    // [NEW] Initialize Job System
    Cogent::Threading::JobSystem::Get().Initialize();
    LOG_INFO("Job System Initialized");

    initWindow();
    initVulkan();
    initResources();
    
    LOG_INFO("--- DIAGNOSTIC CHECK ---");
    // ... (Keep original diagnostic logs if needed)
    
    LOG_INFO("--- ALL SYSTEMS GO ---");

    try {
        mainLoop();
    } catch (const std::exception& e) {
        LOG_ERROR("CRITICAL EXCEPTION IN MAIN LOOP: " + std::string(e.what()));
        std::cerr << "CRITICAL EXCEPTION: " << e.what() << std::endl;
        int x; std::cin >> x; 
    } catch (...) { // Turbo catch
        LOG_ERROR("CRITICAL UNKNOWN EXCEPTION IN MAIN LOOP");
    }

    cleanup();
}

void CogentEngine::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Or TRUE if you handle resize
    window = glfwCreateWindow(WIDTH, HEIGHT, "COGENT Engine (Phase 0: Foundation)", nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("FATAL: Failed to create GLFW Window! Check if GPU drivers support Vulkan.");
    }
    
    glfwSetWindowUserPointer(window, this); // Important for callbacks
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void CogentEngine::initVulkan() {
    // Instance created in GraphicsDevice constructor
    createSurface();
    graphicsDevice.init(surface);
    createDescriptorSetLayout();
    // Command Pool created in GraphicsDevice
    createCommandBuffer();
    createSyncObjects();
    createSwapchain();
    createSwapchainImageViews();
}

void CogentEngine::createSurface() {
    if (glfwCreateWindowSurface(graphicsDevice.getInstance(), window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat Window Surface!");
    }
}

void CogentEngine::initResources() {
    LOG_INFO("Initializing G-Buffer...");
    // gBuffer is initialized in constructor, but we need to trigger init() explicitly now
    gBuffer.init();
    
    // Initialize Streamer
    streamer = std::make_unique<Cogent::Resources::Streamer>(graphicsDevice);
    
    // Initialize ResourceManager with Streamer
    Cogent::Resources::ResourceManager::Get().Init(graphicsDevice.getDevice(), graphicsDevice.getPhysicalDevice(), graphicsDevice.getCommandPool(), graphicsDevice.getGraphicsQueue(), streamer.get());
    
    createUniformBuffer();
    createDescriptorPool();
    createDescriptorSets();
    
    LOG_INFO("Loading Default White Texture...");
    // Assuming texture path is relative to executable
    whiteTexture.load(graphicsDevice.getDevice(), graphicsDevice.getPhysicalDevice(), graphicsDevice.getCommandPool(), graphicsDevice.getGraphicsQueue(), "textures/white.png"); 
    
    createTextureDescriptors();

    LOG_INFO("Building Graphics Pipeline...");
    std::vector<VkDescriptorSetLayout> layouts = { descriptorSetLayout, textureDescriptorLayout };
    gBufferPipeline.init(graphicsDevice.getDevice(), gBuffer.getRenderPass(), {WIDTH, HEIGHT}, layouts);
    
    LOG_INFO("Generating Primitive Meshes...");
    meshes.resize(3);
    PrimitiveMesh generator;

    generator.createCube();
    meshes[0].loadFromMesh(graphicsDevice.getDevice(), graphicsDevice.getPhysicalDevice(), generator.vertices, generator.indices);

    generator.createSphere(1.0f, 32, 32);
    meshes[1].loadFromMesh(graphicsDevice.getDevice(), graphicsDevice.getPhysicalDevice(), generator.vertices, generator.indices);

    generator.createCapsule(0.5f, 2.0f, 32, 16);
    meshes[2].loadFromMesh(graphicsDevice.getDevice(), graphicsDevice.getPhysicalDevice(), generator.vertices, generator.indices);

    createTextureSampler(); 
    // createLightingDescriptors();  // REMOVED: Handled by DeferredLightingPass
    createLightingRenderPass(); 
    createSwapchainFramebuffers(); 
    
    // Initialize Deferred Lighting Pass (and internal descriptors)
    // Initialize Deferred Lighting Pass (and internal descriptors)
    // deferredLightingPass = std::make_unique<DeferredLightingPass>(graphicsDevice, lightingRenderPass, swapchainExtent);
    // deferredLightingPass->init(descriptorSetLayout); // [MODIFIED] Pass Global Layout
    // deferredLightingPass->updateDescriptorSets(gBuffer);

    // Initialize SSS
    screenSpaceShadows = std::make_unique<ScreenSpaceShadows>(graphicsDevice, swapchainExtent);
    // screenSpaceShadows->init(); // Removed: Called in constructor
    screenSpaceShadows->updateDescriptorSets(gBuffer.getDepthImageView(), textureSampler, uniformBuffer);

    LOG_INFO("Resources Initialized Successfully!");

    LOG_INFO("Initializing ImGui Editor UI...");
    
    // We need to fetch Graphics Family Index again or expose it from GraphicsDevice
    auto indices = GraphicsDevice::findQueueFamilies(graphicsDevice.getPhysicalDevice(), surface);

    editorUI.Init(window, graphicsDevice.getInstance(), graphicsDevice.getPhysicalDevice(), graphicsDevice.getDevice(), 
                indices.graphicsFamily.value(),
                graphicsDevice.getGraphicsQueue(), lightingRenderPass, 2);

    sceneDescriptorSet = ImGui_ImplVulkan_AddTexture(textureSampler, gBuffer.getAlbedoView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    LOG_INFO("Initializing Ray Tracer...");
    rayTracer.init(graphicsDevice.getDevice(), graphicsDevice.getPhysicalDevice(), graphicsDevice.getCommandPool(), graphicsDevice.getGraphicsQueue(), swapchainExtent);

    LOG_INFO("Spawning Demo Scene...");
    
    // 1. Center Cube (White Texture)
    spawnObject(0, glm::vec3(0.0f, 0.0f, 0.0f)); 
    
    // 2. Surrounding Lights (Spheres with Bright Colors)
    // We use meshID 1 (Sphere) and manually set color/name
    
    // Right (Red)
    spawnObject(1, glm::vec3(2.5f, 0.0f, 0.0f));
    gameObjects.back().name = "Light_Red";
    gameObjects.back().color = glm::vec4(2.0f, 0.2f, 0.2f, 1.0f); // >1.0 for manual bloom/emissive look if supported, else just bright
    
    // Left (Blue)
    spawnObject(1, glm::vec3(-2.5f, 0.0f, 0.0f));
    gameObjects.back().name = "Light_Blue";
    gameObjects.back().color = glm::vec4(0.2f, 0.2f, 2.0f, 1.0f);

    // Front (Green)
    spawnObject(1, glm::vec3(0.0f, 2.5f, 0.0f)); // Y is Up/Forward depending on coord system. Vulkan Y is down, Z is depth? 
    // Let's assume Z is depth for now based on previous camera setup
    gameObjects.back().name = "Light_Green";
    gameObjects.back().color = glm::vec4(0.2f, 2.0f, 0.2f, 1.0f);

    // Back (Yellow) - Adjusted position to be visible
    spawnObject(1, glm::vec3(0.0f, -2.5f, 0.0f));
    gameObjects.back().name = "Light_Yellow";
    gameObjects.back().color = glm::vec4(2.0f, 2.0f, 0.2f, 1.0f);
    
    // Sun
    spawnObject(3, glm::vec3(0.0f, 5.0f, 5.0f));
}

void CogentEngine::spawnObject(int meshID, glm::vec3 position) {
    LOG_INFO("Attempting to Spawn Object with MeshID: " + std::to_string(meshID));
    GameObject obj;
    obj.id = (int)gameObjects.size();
    
    if (meshID < 0 || meshID >= meshes.size()) {
        LOG_ERROR("Invalid MeshID: " + std::to_string(meshID));
        return;
    }

    obj.meshID = meshID; 

    if (meshID == 0) obj.name = "Cube " + std::to_string(obj.id);
    else if (meshID == 1) obj.name = "Sphere " + std::to_string(obj.id);
    else if (meshID == 2) obj.name = "Capsule " + std::to_string(obj.id);
    else if (meshID == 3) { 
        obj.name = "Sun";
        obj.color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); 
        obj.meshID = 1; 
    }
    else obj.name = "Object " + std::to_string(obj.id);
    
    obj.model = glm::translate(glm::mat4(1.0f), position);
    obj.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); 

    gameObjects.push_back(obj);
    selectedObjectIndex = obj.id;
    
    LOG_INFO("Spawned Object: " + obj.name);
}

void CogentEngine::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Update Streamer
        glm::vec3 camPos = mainCamera.position; 
        streamer->update(camPos, deltaTime);

        glfwPollEvents();

        if (currentState != AppState::EDITOR) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            showCursor = true;
        }
        else {
            // [New] Editor Mode: Always show cursor, allow movement
            showCursor = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            
            // Allow keyboard movement even if cursor is visible (WASD)
            // But NOT if ImGui wants input (typing in text box)
            if (!ImGui::GetIO().WantCaptureKeyboard) {
                mainCamera.processKeyboard(window, deltaTime);
            }
        }

        editorUI.Update(currentState, showCursor, deltaTime, mainCamera, selectedObject, gameObjects, selectedObjectIndex, sceneDescriptorSet, [&](int meshID) {
            spawnObject(meshID, glm::vec3(0, 0, 0)); 
        }, &renderingViewportSize, { (float)swapchainExtent.width, (float)swapchainExtent.height });

        LOG_INFO("MainLoop: EditorUI::Update returned");

        if (currentState == AppState::EDITOR && selectedObjectIndex != -1) {
            if (selectedObjectIndex >= 0 && selectedObjectIndex < gameObjects.size()) {
                 gameObjects[selectedObjectIndex].model = selectedObject.model;
                 gameObjects[selectedObjectIndex].color = selectedObject.color;
            }
        }

        if (currentState == AppState::EDITOR) {
            LOG_INFO("MainLoop: Calling updateUniformBuffer");
            updateUniformBuffer();
            LOG_INFO("MainLoop: updateUniformBuffer returned");
        }

        LOG_INFO("MainLoop: Calling drawFrame");
        drawFrame();
        LOG_INFO("MainLoop: drawFrame returned");
    } // End of main loop
    vkDeviceWaitIdle(graphicsDevice.getDevice());
}

void CogentEngine::cleanup() {
    LOG_INFO("Cleaning up resources...");
    vkDeviceWaitIdle(graphicsDevice.getDevice());
    
    // [NEW] Shutdown Job System
    Cogent::Threading::JobSystem::Get().Shutdown();

    vkDestroySemaphore(graphicsDevice.getDevice(), renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(graphicsDevice.getDevice(), imageAvailableSemaphore, nullptr);
    vkDestroyFence(graphicsDevice.getDevice(), inFlightFence, nullptr);

    // deferredLightingPass destructor handles cleanup (via RAII or manual call if needed)
    // But destructors are better. `LightingPass` had explicit cleanup. `DeferredLightingPass` has destructor.
    // However, we need to ensure resources are destroyed before Device.
    
    // Check if DeferredLightingPass has cleanup? It has destructor.
    // ScreenSpaceShadows has destructor.
    
    vkDestroySampler(graphicsDevice.getDevice(), textureSampler, nullptr);
    for (auto framebuffer : swapchainFramebuffers) {
        vkDestroyFramebuffer(graphicsDevice.getDevice(), framebuffer, nullptr);
    }
    vkDestroyRenderPass(graphicsDevice.getDevice(), lightingRenderPass, nullptr);

    lightingRenderPass = VK_NULL_HANDLE; // Will be destroyed

    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(graphicsDevice.getDevice(), imageView, nullptr);
    }
    vkDestroySwapchainKHR(graphicsDevice.getDevice(), swapchain, nullptr);

    // Command Pool destroyed by GraphicsDevice
    // vkDestroyCommandPool(device, commandPool, nullptr); 

    // gBuffer cleanup handled by destructor
    rayTracer.cleanup(graphicsDevice.getDevice());
    myModel.cleanup(graphicsDevice.getDevice());

    vkDestroyDescriptorPool(graphicsDevice.getDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(graphicsDevice.getDevice(), descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(graphicsDevice.getDevice(), textureDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(graphicsDevice.getDevice(), textureDescriptorLayout, nullptr);
    myTexture.cleanup(graphicsDevice.getDevice());
    vkDestroyBuffer(graphicsDevice.getDevice(), uniformBuffer, nullptr);
    vkFreeMemory(graphicsDevice.getDevice(), uniformBufferMemory, nullptr);

    // Device destroyed by GraphicsDevice destructor
    // vkDestroyDevice(device, nullptr); 
    vkDestroySurfaceKHR(graphicsDevice.getInstance(), surface, nullptr);
    // vkDestroyInstance(instance, nullptr);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(graphicsDevice.getDevice(), imguiPool, nullptr);
    editorUI.Cleanup(graphicsDevice.getDevice());

    glfwDestroyWindow(window);
    glfwTerminate();
}



// Implementations of Core Helpers
// Core Helpers Removed (Moved to GraphicsDevice)

void CogentEngine::createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = graphicsDevice.getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(graphicsDevice.getDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Gagal mengalokasikan Command Buffer!");
    }
}

void CogentEngine::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(graphicsDevice.getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(graphicsDevice.getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(graphicsDevice.getDevice(), &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat objek sinkronisasi!");
    }
}

void CogentEngine::createSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(graphicsDevice.getPhysicalDevice(), surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(graphicsDevice.getPhysicalDevice(), surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(graphicsDevice.getPhysicalDevice(), surface, &formatCount, formats.data());
    
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
            break;
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; 

    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent = {WIDTH, HEIGHT};
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; 

    QueueFamilyIndices indices = GraphicsDevice::findQueueFamilies(graphicsDevice.getPhysicalDevice(), surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.graphicsFamily.value()}; 

    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(graphicsDevice.getDevice(), &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat swapchain!");
    }

    vkGetSwapchainImagesKHR(graphicsDevice.getDevice(), swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(graphicsDevice.getDevice(), swapchain, &imageCount, swapchainImages.data());

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
}

void CogentEngine::createSwapchainImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(graphicsDevice.getDevice(), &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat image views swapchain!");
    }
}
} // End of createSwapchainImageViews

void CogentEngine::recreateSwapchain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(graphicsDevice.getDevice());

    for (auto framebuffer : swapchainFramebuffers) {
        vkDestroyFramebuffer(graphicsDevice.getDevice(), framebuffer, nullptr);
    }
    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(graphicsDevice.getDevice(), imageView, nullptr);
    }
    vkDestroySwapchainKHR(graphicsDevice.getDevice(), swapchain, nullptr);
    
    // gBuffer cleanup handled by destructor/resize

    createSwapchain();
    createSwapchainImageViews();
    createSwapchainFramebuffers();

    gBuffer.resize(swapchainExtent.width, swapchainExtent.height);

    sceneDescriptorSet = ImGui_ImplVulkan_AddTexture(textureSampler, gBuffer.getAlbedoView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void CogentEngine::createLightingRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(graphicsDevice.getDevice(), &renderPassInfo, nullptr, &lightingRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat Lighting Render Pass!");
    }
}

void CogentEngine::createSwapchainFramebuffers() {
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkImageView attachments[] = { swapchainImageViews[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = lightingRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(graphicsDevice.getDevice(), &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat Swapchain Framebuffer!");
        }
    }
}

void CogentEngine::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(graphicsDevice.getDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat Texture Sampler!");
    }
}

// createLightingDescriptors REMOVED (Handled by DeferredLightingPass)

void CogentEngine::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; 
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(graphicsDevice.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat descriptor set layout!");
    }
}

void CogentEngine::createUniformBuffer() {
    VkDeviceSize bufferSize = sizeof(CameraUBO);
    createBuffer(bufferSize, 
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 uniformBuffer, uniformBufferMemory);
    vkMapMemory(graphicsDevice.getDevice(), uniformBufferMemory, 0, bufferSize, 0, &uniformBufferMapped);
}

void CogentEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(graphicsDevice.getDevice(), &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(graphicsDevice.getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    
    // We assume findMemoryType is available as a helper or logic from VulkanUtils/Model
    // For now, implementing simple search here to be self-contained in Engine file
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(graphicsDevice.getPhysicalDevice(), &memProperties);

    uint32_t typeIndex = -1;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            typeIndex = i;
            break;
        }
    }
    if (typeIndex == -1) throw std::runtime_error("Failed to find suitable memory type!");
    
    allocInfo.memoryTypeIndex = typeIndex;

    vkAllocateMemory(graphicsDevice.getDevice(), &allocInfo, nullptr, &bufferMemory);
    vkBindBufferMemory(graphicsDevice.getDevice(), buffer, bufferMemory, 0);
}

void CogentEngine::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(graphicsDevice.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Gagal membuat descriptor pool!");
    }
}

void CogentEngine::createDescriptorSets() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool; 
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout; 

    if (vkAllocateDescriptorSets(graphicsDevice.getDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer;      
    bufferInfo.offset = 0;                  
    bufferInfo.range = sizeof(CameraUBO);   

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;         
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; 
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo; 

    vkUpdateDescriptorSets(graphicsDevice.getDevice(), 1, &descriptorWrite, 0, nullptr);
}

void CogentEngine::updateUniformBuffer() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    CameraUBO ubo{};
    ubo.view = mainCamera.getViewMatrix();
    ubo.proj = mainCamera.getProjectionMatrix(renderingViewportSize.x / renderingViewportSize.y);
    ubo.proj[1][1] *= -1; // Vulkan flip Y

    // For TAA, we should store previous matrices. For now, just use current.
    static glm::mat4 lastView = ubo.view;
    static glm::mat4 lastProj = ubo.proj;
    ubo.prevView = lastView;
    ubo.prevProj = lastProj;
    
    lastView = ubo.view;
    lastProj = ubo.proj;

    ubo.viewPos = mainCamera.position;
    ubo.time = time;
    ubo.deltaTime = deltaTime;

    memcpy(uniformBufferMapped, &ubo, sizeof(ubo));
}

void CogentEngine::createTextureDescriptors() {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1; 
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr; 
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(graphicsDevice.getDevice(), &layoutInfo, nullptr, &textureDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("FATAL ERROR: Failed to create Texture Descriptor Layout!");
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1; 

    if (vkCreateDescriptorPool(graphicsDevice.getDevice(), &poolInfo, nullptr, &textureDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("FATAL ERROR: Failed to create Texture Descriptor Pool!");
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = textureDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureDescriptorLayout;

    if (vkAllocateDescriptorSets(graphicsDevice.getDevice(), &allocInfo, &textureDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("FATAL ERROR: Failed to allocate Texture Descriptor Set!");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = whiteTexture.getImageView();
    imageInfo.sampler = whiteTexture.getSampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = textureDescriptorSet;
    descriptorWrite.dstBinding = 0; 
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(graphicsDevice.getDevice(), 1, &descriptorWrite, 0, nullptr);
}

void CogentEngine::drawFrame() {
    LOG_INFO("drawFrame: Waiting for Fences");
    vkWaitForFences(graphicsDevice.getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    LOG_INFO("drawFrame: Fences Ready");

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(graphicsDevice.getDevice(), swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    LOG_INFO("drawFrame: Image Acquired Index: " + std::to_string(imageIndex));

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return; 
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    LOG_INFO("drawFrame: Resetting Fences");
    vkResetFences(graphicsDevice.getDevice(), 1, &inFlightFence);
    
    LOG_INFO("drawFrame: Resetting Command Buffer");
    vkResetCommandBuffer(commandBuffer, 0);
    
    LOG_INFO("drawFrame: Recording Command Buffer");
    recordCommandBuffer(commandBuffer, imageIndex);
    LOG_INFO("drawFrame: Command Buffer Recorded");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    LOG_INFO("drawFrame: Submitting Queue");
    if (vkQueueSubmit(graphicsDevice.getGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }
    LOG_INFO("drawFrame: Queue Submitted");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(graphicsDevice.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }
}

void CogentEngine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to start recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = gBuffer.getRenderPass();
    renderPassInfo.framebuffer = gBuffer.getFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;

    std::array<VkClearValue, 4> clearValues{};
    clearValues[0].color = {{0.53f, 0.81f, 0.92f, 1.0f}}; 
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; 
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; 
    clearValues[3].depthStencil = {1.0f, 0};           

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    rayTracer.render(commandBuffer, VK_NULL_HANDLE, mainCamera, static_cast<float>(glfwGetTime()));

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchainExtent.width;
        viewport.height = (float)swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gBufferPipeline.getPipeline());
        
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            gBufferPipeline.getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            gBufferPipeline.getPipelineLayout(), 1, 1, &textureDescriptorSet, 0, nullptr);


        for (const auto& obj : gameObjects) {
            // LOG_INFO("recordCommandBuffer: Drawing Object " + obj.name);
            ObjectPushConstant pc = obj.getPushConstant(); 
            vkCmdPushConstants(
                commandBuffer, 
                gBufferPipeline.getPipelineLayout(), 
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                0, 
                sizeof(ObjectPushConstant), 
                &pc
            );

            if (obj.id >= 0 && obj.id < meshes.size()) {
                meshes[obj.id].draw(commandBuffer);
            } else {
                 LOG_ERROR("Invalid Mesh ID: " + std::to_string(obj.id));
            }
        }

    vkCmdEndRenderPass(commandBuffer);

    std::array<VkImageMemoryBarrier, 2> barriers{};

    if (!screenSpaceShadows) {
        LOG_ERROR("FATAL: screenSpaceShadows is NULL!");
        throw std::runtime_error("screenSpaceShadows is NULL");
    }

    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 
    barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = gBuffer.getAlbedoImage(); 
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = gBuffer.getNormalImage();
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         
        0,
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(barriers.size()), barriers.data()
    );

    VkRenderPassBeginInfo lightRenderPassInfo{};
    lightRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    lightRenderPassInfo.renderPass = lightingRenderPass;
    lightRenderPassInfo.framebuffer = swapchainFramebuffers[imageIndex]; 
    lightRenderPassInfo.renderArea.offset = {0, 0};
    lightRenderPassInfo.renderArea.extent = swapchainExtent;

    VkClearValue clearColor = {};
    clearColor.color = {{0.53f, 0.81f, 0.92f, 1.0f}}; 
    lightRenderPassInfo.clearValueCount = 1;
    lightRenderPassInfo.pClearValues = &clearColor;

        // Explicit Memory Barrier to ensure GBuffer write is done before SSS read (already partly handled by subpass dependencies or external barriers)
        // Note: The previous barriers (lines 1039-1077 in original file) handle transition to SHADER_READ_ONLY_OPTIMAL for Fragment Shader. 
        // SSS uses Compute Shader. We might need VK_IMAGE_LAYOUT_GENERAL or SHADER_READ_ONLY_OPTIMAL compatible with Storage Image or Sampled Image.
        // SSS samples depth (should be DepthReadOnly) and writes to shadow mask (General/Storage).
        
        // --- 1. Screen Space Shadows Pass ---
        // SSS reads Depth, writes to Shadow Mask.
        // Need to ensure Depth is readable. (Depth Attachment Store Op was STORE, Layout DEPTH_STENCIL_ATTACHMENT_OPTIMAL or similar)
        // Transition Depth to Shader Read Only if needed, or use Combined Image Sampler layout.
        
        // Transition SSS Image to GENERAL for Compute Write
        VkImageMemoryBarrier sssImageBarrier{};
        sssImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        sssImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
        sssImageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        sssImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        sssImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        sssImageBarrier.image = screenSpaceShadows->getImage(); 
        sssImageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        sssImageBarrier.subresourceRange.baseMipLevel = 0;
        sssImageBarrier.subresourceRange.levelCount = 1;
        sssImageBarrier.subresourceRange.baseArrayLayer = 0;
        sssImageBarrier.subresourceRange.layerCount = 1;
        sssImageBarrier.srcAccessMask = 0;
        sssImageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        // Ensure Depth is readable by SSS (Synchronize GBuffer Write -> Compute Read)
        VkImageMemoryBarrier depthBarrier{};
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // No layout change needed
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = gBuffer.getDepthImage(); // Need getter
        depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthBarrier.subresourceRange.baseMipLevel = 0;
        depthBarrier.subresourceRange.levelCount = 1;
        depthBarrier.subresourceRange.baseArrayLayer = 0;
        depthBarrier.subresourceRange.layerCount = 1;
        depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkImageMemoryBarrier sssBarriers[] = { sssImageBarrier, depthBarrier };

        if (sssImageBarrier.image == VK_NULL_HANDLE) LOG_ERROR("FATAL: SSS Image is NULL!");
        if (depthBarrier.image == VK_NULL_HANDLE) LOG_ERROR("FATAL: Depth Image is NULL!");

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            2, sssBarriers
        );

        // Dispatch SSS:
        // Use a fixed light direction for now (e.g. from top-right-front)
        glm::vec4 lightDir = glm::vec4(normalize(glm::vec3(0.5f, -1.0f, 0.2f)), 0.0f);
        screenSpaceShadows->execute(commandBuffer, mainCamera.getViewMatrix(), mainCamera.getProjectionMatrix(), lightDir);

        // Memory Barrier: Ensure SSS Write finishes before Lighting Pass reads it? 
        // (If Lighting Pass reads shadow mask)
        VkMemoryBarrier sssBarrier{};
        sssBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        sssBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sssBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &sssBarrier, 0, nullptr, 0, nullptr);

        // --- 2. Deferred Lighting Pass ---

        if (lightRenderPassInfo.renderPass == VK_NULL_HANDLE) LOG_ERROR("FATAL: Lighting RenderPass is NULL!");
        if (lightRenderPassInfo.framebuffer == VK_NULL_HANDLE) LOG_ERROR("FATAL: Lighting Framebuffer is NULL!");

        if (lightRenderPassInfo.renderPass == VK_NULL_HANDLE) LOG_ERROR("FATAL: Lighting RenderPass is NULL!");
        if (lightRenderPassInfo.framebuffer == VK_NULL_HANDLE) LOG_ERROR("FATAL: Lighting Framebuffer is NULL!");

        vkCmdBeginRenderPass(commandBuffer, &lightRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewportFullscreen{};
        
        float vpWidth = renderingViewportSize.x;
        float vpHeight = renderingViewportSize.y;
        
        if(vpWidth > swapchainExtent.width) vpWidth = (float)swapchainExtent.width;
        if(vpHeight > swapchainExtent.height) vpHeight = (float)swapchainExtent.height;
        if(vpWidth < 1.0f) vpWidth = 1.0f; 
        if(vpHeight < 1.0f) vpHeight = 1.0f;

        viewportFullscreen.width = vpWidth;
        viewportFullscreen.height = vpHeight;
        viewportFullscreen.minDepth = 0.0f;
        viewportFullscreen.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewportFullscreen);

        VkRect2D scissorFullscreen{};
        scissorFullscreen.offset = {0, 0};
        scissorFullscreen.extent = {(uint32_t)vpWidth, (uint32_t)vpHeight};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissorFullscreen);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissorFullscreen);

        // Execute Lighting Pass (Fullscreen Quad)
        // Pass the Global UBO (Camera) descriptor set if needed, or scene descriptor.
        // The DeferredLightingPass::execute signature might need checking. 
        // Checked header: void execute(VkCommandBuffer cmd, VkDescriptorSet sceneGlobalDescSet);
        if (false /*!deferredLightingPass*/) {
             LOG_ERROR("FATAL: deferredLightingPass is NULL!");
             throw std::runtime_error("deferredLightingPass is NULL");
        }
        
        // deferredLightingPass->execute(commandBuffer, descriptorSet); // descriptorSet is UBO set

        editorUI.Draw(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to stop recording command buffer!");
    }
}
