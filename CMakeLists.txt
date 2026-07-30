cmake_minimum_required(VERSION 3.15)
project(COGENT_Engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fix MSVC warnings and mismatched runtimes
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Generate embedded shaders
execute_process(
    COMMAND python embed_shaders.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# 1. Vulkan & Libraries
find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

# Setup Path SDK Manual
set(GLFW_INCLUDE_DIR "D:/SDK/GLFW/glfw-3.4.bin.WIN64/include")
set(GLFW_LIBRARY_DIR "D:/SDK/GLFW/glfw-3.4.bin.WIN64/lib-vc2022")
set(TINYOBJ_INCLUDE_DIR "D:/SDK/tinyobjloader")
set(GLM_INCLUDE_DIR "D:/SDK/GLM")

# 2. DEFINISIKAN SUMBER KODE (Source Files)
# Kita harus sebutkan path lengkapnya sekarang
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/EmbeddedShaders.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/GBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/RenderPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/RenderPipeline.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/LightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/LightingPass.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Streaming/Streamer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/VulkanUtils.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Camera.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Logger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Memory/MemoryManager.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Graphics/DescriptorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Graphics/PipelineCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Diagnostics/GpuProfiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Graphics/ShaderSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/Graph/RenderGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/HDRPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/DeferredLightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/ShadowPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/Lighting/LightCulling.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/ScreenSpaceShadows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/SSAO.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/TAAPass.cpp
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
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/VMA"
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    "${GLFW_LIBRARY_DIR}/glfw3.lib"
)

add_executable(test_glfw test_glfw.cpp)
target_link_libraries(test_glfw PRIVATE Vulkan::Vulkan "${GLFW_LIBRARY_DIR}/glfw3.lib")
target_include_directories(test_glfw PRIVATE "${GLFW_INCLUDE_DIR}")

add_executable(test_cts test_cts.cpp)
target_include_directories(test_cts PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_cts PRIVATE Threads::Threads)

add_executable(test_res test_res.cpp 
    Resources/Texture.cpp 
    Resources/Streaming/Streamer.cpp
    Core/Logger.hpp)
target_include_directories(test_res PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Graphics"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/VMA"
    "${GLM_INCLUDE_DIR}"
    "${TINYOBJ_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Geometry"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scene"
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(test_res PRIVATE Threads::Threads Vulkan::Vulkan)

add_executable(test_visibility test_visibility.cpp 
    Renderer/Visibility/VisibilitySystem.cpp
    Core/Logger.hpp)
target_include_directories(test_visibility PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GLM_INCLUDE_DIR}"
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(test_visibility PRIVATE Threads::Threads)

add_executable(test_ai test_ai.cpp Core/Logger.hpp)
target_include_directories(test_ai PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_ai PRIVATE Threads::Threads)

add_executable(test_performance test_performance.cpp Core/Diagnostics/FramePacer.cpp)
target_include_directories(test_performance PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_performance PRIVATE Threads::Threads)

add_executable(test_raytracing test_raytracing.cpp RayTracing/BVHBuilder.cpp)
target_include_directories(test_raytracing PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${GLM_INCLUDE_DIR}" ${Vulkan_INCLUDE_DIRS})
target_link_libraries(test_raytracing PRIVATE Threads::Threads Vulkan::Vulkan)

add_executable(test_nai_runtime test_nai_runtime.cpp AI/Runtime/NAIRuntime.cpp)
target_include_directories(test_nai_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_nai_runtime PRIVATE Threads::Threads)

add_executable(test_framegen test_framegen.cpp Renderer/NFG/NFGPass.cpp AI/Runtime/NAIRuntime.cpp Core/Diagnostics/FramePacer.cpp)
target_include_directories(test_framegen PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_framegen PRIVATE Threads::Threads)