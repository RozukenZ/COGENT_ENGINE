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
#include <optional>
#include <chrono>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

// Include modul buatan kita
#include "GBuffer.hpp"
#include "Model.hpp"
#include "RenderPipeline.hpp"
#include "Types.hpp"
#include "VulkanUtils.hpp"
#include "LightingPass.hpp"
#include "Resources/Texture.hpp"
#include "Core/Camera.hpp"
#include "Editor/EditorUI.hpp"


const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;

// Struktur untuk Queue Families (Graphics & Compute)
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> computeFamily; // Penting untuk AI nanti

    bool isComplete() {
        return graphicsFamily.has_value() && computeFamily.has_value();
    }
};

// Global Variables untuk Input Mouse
Camera mainCamera(glm::vec3(2.0f, 2.0f, 2.0f));
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;

// Time Variables
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Cursor Variables
bool showCursor = false;       // Status cursor saat ini
bool lastCtrlState = false;    // Untuk toggle logic

// Callback Mouse (Harus di luar class atau static)
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    // [FIX] Kalau cursor lagi muncul (mode UI), jangan putar kamera!
    if (showCursor) {
        firstMouse = true; // Reset agar tidak lompat saat balik ke mode FPS
        return; 
    }

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 

    lastX = xpos;
    lastY = ypos;

    mainCamera.processMouseMovement(xoffset, yoffset);
}

ObjectPushConstant selectedObject{};

class CogentEngine {
public:
    void run() {
        initWindow();
        initVulkan();
        initResources(); // Setup G-Buffer di sini
        // --- DIAGNOSIS POINTER  ---
        std::cout << "--- DIAGNOSTIC CHECK ---" << std::endl;
        std::cout << "Device Address: " << device << std::endl;
        std::cout << "Graphics Queue: " << graphicsQueue << std::endl;
        std::cout << "Present Queue:  " << presentQueue << std::endl;
        std::cout << "Command Buffer: " << commandBuffer << std::endl;
        std::cout << "Swapchain:      " << swapchain << std::endl;
        std::cout << "G-Buffer FBO:   " << gBuffer.getFramebuffer() << std::endl;
        
        if (device == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || 
            presentQueue == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE || 
            swapchain == VK_NULL_HANDLE) {
            std::cerr << "[FATAL ERROR] One of Components Vulkan is NULL (00000000)!" << std::endl;
            // Stop paksa biar kita lihat lognya
            int x; std::cin >> x; 
            exit(-1);
        }
        std::cout << "--- ALL SYSTEMS GO ---" << std::endl;
        mainLoop();
        cleanup();
    }

private:

    AppState currentState = AppState::LOADING; 
    // Instance Editor UI ImGui
    EditorUI editorUI;
    
    // ImGui Resources
    VkDescriptorPool imguiPool;

    GLFWwindow* window;

    // Vulkan Handles Core
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue computeQueue;

    // COMMAND SYSTEM
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    // SYNCHRONIZATION OBJECTS
    // Semaphore: Sinyal antar proses di dalam GPU (Image Available -> Render Finished)
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    // Fence: Sinyal agar CPU menunggu GPU selesai (agar tidak menumpuk frame)
    VkFence inFlightFence;

    // Render System
    GBuffer gBuffer;
    Model myModel;
    RenderPipeline gBufferPipeline;

    // --- LIGHTING SYSTEM ---
    LightingPass lightingPass;
    VkRenderPass lightingRenderPass; 
    std::vector<VkFramebuffer> swapchainFramebuffers; 

    // Sampler & Descriptors (Untuk membaca G-Buffer)
    VkSampler textureSampler;
    VkDescriptorSetLayout lightingDescriptorLayout;
    VkDescriptorPool lightingDescriptorPool;
    VkDescriptorSet lightingDescriptorSet;

    // UNIFORM BUFFER SYSTEM (CAMERA)
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    //SWAPCHAIN VARIABLES
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImageView> swapchainImageViews;
    VkQueue presentQueue;

    // TEXTURE RESOURCE
    Texture myTexture; 
    VkDescriptorSetLayout textureDescriptorLayout;
    VkDescriptorPool textureDescriptorPool;
    VkDescriptorSet textureDescriptorSet;

    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped;

