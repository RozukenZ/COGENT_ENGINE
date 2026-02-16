#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <filesystem>
#include "../Core/Types.hpp"
#include "../Core/Camera.hpp"

enum class AppState {
    LOADING,
    PROJECT_MENU,
    EDITOR
};

class EditorUI {
public:
    void Init(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice, 
              VkDevice device, uint32_t queueFamily, VkQueue queue, 
              VkRenderPass renderPass, uint32_t minImageCount);

    void Update(AppState& currentState, bool& showCursor, float& deltaTime, Camera& camera, ObjectPushConstant& selectedObject, std::vector<GameObject>& gameObjects, int& selectedIndex, VkDescriptorSet sceneTexture = VK_NULL_HANDLE);
    void Draw(VkCommandBuffer commandBuffer);
    void Cleanup(VkDevice device);
    

private:
    void ApplyModernDarkTheme();
    void RenderLoadingScreen(AppState& currentState);
    void RenderProjectHub(AppState& currentState, bool& showCursor);
    void RenderEditorWorkspace(bool showCursor, float deltaTime, Camera& camera, ObjectPushConstant& selectedObject, std::vector<GameObject>& gameObjects, int& selectedIndex, VkDescriptorSet sceneTexture);
    void RenderHierarchy(std::vector<GameObject>& objects, int& selectedIndex);
    void RenderFolderBrowserModal(); 

    VkDescriptorPool imguiPool;
    float loadingProgress = 0.0f;
    
    // Project Hub State
    bool showNewProjectModal = false; // Hanya flag visual
    bool openNewProjectPopup = false; // Trigger untuk OpenPopup
    char projectNameBuffer[128] = "MyNewGame";
    int selectedHubTab = 0; 
    
    // File Browser State
    bool showFolderBrowser = false; // Flag visual
    bool openFolderPopup = false;   // Trigger untuk OpenPopup
    std::string currentPath = ".";  // [FIX] Default ke folder project biar aman
    std::string selectedPath = "."; // [FIX] Default ke folder project biar aman
};