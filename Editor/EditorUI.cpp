#include "EditorUI.hpp"
#include <array>
#include <iostream>
#include <cmath>
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#include "Types.hpp"
#include "Camera.hpp"
#include "Logger.hpp" // [NEW] Include Logger

namespace fs = std::filesystem;

void EditorUI::Init(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice, 
                    VkDevice device, uint32_t queueFamily, VkQueue queue, 
                    VkRenderPass renderPass, uint32_t minImageCount) 
{
    // 1. Create Descriptor Pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool);

    // 2. Setup Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking

    // 3. Apply Ultra Modern Theme
    ApplyModernDarkTheme();

    // 4. Setup Backend
    ImGui_ImplGlfw_InitForVulkan(window, true);
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = device;
    init_info.QueueFamily = queueFamily;
    init_info.Queue = queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = minImageCount;
    init_info.ImageCount = minImageCount;
    
    init_info.PipelineInfoMain.RenderPass = renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT; 
    
    ImGui_ImplVulkan_Init(&init_info);
}

// [FIX] Parameter fungsi ditambahkan: onSpawn callback, outSceneSize, textureSize
void EditorUI::Update(AppState& currentState, bool& showCursor, float& deltaTime, Camera& camera, ObjectPushConstant& selectedObject, std::vector<GameObject>& gameObjects, int& selectedIndex, VkDescriptorSet sceneTexture, std::function<void(int)> onSpawn, glm::vec2* outSceneSize, glm::vec2 textureSize) {
    
    // 1. Setup Frame ImGui
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 2. Setup Fullscreen Window Docking (Opsional tapi bagus untuk Editor)
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    
    // 3. State Machine Logic
    if (currentState == AppState::LOADING) {
        RenderLoadingScreen(currentState);
    } 
    else if (currentState == AppState::PROJECT_MENU) {
        try {
            RenderProjectHub(currentState, showCursor);
            
            // [FIX] Logic Folder Browser (Floating Window)
            if (openFolderPopup) {
                showFolderBrowser = true;
                openFolderPopup = false;
            }

            if (showFolderBrowser) {
                RenderFolderBrowserModal();
            }
            LOG_INFO("RenderProjectHub Finished");
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in ProjectHub: " + std::string(e.what()));
        }
    } 
    else if (currentState == AppState::EDITOR) {
        // [FIX] Passing data Camera & Object ke Workspace
        // Agar Gizmo dan Inspector bisa bekerja!
        RenderEditorWorkspace(showCursor, deltaTime, camera, selectedObject, gameObjects, selectedIndex, sceneTexture, onSpawn, outSceneSize, textureSize);
    }

    // 4. Render Draw Data
    LOG_INFO("ImGui::Render() Starting...");
    ImGui::Render();
    LOG_INFO("ImGui::Render() Finished");
}

void EditorUI::Draw(VkCommandBuffer commandBuffer) {
    LOG_INFO("EditorUI::Draw called");
    if (ImGui::GetDrawData()) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
        LOG_INFO("ImGui_ImplVulkan_RenderDrawData Finished");
    } else {
        LOG_WARN("ImGui::GetDrawData() returned NULL!");
    }
}

void EditorUI::Cleanup(VkDevice device) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, imguiPool, nullptr);
}

// ==================================================================================
//                              ULTRA MODERN THEME
// ==================================================================================