    bool framebufferResized = false;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Beritahu GLFW kita pakai Vulkan
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "COGENT Engine (Phase 1: G-Buffer)", nullptr, nullptr);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Hide Mouse
    }

    void initVulkan() {
        createInstance();
        createSurface();
        pickPhysicalDevice(); // Logika seleksi GPU Discrete
        createLogicalDevice();
        createDescriptorSetLayout();
        createCommandPool();
        createCommandBuffer();
        createSyncObjects(); // Setup Queue (Graphics + Compute)
        createSwapchain();
        createSwapchainImageViews();
    }

    void initResources() {
        // --- 1. CORE RESOURCES ---
        std::cout << "Initializing G-Buffer (Albedo, Normal, Velocity, Depth)..." << std::endl;
        gBuffer.init(device, physicalDevice, WIDTH, HEIGHT);
        
        createUniformBuffer();
        createDescriptorPool(); // Pool untuk Camera
        createDescriptorSets(); // Set untuk Camera
        
        // --- 2. LOAD TEXTURE (WAJIB DULUAN!) ---
        // Kita harus load gambar dulu biar ImageView-nya terbentuk
        std::cout << "Loading Texture..." << std::endl;
        // Pastikan file gambar ada di build/Debug/textures/viking_room.png
        myTexture.load(device, physicalDevice, commandPool, graphicsQueue, "textures/viking_room.png"); 
        
        // --- 3. TEXTURE DESCRIPTORS (SET 1) ---
        // Sekarang aman dipanggil karena myTexture sudah punya ImageView
        createTextureDescriptors();

        // --- 4. INIT GRAPHICS PIPELINE ---
        std::cout << "Building Graphics Pipeline..." << std::endl;
        // Pipeline butuh 2 layout: Camera (Set 0) & Texture (Set 1)
        std::vector<VkDescriptorSetLayout> layouts = { descriptorSetLayout, textureDescriptorLayout };
        
        gBufferPipeline.init(device, gBuffer.getRenderPass(), {WIDTH, HEIGHT}, layouts);
        
        // --- 5. LOAD MODEL ---
        myModel.loadModel(device, physicalDevice, "models/viking_room.obj");

        // --- 6. LIGHTING PASS RESOURCES ---
        // Sampler ini dipakai untuk G-Buffer Reading (Lighting Pass)
        createTextureSampler(); 
        
        createLightingDescriptors(); // Butuh Texture Sampler di atas
        createLightingRenderPass(); 
        createSwapchainFramebuffers(); 
        
        lightingPass.init(device, lightingRenderPass, lightingDescriptorLayout, swapchainExtent);
        
        std::cout << "Resources Initialized Successfully!" << std::endl;

        // --- 7. INIT ImGui untuk Editor UI ---
        std::cout << "Initializing ImGui Editor UI..." << std::endl;
        editorUI.Init(window, instance, physicalDevice, device, 
                      findQueueFamilies(physicalDevice).graphicsFamily.value(), 
                      graphicsQueue, lightingRenderPass, 2);

    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<CogentEngine*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true; // Tandai bahwa layar berubah
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            // 1. Time Calculation
            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            glfwPollEvents();

            // --- [FIX CURSOR LOGIC] ---
            // Jika kita TIDAK di dalam game (misal: Loading atau Menu),
            // Cursor WAJIB muncul.
            if (currentState != AppState::EDITOR) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                showCursor = true;
            }
            // Jika di dalam Game/Editor, kita pakai logic Toggle
            else {
                // Toggle Cursor (Left Ctrl)
                bool currentCtrlState = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
                if (currentCtrlState && !lastCtrlState) {
                    showCursor = !showCursor;
                    
                    if (showCursor) {
                        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    } else {
                        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        firstMouse = true;
                    }
                }
                lastCtrlState = currentCtrlState;

                // Process Camera Input (Hanya jika cursor hidden)
                if (!showCursor) {
                    mainCamera.processKeyboard(window, deltaTime);
                }
            }

            // 3. Update UI Logic
            // This is important! For ImGui to work properly
            // Remember: ImGui Render ada di dalam recordCommandBuffer
            // Dont forget to pass currentState by reference!
            // so that EditorUI can modify it when needed
            // Example: currentState = AppState::EDITOR;
            editorUI.Update(currentState, showCursor, deltaTime, mainCamera, selectedObject);

            // 4. Update Uniform Buffer
            if (currentState == AppState::EDITOR) {
                updateUniformBuffer();
            }

            // 5. Render
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        // 1. Mulai Rekam Perintah
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to start recording command buffer!");
        }

        // =========================================================
        // PASS 1: GEOMETRY PASS
        // (Menggambar Model 3D + Texture ke G-Buffer)
        // =========================================================
        
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = gBuffer.getRenderPass();
        renderPassInfo.framebuffer = gBuffer.getFramebuffer();
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchainExtent;

        // Clear Screen (Hitamkan G-Buffer sebelum digambar)
        std::array<VkClearValue, 4> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Albedo
        clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Normal
        clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Velocity
        clearValues[3].depthStencil = {1.0f, 0};           // Depth

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // MULAI RENDER PASS 1
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Setup Viewport (Area Gambar)
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

            // Bind Pipeline G-Buffer (Shader Geometry)
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gBufferPipeline.getPipeline());
            
            // --- BIND DESCRIPTOR SETS ---
            
            // 1. Bind Set 0: Camera (View/Proj)
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                gBufferPipeline.getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

            // 2. [BARU] Bind Set 1: Texture (Material)
            // Perhatikan parameter 'firstSet' adalah 1
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                gBufferPipeline.getPipelineLayout(), 1, 1, &textureDescriptorSet, 0, nullptr);

            // -----------------------------

            // Hitung Rotasi Model (Animasi Putar)
            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); 
            modelMatrix = glm::rotate(modelMatrix, time * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 

            vkCmdPushConstants(commandBuffer, gBufferPipeline.getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMatrix);

            // DRAW MODEL
            myModel.draw(commandBuffer);

        vkCmdEndRenderPass(commandBuffer);


        // =========================================================
        // TRANSITION BARRIER
        // (Stop G-Buffer dari mode TULIS, ubah ke mode BACA untuk Shader Lighting)
        // =========================================================
        
        std::array<VkImageMemoryBarrier, 2> barriers{};

        // 1. Albedo Image Transition
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Bekas ditulis
        barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Siap dibaca shader
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

        // 2. Normal Image Transition
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

        // Eksekusi Barrier
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         
            0,
            0, nullptr,
            0, nullptr,
            static_cast<uint32_t>(barriers.size()), barriers.data()
        );


        // =========================================================
        // PASS 2: LIGHTING PASS
        // (Menggambar Fullscreen Quad ke Layar Monitor menggunakan data G-Buffer)
        // =========================================================
        
        VkRenderPassBeginInfo lightRenderPassInfo{};
        lightRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        lightRenderPassInfo.renderPass = lightingRenderPass;
        lightRenderPassInfo.framebuffer = swapchainFramebuffers[imageIndex]; // Framebuffer Layar Asli
        lightRenderPassInfo.renderArea.offset = {0, 0};
        lightRenderPassInfo.renderArea.extent = swapchainExtent;

        VkClearValue clearColor = {};
        clearColor.color = {{0.2f, 0.3f, 0.4f, 1.0f}};;
        lightRenderPassInfo.clearValueCount = 1;
        lightRenderPassInfo.pClearValues = &clearColor;

        // MULAI RENDER PASS 2
        vkCmdBeginRenderPass(commandBuffer, &lightRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Viewport Fullscreen
            VkViewport viewportFullscreen{};
            viewportFullscreen.width = (float)swapchainExtent.width;
            viewportFullscreen.height = (float)swapchainExtent.height;
            viewportFullscreen.minDepth = 0.0f;
            viewportFullscreen.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewportFullscreen);

            VkRect2D scissorFullscreen{};
            scissorFullscreen.extent = swapchainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissorFullscreen);

            // Bind Pipeline Lighting (Shader Lighting)
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPass.getPipeline());

            // Bind Descriptor Lighting (Input: Texture G-Buffer Albedo & Normal)
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                lightingPass.getPipelineLayout(), 0, 1, &lightingDescriptorSet, 0, nullptr);

            // DRAW 3 VERTEX (Segitiga Ajaib Fullscreen)
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            // [FIX TERBARU] Render ImGui di ATAS scene 3D via Class EditorUI
            // Ini wajib ada, kalau tidak data UI yg sudah dibuat di renderUI() tidak akan tampil!
            // Gantikan "ImGui_ImplVulkan_RenderDrawData" manual dengan:
            editorUI.Draw(commandBuffer);

            vkCmdEndRenderPass(commandBuffer);


        // Selesai Rekam
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to stop recording command buffer!");
        }
    }

    void cleanup() {
        std::cout << "Cleaning up resources..." << std::endl;
        //Make sure GPU on idle before cleaning up resources
        vkDeviceWaitIdle(device);
        
        //Destroy synchronization objects
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        //Destroy Lighting Pass Resources
        lightingPass.cleanup(device);
        vkDestroyDescriptorPool(device, lightingDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, lightingDescriptorLayout, nullptr);
        vkDestroySampler(device, textureSampler, nullptr);
        for (auto framebuffer : swapchainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        vkDestroyRenderPass(device, lightingRenderPass, nullptr);

        // Destroy Swapchain Image Views
        for (auto imageView : swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);

        // Destroy Command Pool
        vkDestroyCommandPool(device, commandPool, nullptr);

        // Cleaning G-Buffer
        gBuffer.cleanup(device);

        // Cleaning Model
        myModel.cleanup(device);

        // Cleaning Pipeline and Buffers
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, textureDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, textureDescriptorLayout, nullptr);
        myTexture.cleanup(device);
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        vkFreeMemory(device, uniformBufferMemory, nullptr);

        // Destroy Core Vulkan
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(device, imguiPool, nullptr);
        editorUI.Cleanup(device);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void drawFrame() {
        // 1. CPU-GPU Synchronization (Wait for Previous Frame)
        // We must wait until the GPU has finished rendering the previous frame.
        // Otherwise, we might overwrite resources (command buffers) currently in use.
        // UINT64_MAX means we disable timeout (wait forever until signaled).
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

        // 2. Acquire Next Image from Swapchain
        // Ask the swapchain for the index of the next available image.
        // We use a Semaphore (imageAvailableSemaphore) to signal when this image is ready.
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        // Handle Window Resize (Out of Date)
        // If the window was resized, the swapchain is invalid. We must recreate it immediately.
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return; // Skip this frame, try again next loop
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swapchain image!");
        }

        // 3. Reset Fence (Manual Reset)
        // Unlike semaphores, Fences must be manually reset to "Unsignaled" state.
        // If we forget this, vkWaitForFences will return immediately next frame (Bug!).
        vkResetFences(device, 1, &inFlightFence);

        // 4. Reset & Record Command Buffer
        // Clear the previous commands and record new ones for the current image.
        // This ensures we are drawing with the latest game state.
        vkResetCommandBuffer(commandBuffer, 0);
        recordCommandBuffer(commandBuffer, imageIndex);

        // 5. Configure Submission Info
        // We need to tell the GPU which semaphores to wait on and which to signal.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Wait until the image is available before writing colors (Color Attachment stage)
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // Signal this semaphore when rendering is finished
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // 6. Submit to Graphics Queue
        // Send the command buffer to the GPU for execution.
        // The 'inFlightFence' will be signaled when this specific batch is done.
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        // 7. Present to Screen
        // Hand over the rendered image to the presentation engine.
        // We must wait for 'renderFinishedSemaphore' before showing it.
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapchains[] = {swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        // Handle Resize after Present
        // Sometimes resize happens exactly during presentation. Check again here.
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapchain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swapchain image!");
        }
    }

    // --- HELPER FUNCTIONS (Logika Detail) ---

    void updateCamera() {
        // Simulasi kalkulasi matriks view/proj (Logic GLM)
        glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        proj[1][1] *= -1; // Fix Vulkan coordinate system
    }

    void createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "COGENT Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "COGENT";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        // Setup Extensions (GLFW butuh surface extension)
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat Vulkan Instance!");
        }
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat Window Surface!");
        }
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) throw std::runtime_error("Tidak ditemukan GPU dengan support Vulkan!");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Cari GPU Discrete (NVIDIA/AMD)
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && isDeviceSuitable(device)) {
                physicalDevice = device;
                std::cout << "Selected GPU: " << props.deviceName << std::endl;
                break;
            }
        }

        // Fallback ke Integrated jika tidak ada Discrete
        if (physicalDevice == VK_NULL_HANDLE && deviceCount > 0) {
            physicalDevice = devices[0];
            std::cout << "Warning: Menggunakan Integrated GPU." << std::endl;
        }

        if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("Gagal menemukan GPU yang cocok!");
    }

    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);
        return indices.isComplete();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // Cari queue yang support Graphics
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            // Cari queue yang support Compute (Untuk AI)
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                indices.computeFamily = i;
            }
            if (indices.isComplete()) break;
            i++;
        }
        return indices;
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        // RESET_COMMAND_BUFFER_BIT memungkinkan kita merekam ulang command buffer setiap frame
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat Command Pool!");
        }
    }

    void createCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Gagal mengalokasikan Command Buffer!");
        }
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // SIGNALED_BIT penting agar frame pertama tidak macet menunggu fence
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat objek sinkronisasi!");
        }
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;

        // Setup Queue Graphics
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);

        // Setup Features (Nanti kita aktifkan fitur advance di sini)
        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        // Aktifkan extension Swapchain (Wajib untuk rendering ke layar)
        const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to make Logical Device!");
        }

        // --- TAMBAHKAN BARIS INI YANG HILANG ---
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue); 
        // ---------------------------------------

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &presentQueue);
        // computeQueue bisa diambil nanti saat fitur AI diaktifkan
    }

    void createSwapchain() {
        // 1. Cek Kapabilitas Surface
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        // 2. Tentukan Format (Pilih B8G8R8A8_SRGB agar warna akurat)
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        
        VkSurfaceFormatKHR surfaceFormat = formats[0];
        for (const auto& availableFormat : formats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = availableFormat;
                break;
            }
        }

        // 3. Tentukan Mode Presentasi (Mailbox = Triple Buffering = Smooth)
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // Default (V-Sync)
        // Kalau mau uncap FPS bisa cari VK_PRESENT_MODE_MAILBOX_KHR atau IMMEDIATE

        // 4. Tentukan Resolusi Swapchain
        VkExtent2D extent;
        if (capabilities.currentExtent.width != UINT32_MAX) {
            extent = capabilities.currentExtent;
        } else {
            // Handle high-DPI display logic here if needed
            extent = {WIDTH, HEIGHT};
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        // 5. Create Swapchain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Render langsung ke sini (atau copy dari GBuffer)

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.graphicsFamily.value()}; 
        // Note: Jika Graphics & Present queue beda, logic sharing mode harus concurrent.
        // Untuk simplisitas kita asumsikan sama (exclusive).

        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat swapchain!");
        }

        // Simpan data
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        swapchainImageFormat = surfaceFormat.format;
        swapchainExtent = extent;
    }

    void createSwapchainImageViews() {
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

            if (vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Gagal membuat image views swapchain!");
            }
        }
    }

    void recreateSwapchain() {
        vkDeviceWaitIdle(device);
        // Hancurkan yang lama
        for (auto imageView : swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);

        // Buat baru
        createSwapchain();
        createSwapchainImageViews();
        
        // Note: Karena kita pakai Deferred G-Buffer yang ukurannya fix, 
        // kita mungkin perlu resize G-Buffer juga di sini jika mau responsif.
    }

    void createLightingRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainImageFormat; // Format layar monitor
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Bersihkan layar tiap frame
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Simpan hasilnya untuk ditampilkan
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Siap dipresentasikan

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        // Dependency: Tunggu sampai tahap color output sebelumnya selesai
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

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &lightingRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat Lighting Render Pass!");
        }
    }

    void createSwapchainFramebuffers() {
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

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Gagal membuat Swapchain Framebuffer!");
            }
        }
    }

    void createTextureSampler() {
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

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat Texture Sampler!");
        }
    }

    void createLightingDescriptors() {
        // 1. Layout: Binding 0 = Albedo, Binding 1 = Normal
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        
        // Binding 0: Albedo Texture
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 1: Normal Texture
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &lightingDescriptorLayout) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat Lighting Descriptor Layout!");
        }

        // 2. Pool
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = 2; // Albedo + Normal

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &lightingDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat Lighting Descriptor Pool!");
        }

        // 3. Allocate Set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = lightingDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &lightingDescriptorLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &lightingDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Gagal allocate Lighting Descriptor Set!");
        }

        // 4. Update Set (HUBUNGKAN G-BUFFER KE SHADER)
        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        // --- BINDING 0: ALBEDO TEXTURE ---
        VkDescriptorImageInfo albedoInfo{};
        albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        // [FIX] Gunakan getAlbedoView() (Bukan Image!)
        albedoInfo.imageView = gBuffer.getAlbedoView(); 
        
        albedoInfo.sampler = textureSampler;

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = lightingDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &albedoInfo;

        // --- BINDING 1: NORMAL TEXTURE ---
        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        // [FIX] Gunakan getNormalView() (Bukan Image!)
        normalInfo.imageView = gBuffer.getNormalView();
        
        normalInfo.sampler = textureSampler;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = lightingDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &normalInfo;

        // Kirim update ke GPU
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    // 1. Membuat Layout (Aturan Shader: Binding 0 adalah UBO)
    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        // Vertex shader butuh ini untuk hitung posisi
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; 
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat descriptor set layout!");
        }
    }

    // 2. Membuat Buffer Fisik di GPU
    void createUniformBuffer() {
        VkDeviceSize bufferSize = sizeof(CameraUBO);

        // Gunakan helper createBuffer yang sama dengan Model.cpp
        // (Atau copy paste logic createBuffer sederhana di sini)
        createBuffer(bufferSize, 
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     uniformBuffer, uniformBufferMemory);

        // Map memory sekali saja agar performa update cepat (Persistent Mapping)
        vkMapMemory(device, uniformBufferMemory, 0, bufferSize, 0, &uniformBufferMapped);
    }

    // Helper sederhana untuk createBuffer (taruh di private class CogentEngine)
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memRequirements.size;
        
        // Cari memory type (Copy logic findMemoryType dari GBuffer.cpp atau Model.cpp ke sini)
        // Agar main.cpp rapi, kita asumsikan kamu punya helper findMemoryType
        allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

        vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    // 3. Membuat Pool & Descriptor Set
    void createDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Gagal membuat descriptor pool!");
        }
    }

    void createDescriptorSets() {
        // 1. Setup Allocation Info
        // We need a pool (the storage) and a layout (the blueprint)
        // This is like requesting a specific blank form from the GPU
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool; // Grab from our pool
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout; // Use the UBO layout we defined earlier

        // 2. Allocate Descriptor Set
        // Try to allocate memory on the GPU. If this fails, we crash hard.
        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        // 3. Configure Buffer Info
        // Link the actual physical buffer to the descriptor
        // This tells the GPU: "Hey, the camera data starts here and is this big"
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer;      // The actual buffer resource
        bufferInfo.offset = 0;                  // Start from the beginning
        bufferInfo.range = sizeof(CameraUBO);   // Size of the data (View + Proj + Time, etc.)

        // 4. Update Descriptor Set (Write)
        // Prepare the write operation to link bufferInfo to the Descriptor Set
        // Important: dstBinding must match 'layout(binding = 0)' in your shader!
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 0;         // Binding 0 (Vertex Shader UBO)
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // It's a UBO
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo; // Pointer to the buffer info above

        // 5. Commit Update
        // Push these changes to the device. 
        // Now the Shader can finally read our CameraUBO data!
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
    
    // 4. Update Data Each Frame
    void updateUniformBuffer() {
        // 1. Safety Check (Window Minimize & Buffer Validity)
        // This is important! Prevent crash when window is minimized (size 0)
        // If swapchain size is 0, we stop here immediately
        if (swapchainExtent.width == 0 || swapchainExtent.height == 0) return;

        // Check if memory is properly mapped before accessing it
        // Remember: uniformBufferMapped must not be null!
        if (uniformBufferMapped == nullptr) {
            static bool errorPrinted = false; 
            if (!errorPrinted) {
                std::cerr << "[ERROR] uniformBufferMapped is NULL! Skipping update camera." << std::endl;
                errorPrinted = true;
            }
            return; 
        }

        // 2. Static Variables for TAA (Motion Vectors)
        // We use static to keep these values across frames
        // LastViewMatrix & LastProjMatrix needed for temporal anti-aliasing logic
        static glm::mat4 lastViewMatrix = glm::mat4(1.0f);
        static glm::mat4 lastProjMatrix = glm::mat4(1.0f);
        
        // FirstFrame flag ensuring no glitch at startup (visual artifact)
        static bool firstFrame = true;

        // 3. Time Calculation Logic (Animation Ready)
        // We calculate total time for future animation needs (water, grass, etc)
        // Use high_resolution_clock for precision
        // Don't remove this! Important for shader animations later
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        CameraUBO ubo{};

        // 4. Fill Data to UBO (Time & DeltaTime)
        // Send these values to GPU so shaders can use them
        // Example: Use ubo.time for sin wave animation in vertex shader
        ubo.time = time;
        ubo.deltaTime = deltaTime; 

        // 5. Calculate Current Matrices (View)
        // [UPDATE Phase 3] Using Main Camera (First Person Control)
        // Get view matrix directly from our Camera class
        // Also update viewPos for specular lighting calculations
        ubo.view = mainCamera.getViewMatrix();
        ubo.viewPos = mainCamera.position;

        // 6. Projection Matrix (Perspective Lens)
        // FOV 45 degree, dynamic aspect ratio from swapchain (Responsive UI)
        // Near plane 0.1, Far plane 1000.0f (Increased for better view distance)
        // Important: Flip Y-coordinate (ubo.proj[1][1] *= -1) because Vulkan is different from OpenGL
        float aspectRatio = (float)swapchainExtent.width / (float)swapchainExtent.height;
        ubo.proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
        ubo.proj[1][1] *= -1; 

        // 7. Assign Previous Matrices (TAA / Motion Blur)
        // If this is the very first frame, use current as previous to avoid glitches
        // Otherwise, use the stored matrices from the last frame
        if (firstFrame) {
            lastViewMatrix = ubo.view;
            lastProjMatrix = ubo.proj;
            firstFrame = false;
        }

        ubo.prevView = lastViewMatrix;
        ubo.prevProj = lastProjMatrix;

        // 8. Copy Data to GPU Memory
        // Use memcpy to transfer CameraUBO struct to mapped memory
        // This sends all our matrices & time data to the Vertex/Fragment Shader
        memcpy(uniformBufferMapped, &ubo, sizeof(ubo));

        // 9. Store Current Matrices for Next Frame
        // Save current View & Proj to static variables
        // This will become "prevView" & "prevProj" in the next loop
        lastViewMatrix = ubo.view;
        lastProjMatrix = ubo.proj;
    }

    // TEXTURE DESCRIPTORS
    void createTextureDescriptors() {
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // CRITICAL: TEXTURE DESCRIPTOR PIPELINE
        // -------------------------------------------------------------------------------------
        // This function manages the bridge between CPU texture memory and the GPU Fragment Shader.
        // WARNING: DO NOT REFACTOR OR REORDER THIS LOGIC.
        // Any misalignment here will cause Validation Layer errors or immediate GPU Device Loss.
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        // =====================================================================================
        // 1. DESCRIPTOR LAYOUT CONFIGURATION
        // =====================================================================================
        
        // [CRITICAL] Binding 0: Combined Image Sampler
        // This STRICTLY matches 'layout(binding = 0) uniform sampler2D' in the Fragment Shader.
        // DO NOT CHANGE the binding index or stage flags unless the Shader is also updated.
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorCount = 1; 
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr; // We use dynamic samplers from the Texture class
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        // Failsafe: If layout creation fails, the texture system is non-functional.
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &textureDescriptorLayout) != VK_SUCCESS) {
            throw std::runtime_error("FATAL ERROR: Failed to create Texture Descriptor Layout!");
        }

        // =====================================================================================
        // 2. DESCRIPTOR POOL ALLOCATION
        // =====================================================================================

        // [MEMORY] Pool Sizing
        // We allocate EXACTLY one descriptor for the texture sampler.
        // WARNING: Do not arbitrarily increase 'maxSets' without memory budget validation.
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1; 

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &textureDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("FATAL ERROR: Failed to create Texture Descriptor Pool!");
        }

        // =====================================================================================
        // 3. DESCRIPTOR SET INSTANTIATION
        // =====================================================================================

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = textureDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &textureDescriptorLayout;

        // If allocation fails, it likely means the Pool is exhausted or fragmented.
        if (vkAllocateDescriptorSets(device, &allocInfo, &textureDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("FATAL ERROR: Failed to allocate Texture Descriptor Set!");
        }

        // =====================================================================================
        // 4. WRITE & UPDATE (GPU COMMIT)
        // =====================================================================================

        // [LINKING] Image View & Sampler
        // This connects the loaded texture memory to the allocated descriptor.
        // NOTE: Image Layout MUST be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL before this step.
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = myTexture.getImageView();
        imageInfo.sampler = myTexture.getSampler();

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = textureDescriptorSet;
        descriptorWrite.dstBinding = 0; // Must match Layout Binding defined above
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        // EXECUTE UPDATE:
        // Pushes the configuration to the GPU. After this line, the shader can sample the texture.
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

};

int main() {
    CogentEngine app;
    try {
        app.run();
    } catch (const std::exception& e) {
        // Ini akan mencetak alasan errornya
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        
        // Tahan window biar kita bisa baca errornya
        std::cout << "\nPress ENTER to close..." << std::endl;
        std::cin.get(); 
        
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}