cmake_minimum_required(VERSION 3.10)
project(COGENT_Engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Vulkan & Libraries
find_package(Vulkan REQUIRED)

# Setup Path SDK Manual
set(GLFW_INCLUDE_DIR "D:/SDK/GLFW/glfw-3.4.bin.WIN64/include")
set(GLFW_LIBRARY_DIR "D:/SDK/GLFW/glfw-3.4.bin.WIN64/lib-vc2022")
set(TINYOBJ_INCLUDE_DIR "D:/SDK/tinyobjloader")
set(GLM_INCLUDE_DIR "D:/SDK/GLM")

# 2. DEFINISIKAN SUMBER KODE (Source Files)
# Kita harus sebutkan path lengkapnya sekarang
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/GBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/RenderPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/RenderPipeline.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/LightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/LightingPass.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/VulkanUtils.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Camera.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Logger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/imgui_impl_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Editor/EditorUI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vendor/ImGuizmo/ImGuizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Geometry/PrimitiveMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Geometry/PrimitiveMesh.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RayTracing/RayTracer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/CogentEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Graphics/GraphicsDevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Graphics/Swapchain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Diagnostics/GpuProfiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/Graph/RenderGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/DeferredLightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/ShadowPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/ShadowPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/ScreenSpaceShadows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/Visibility/VisibilitySystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/InstanceBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/ANSR/ANSRPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/PostProcess/AutoExposurePass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/VulkanUtils.cpp
)

# 3. Create Executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# 4. INCLUDE DIRECTORIES (Agar #include tidak error)
# Ini trik agar kamu tetap bisa nulis #include "GBuffer.hpp" tanpa nulis folder-nya
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${Vulkan_INCLUDE_DIRS} 
    "${GLFW_INCLUDE_DIR}"
    "${TINYOBJ_INCLUDE_DIR}"
    "${GLM_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Graphics"
    "${CMAKE_CURRENT_SOURCE_DIR}/Renderer"
    "${CMAKE_CURRENT_SOURCE_DIR}/Renderer/Graph"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/ImGuizmo"
    "${CMAKE_CURRENT_SOURCE_DIR}/Editor"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scene"
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene
    ${CMAKE_CURRENT_SOURCE_DIR}/Geometry
    "${CMAKE_CURRENT_SOURCE_DIR}/RayTracing"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
)

# 5. LINK LIBRARIES
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    "${GLFW_LIBRARY_DIR}/glfw3.lib"
)

# Copy Shaders ke folder Build otomatis (Opsional tapi berguna)
file(COPY Shaders DESTINATION ${CMAKE_BINARY_DIR})