void EditorUI::ApplyModernDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Spacing & Rounding - More generous and modern
    style.WindowPadding     = ImVec2(15.0f, 15.0f);
    style.FramePadding      = ImVec2(12.0f, 8.0f);
    style.ItemSpacing       = ImVec2(12.0f, 10.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.IndentSpacing     = 28.0f;
    style.ScrollbarSize     = 16.0f;
    
    // Rounded corners everywhere for modern look
    style.WindowRounding    = 12.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 10.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    
    // Borders
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    // Modern color palette - Deep dark with vibrant accents
    ImVec4* colors = style.Colors;
    
    // Text
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // Backgrounds - Rich dark tones
    colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.13f, 0.98f);
    
    // Borders - Subtle but visible
    colors[ImGuiCol_Border]                 = ImVec4(0.25f, 0.25f, 0.28f, 0.60f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
    
    // Title bars - Slightly lighter
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    
    // Frames - Input fields, etc
    colors[ImGuiCol_FrameBg]                = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
    
    // Buttons - Modern flat design with vibrant accent
    colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.30f, 0.52f, 0.90f, 1.00f);  // Vibrant blue
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.26f, 0.46f, 0.82f, 1.00f);
    
    // Accent color - Electric blue
    colors[ImGuiCol_CheckMark]              = ImVec4(0.35f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.35f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    
    // Headers - Collapsible sections
    colors[ImGuiCol_Header]                 = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.30f, 0.52f, 0.90f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.35f, 0.65f, 1.00f, 1.00f);
    
    // Separator
    colors[ImGuiCol_Separator]              = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.35f, 0.65f, 1.00f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.35f, 0.65f, 1.00f, 1.00f);
    
    // Tabs - Modern tab design
    colors[ImGuiCol_Tab]                    = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.35f, 0.65f, 1.00f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
}

// ==================================================================================
//                              MODERN RENDER FUNCTIONS
// ==================================================================================

void EditorUI::RenderLoadingScreen(AppState& currentState) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;
    ImGui::Begin("Loading", nullptr, flags);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    
    // Animated logo with glow effect
    ImGui::SetCursorPos(ImVec2(center.x - 180, center.y - 120));
    
    // Title with gradient-like effect (layered text)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.45f, 0.85f, 0.3f));
    ImGui::SetWindowFontScale(2.8f);
    ImGui::SetCursorPos(ImVec2(center.x - 177, center.y - 117));
    ImGui::Text("COGENT ENGINE");
    ImGui::PopStyleColor();
    
    ImGui::SetCursorPos(ImVec2(center.x - 180, center.y - 120));
    ImGui::SetWindowFontScale(2.8f);
    ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.00f, 1.0f), "COGENT ENGINE");
    ImGui::SetWindowFontScale(1.0f);
    
    // Subtitle
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y - 50));
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.75f, 1.0f), "Next-Gen Game Engine");

    // Animated progress
    loadingProgress += 0.008f;
    if (loadingProgress >= 1.0f) {
        loadingProgress = 1.0f;
        currentState = AppState::PROJECT_MENU;
    }

    // Loading text with animation
    ImGui::SetCursorPos(ImVec2(center.x - 220, center.y + 30));
    const char* loadingTexts[] = {
        "Initializing Vulkan Renderer...",
        "Loading Core Modules...",
        "Preparing Workspace...",
        "Almost Ready..."
    };
    int textIndex = static_cast<int>(loadingProgress * 4) % 4;
    ImGui::TextDisabled("%s", loadingTexts[textIndex]);
    
    // Modern progress bar with glow
    ImGui::SetCursorPos(ImVec2(center.x - 250, center.y + 65));
    
    // Background bar
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.35f, 0.65f, 1.00f, 1.0f));
    ImGui::ProgressBar(loadingProgress, ImVec2(500, 8), "");
    ImGui::PopStyleColor(2);
    
    // Progress percentage
    ImGui::SetCursorPos(ImVec2(center.x - 20, center.y + 80));
    ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.00f, 1.0f), "%.0f%%", loadingProgress * 100);

    ImGui::End();
}

void EditorUI::RenderProjectHub(AppState& currentState, bool& showCursor) {
    showCursor = true; 
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;
    ImGui::Begin("Hub", nullptr, flags);

    // [LOG] Start Hub
    // LOG_INFO("Project Hub Rendered");

    ImGui::Columns(2, "HubSplit", false);
    ImGui::SetColumnWidth(0, 320);

    // --- LEFT SIDEBAR ---
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
    ImGui::BeginChild("Sidebar", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::Dummy(ImVec2(0, 30));
    
    // Logo
    ImGui::SetCursorPosX(40);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.00f, 1.0f), "COGENT");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPosX(40);
    ImGui::TextDisabled("Game Engine Hub");
    
    ImGui::Dummy(ImVec2(0, 40));
    
    // Navigation Buttons
    ImGui::Indent(25);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    
    // Logic Tab Active Color
    ImVec4 activeBtn = ImVec4(0.35f, 0.40f, 0.45f, 1.0f); 
    ImVec4 normalBtn = ImVec4(0.25f, 0.25f, 0.30f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, selectedHubTab == 0 ? activeBtn : normalBtn);
    if (ImGui::Button("PROJECTS", ImVec2(250, 50))) selectedHubTab = 0;
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));
    
    ImGui::PushStyleColor(ImGuiCol_Button, selectedHubTab == 1 ? activeBtn : normalBtn);
    if (ImGui::Button("LEARN", ImVec2(250, 50))) selectedHubTab = 1;
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));
    
    ImGui::PushStyleColor(ImGuiCol_Button, selectedHubTab == 2 ? activeBtn : normalBtn);
    if (ImGui::Button("COMMUNITY", ImVec2(250, 50))) selectedHubTab = 2;
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));
    
    ImGui::PushStyleColor(ImGuiCol_Button, selectedHubTab == 3 ? activeBtn : normalBtn);
    if (ImGui::Button("SETTINGS", ImVec2(250, 50))) selectedHubTab = 3;
    ImGui::PopStyleColor();
    
    ImGui::PopStyleVar();
    ImGui::Unindent(25);
    
    // Footer
    ImGui::SetCursorPos(ImVec2(40, ImGui::GetWindowHeight() - 60));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.55f, 1.0f));
    ImGui::Text("Version 1.0.0");
    ImGui::Text("© 2026 Cogent Studios");
    ImGui::PopStyleColor();
    
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::NextColumn();

    // --- RIGHT PANEL (CONTENT) ---
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false);
    
    ImGui::Dummy(ImVec2(0, 30));
    
    if (selectedHubTab == 0) {
        // Header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.96f, 0.98f, 1.0f));
        ImGui::SetWindowFontScale(1.8f);
        ImGui::Text("Recent Projects");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Continue where you left off");
        
        ImGui::Dummy(ImVec2(0, 20));
        
        // Search
        ImGui::PushItemWidth(400);
        static char searchBuf[128] = "";
        ImGui::InputTextWithHint("##search", "Search projects...", searchBuf, 128);
        ImGui::PopItemWidth();
        
        ImGui::Dummy(ImVec2(0, 20));

        // Project Cards Area
        ImGui::BeginChild("ProjectCards", ImVec2(0, -100), false);
        
        // Card 1
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.17f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
        
        ImGui::BeginChild("Card1", ImVec2(ImGui::GetContentRegionAvail().x, 100), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPos(ImVec2(20, 20)); ImGui::SetWindowFontScale(1.3f); ImGui::Text("FPS Shooter Demo"); ImGui::SetWindowFontScale(1.0f);
        ImGui::SetCursorPos(ImVec2(20, 50)); ImGui::TextDisabled("D:/Dev/FPS_Demo");
        ImGui::SetCursorPos(ImVec2(20, 70)); ImGui::TextDisabled("Last modified: 2 hours ago");
        
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 120, 30));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 1.00f, 1.0f));
        if (ImGui::Button("OPEN", ImVec2(100, 40))) { 
            LOG_INFO("Opening Demo Project: FPS Shooter");
            currentState = AppState::EDITOR; 
            showCursor = false; 
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();
        
        ImGui::Dummy(ImVec2(0, 15));
        
        // Card 2
        ImGui::BeginChild("Card2", ImVec2(ImGui::GetContentRegionAvail().x, 100), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPos(ImVec2(20, 20)); ImGui::SetWindowFontScale(1.3f); ImGui::Text("RPG Open World"); ImGui::SetWindowFontScale(1.0f);
        ImGui::SetCursorPos(ImVec2(20, 50)); ImGui::TextDisabled("D:/Dev/RPG_Project");
        ImGui::SetCursorPos(ImVec2(20, 70)); ImGui::TextDisabled("Last modified: 1 day ago");
        
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 120, 30));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 1.00f, 1.0f));
        if (ImGui::Button("OPEN##2", ImVec2(100, 40))) { 
            LOG_INFO("Opening Demo Project: RPG Open World");
            currentState = AppState::EDITOR; 
            showCursor = false; 
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::EndChild(); // End ProjectCards

        // Action Buttons (Bottom)
        ImGui::Dummy(ImVec2(0, 10));
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 1.00f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        
        // [FIX UTAMA] Logic Show New Project (Toggle Flag Overlay)
        if (ImGui::Button("NEW PROJECT", ImVec2(200, 50))) {
            LOG_INFO("Button [NEW PROJECT] Clicked");
            showNewProjectModal = true;
        }
        
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
        
        if (ImGui::Button("OPEN FROM DISK", ImVec2(200, 50))) { }
        
        ImGui::PopStyleColor();



        ImGui::PopStyleVar();
    } 
    else {
        ImGui::Dummy(ImVec2(0, 100));
        ImGui::SetCursorPosX(50);
        ImGui::TextDisabled("This section is under construction...");
    }

    // --- CREATE PROJECT OVERLAY ---
    // If flag is active, render the Create Form INSTEAD of using a Popup
    if (showNewProjectModal) {
        // [1] DIMMER BACKGROUND
        // Render a full-screen invisible window with a semi-transparent background to block input to the Hub
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.7f));
        ImGui::Begin("##Dimmer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::End();
        ImGui::PopStyleColor();

        // [2] CREATE PROJECT WINDOW (Floating, Centered)
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(600, 450));
        
        // Styling for the Modal
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f); // Make input fields nicer
        
        // Use ImGui::Begin() to create a true floating window
        // Note: We use a unique name to ensure it's treated as a new window
        ImGui::Begin("Create New Project", &showNewProjectModal, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
        
        ImGui::SetWindowFontScale(1.3f);
        ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.00f, 1.0f), "Create New Project");
        ImGui::SetWindowFontScale(1.0f);
        
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 20));
        
        ImGui::Text("Project Name");
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::PushItemWidth(-1);
        
        // Ensure buffer is valid
        if (projectNameBuffer != nullptr) {
             ImGui::InputTextWithHint("##Name", "Enter project name...", projectNameBuffer, 128);
        }
        
        ImGui::PopItemWidth();
        
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::Text("Project Location");
        ImGui::Dummy(ImVec2(0, 5));
        
        ImGui::PushItemWidth(-120);
        
        // Safer String Copy
        char pathBuf[256] = {0}; 
        snprintf(pathBuf, sizeof(pathBuf), "%s", selectedPath.c_str());
        
        ImGui::InputText("##Path", pathBuf, 256, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
        
        // Trigger generic folder popup (which is robust)
        if (ImGui::Button("Browse...", ImVec2(100, 0))) {
            openFolderPopup = true; 
        }
        
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 20));
        
        ImGui::Text("Project Template");
        ImGui::Dummy(ImVec2(0, 10));
        
        static int selectedTemplate = 0;
        ImGui::RadioButton("First Person", &selectedTemplate, 0); 
        ImGui::SameLine(200);
        ImGui::RadioButton("Third Person", &selectedTemplate, 1);
        ImGui::Dummy(ImVec2(0, 5));
        
        ImGui::RadioButton("Vehicle", &selectedTemplate, 2); 
        ImGui::SameLine(200);
        ImGui::RadioButton("Blank", &selectedTemplate, 3);
        
        ImGui::Dummy(ImVec2(0, 30));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        float buttonWidth = 150;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonWidth * 2 - 30);
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
        
        if (ImGui::Button("CANCEL", ImVec2(buttonWidth, 40))) {
            showNewProjectModal = false; // Just hide the overlay
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 1.00f, 1.0f));
        
        if (ImGui::Button("CREATE", ImVec2(buttonWidth, 40))) {
            if (std::string(projectNameBuffer).empty()) {
                // Warning logic...
            } else {
                LOG_INFO("Creating Project: " + std::string(projectNameBuffer));
                currentState = AppState::EDITOR;
                showCursor = false;
                showNewProjectModal = false;
            }
        }
        ImGui::PopStyleColor();
        
        ImGui::End(); // End Create Project Window
        
        ImGui::PopStyleVar(); // Pop WindowRounding (Create Project)
        ImGui::PopStyleVar(); // Pop TitleBarRound (Create Project)
        ImGui::PopStyleColor(); // Pop WindowBg (Create Project)
    }


    // Old Modal Logic Removed
    
    ImGui::EndChild();
    ImGui::End();
}

// ==================================================================================
//                          MODERN FOLDER BROWSER
// ==================================================================================

void EditorUI::RenderFolderBrowserModal() {
    // [1] DIMMER
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.7f));
    ImGui::Begin("##DimmerFolder", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::End();
    ImGui::PopStyleColor();

    // [2] BROWSER WINDOW
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(700, 500));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    
    // Use standard window, not popup
    if (ImGui::Begin("Select Folder", &showFolderBrowser, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {
        
        // --- HEADER ---
        ImGui::SetWindowFontScale(1.3f);
        ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.00f, 1.0f), "Select Project Location");
        ImGui::SetWindowFontScale(1.0f);
        
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        // --- DRIVE SHORTCUTS ---
        ImGui::Text("Quick Access:");
        ImGui::SameLine();
        
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
        
        // Tombol Drive (Manual karena filesystem C++ tidak punya deteksi drive cross-platform yang mudah)
        if (ImGui::Button("C:", ImVec2(60, 30))) currentPath = "C:/";
        ImGui::SameLine();
        if (ImGui::Button("D:", ImVec2(60, 30))) currentPath = "D:/";
        ImGui::SameLine();
        if (ImGui::Button("E:", ImVec2(60, 30))) currentPath = "E:/";
        ImGui::SameLine();
        
        // Tombol Home User
        if (ImGui::Button("Home", ImVec2(80, 30))) {
            #ifdef _WIN32
            char* buf = nullptr;
            size_t sz = 0;
            if (_dupenv_s(&buf, &sz, "USERPROFILE") == 0 && buf != nullptr) {
                currentPath = std::string(buf);
                free(buf);
            }
            #else
            const char* home = getenv("HOME");
            if (home) currentPath = std::string(home);
            #endif
        }
        
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::Dummy(ImVec2(0, 10));
        
        // --- NAVIGATION BAR ---
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.24f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        
        // Tombol UP
        if (ImGui::Button("⬆UP", ImVec2(70, 32))) {
            try {
                fs::path p = currentPath;
                if (p.has_parent_path() && p != p.root_path()) {
                    currentPath = p.parent_path().string();
                }
            } catch (...) {}
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        
        // Kolom Path (Read Only)
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushItemWidth(-1);
        
        char pathDisplay[512] = {0};
        if (currentPath.length() < 512) {
             #ifdef _WIN32
             strncpy_s(pathDisplay, currentPath.c_str(), 511);
             #else
             strncpy(pathDisplay, currentPath.c_str(), 511);
             #endif
        }

        ImGui::InputText("##CurrentPath", pathDisplay, 512, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        // --- FOLDER LIST (CONTENT) ---
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        
        // Area Scrollable
        ImGui::BeginChild("FolderList", ImVec2(0, -90), true);
        
        try {
            // Cek apakah path valid sebelum iterasi
            if (fs::exists(currentPath) && fs::is_directory(currentPath)) {
                bool anyFolders = false;
                
                // Iterasi folder dengan opsi skip_permission_denied
                for (const auto& entry : fs::directory_iterator(currentPath, fs::directory_options::skip_permission_denied)) {
                    // Hanya tampilkan direktori
                    if (entry.is_directory()) {
                        std::string dirName;
                        try { 
                            dirName = entry.path().filename().string(); 
                        } catch (const std::exception& e) {
                             LOG_WARN("Error reading filename: " + std::string(e.what()));
                             continue; 
                        } // Skip jika nama folder encoding error

                        // Skip folder sistem / hidden (biasanya diawali titik atau $)
                        if (dirName.empty() || dirName[0] == '$' || dirName[0] == '.') continue;

                        anyFolders = true;
                        std::string label =  dirName;
                        
                        // Style item list
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                        
                        // Seleksi folder (Masuk ke dalam)
                        if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0, 30))) {
                            currentPath = entry.path().string();
                        }
                        ImGui::PopStyleVar();
                        
                        // Tooltip path lengkap saat hover
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", entry.path().string().c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }
                
                if (!anyFolders) {
                    ImGui::Dummy(ImVec2(0, 50));
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 60);
                    ImGui::TextDisabled("No folders found");
                }
            } else {
                ImGui::Dummy(ImVec2(0, 50));
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 80);
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Invalid directory path");
            }
        } 
        catch (...) {
            // Catch-all block agar aplikasi TIDAK CRASH saat akses folder bermasalah
            ImGui::Dummy(ImVec2(0, 50));
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 60);
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Access Denied");
            LOG_ERROR("Exception in Folder Browser (Read Directory)");
        }
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // --- FOOTER BUTTONS ---
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));

        float buttonWidth = 180;
        // Posisikan tombol di kanan bawah
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonWidth * 2 - 30);
        
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        
        // Tombol Cancel
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
        if (ImGui::Button("CANCEL", ImVec2(buttonWidth, 40))) {
            showFolderBrowser = false;
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        
        // Tombol Select
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 1.00f, 1.0f));
        if (ImGui::Button("✓ SELECT THIS FOLDER", ImVec2(buttonWidth, 40))) {
            selectedPath = currentPath; // Simpan path ke variabel utama
            showFolderBrowser = false;  // Tutup browser
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::End();
    }
    
    // Kembalikan style rounding modal
    ImGui::PopStyleVar();
}

// ==================================================================================
//                          MODERN EDITOR WORKSPACE
// ==================================================================================

   void EditorUI::RenderEditorWorkspace(bool showCursor, float deltaTime, Camera& camera, ObjectPushConstant& selectedObject, std::vector<GameObject>& gameObjects, int& selectedIndex, VkDescriptorSet sceneTexture, std::function<void(int)> onSpawn, glm::vec2* outSceneSize, glm::vec2 textureSize) {
    
    // 1. Setup DockSpace
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // [FIX] Define Gizmo Variables
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    // 2. Main Menu Bar
    if (ImGui::BeginMainMenuBar()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.65f, 1.00f, 1.0f));
        ImGui::Text("COGENT ENGINE");
        ImGui::PopStyleColor();
        ImGui::Separator();
        
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) {}
            if (ImGui::MenuItem("Exit", "Alt+F4")) exit(0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Scene", NULL, true);
            ImGui::MenuItem("Inspector", NULL, true);
            ImGui::MenuItem("Hierarchy", NULL, true);
            ImGui::EndMenu();
        }
             
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 150);
        ImGui::TextDisabled("%.1f FPS", 1.0f / deltaTime);
        ImGui::EndMainMenuBar();
    }

    // 3. Toolbar (Top)
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 100);
        if(ImGui::Button("PLAY", ImVec2(60, 30))) {}
        ImGui::SameLine();
        if(ImGui::Button("STOP", ImVec2(60, 30))) {}
    ImGui::End();

    // 4. Scene View (The Central Node)
    // We render the texture here!
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0)); // No padding for image
    ImGui::Begin("Scene View", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        // Get Window Size for Aspect Ratio
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        
        // [FIX] Return Scene Size for Aspect Ratio Correction
        if (outSceneSize) {
            outSceneSize->x = windowSize.x;
            outSceneSize->y = windowSize.y;
        }



        if (sceneTexture) {
            // [FIX] UV Calculation for Dynamic Viewport
            // We only display the portion of the G-Buffer that we actually rendered to (the top-left corner)
            ImVec2 uv0 = ImVec2(0, 0);
            ImVec2 uv1 = ImVec2(1, 1);
            
            if (textureSize.x > 0 && textureSize.y > 0) {
                uv1.x = windowSize.x / textureSize.x;
                uv1.y = windowSize.y / textureSize.y;
                
                // Safety Clamp (Don't sample outside texture)
                if (uv1.x > 1.0f) uv1.x = 1.0f;
                if (uv1.y > 1.0f) uv1.y = 1.0f;
            }

            ImGui::Image((ImTextureID)sceneTexture, windowSize, uv0, uv1);
        } else {
             ImGui::TextDisabled("No Scene Texture Available");
        }
        
        // Gizmo Logic inside Scene Window
        if (showCursor) {
            ImGuizmo::SetDrawlist();
            ImGuizmo::Enable(true);
            
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 windowSize = ImGui::GetWindowSize();
            ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);

            // Calculate formatted Camera Matrices
            glm::mat4 viewMatrix = camera.getViewMatrix();
            glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), windowSize.x / windowSize.y, 0.1f, 1000.0f);
            projectionMatrix[1][1] *= -1; 

            ImGuizmo::Manipulate(
                glm::value_ptr(viewMatrix),
                glm::value_ptr(projectionMatrix),
                currentGizmoOperation,
                currentGizmoMode,
                glm::value_ptr(selectedObject.model)
            );
        }

        // [FIX] Spawn Context Menu (Inside Scene View)
        // Now safe because we are inside NewFrame() and inside a Window
        if (showCursor && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered()) {
            ImGui::OpenPopup("SpawnContext");
        }

        if (ImGui::BeginPopup("SpawnContext")) {
            ImGui::Text("Add Object");
            ImGui::Separator();
            
            if (ImGui::MenuItem("Cube"))    { if(onSpawn) onSpawn(0); }
            if (ImGui::MenuItem("Sphere"))  { if(onSpawn) onSpawn(1); }
            if (ImGui::MenuItem("Capsule")) { if(onSpawn) onSpawn(2); }
            ImGui::Separator();
            if (ImGui::MenuItem("Sun (Light)")) { if(onSpawn) onSpawn(3); } // [FIX] Add Sun
            
            ImGui::EndPopup();
        }

    ImGui::End();
    ImGui::PopStyleVar(); // [FIX] Pop WindowPadding pushed at line 888
    // Setup for Gizmo Logic
    // 5. Inspector Panel
    ImGui::Begin("Inspector");
        if (selectedObject.id != -1) {
            ImGui::SetWindowFontScale(1.2f);
            ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.00f, 1.0f), "INSPECTOR");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 10));
            
            // Transform Section
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Gizmo Operation Selection
                ImGui::Text("Gizmo:");
                if (ImGui::RadioButton("Trans", currentGizmoOperation == ImGuizmo::TRANSLATE)) currentGizmoOperation = ImGuizmo::TRANSLATE; ImGui::SameLine();
                if (ImGui::RadioButton("Rot", currentGizmoOperation == ImGuizmo::ROTATE)) currentGizmoOperation = ImGuizmo::ROTATE; ImGui::SameLine();
                if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE)) currentGizmoOperation = ImGuizmo::SCALE;
                ImGui::Dummy(ImVec2(0, 10));

                // Decompose matrix to display values in DragFloat
                float matrixTranslation[3], matrixRotation[3], matrixScale[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(selectedObject.model), matrixTranslation, matrixRotation, matrixScale);
                
                bool valuesChanged = false;
                ImGui::Text("Position");
                if (ImGui::DragFloat3("##pos", matrixTranslation, 0.1f)) valuesChanged = true;
                
                ImGui::Text("Rotation");
                if (ImGui::DragFloat3("##rot", matrixRotation, 0.1f)) valuesChanged = true;
                
                ImGui::Text("Scale");
                if (ImGui::DragFloat3("##scl", matrixScale, 0.1f)) valuesChanged = true;
                
                // Recompose matrix if values changed via UI
                if (valuesChanged) {
                    ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, glm::value_ptr(selectedObject.model));
                }
            }
            
            ImGui::Dummy(ImVec2(0, 20));

            // Material Section
            if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Albedo Color");
                ImGui::ColorPicker4("##picker", glm::value_ptr(selectedObject.color), ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
            }
        } else {
            ImGui::TextDisabled("No object selected.");
        }
    ImGui::End();

    // 6. Hierarchy Panel
    RenderHierarchy(gameObjects, selectedIndex);
}

// Tambahkan parameter list object dan index yang dipilih
void EditorUI::RenderHierarchy(std::vector<GameObject>& objects, int& selectedIndex) {
    ImGui::Begin("Hierarchy");
    // Loop semua object
    for (int i = 0; i < objects.size(); i++) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selectedIndex == i) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", objects[i].name.c_str());

        if (ImGui::IsItemClicked()) {
            selectedIndex = i;
        }
    }
    // Klik area kosong untuk unselect
    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
        selectedIndex = -1;
    }
    ImGui::End();
}