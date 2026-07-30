# 02 — ARCHITECTURE DOCUMENT

## COGENT ENGINE — System Architecture Specification

**Version**: 1.0.0  
**Classification**: Internal — Engineering Bible  
**Last Updated**: 2026-07-30  
**Document Owner**: Engine Architecture Team  

---

## Table of Contents

1. [Tujuan (Purpose)](#1-tujuan-purpose)
2. [Scope](#2-scope)
3. [Dependency](#3-dependency)
4. [Requirement](#4-requirement)
5. [Architecture Philosophy](#5-architecture-philosophy)
6. [Layered Architecture](#6-layered-architecture)
7. [Module Architecture](#7-module-architecture)
8. [Data Flow](#8-data-flow)
9. [Module Flow](#9-module-flow)
10. [UML — Complete Class Hierarchy](#10-uml--complete-class-hierarchy)
11. [Flowchart — Engine Lifecycle](#11-flowchart--engine-lifecycle)
12. [Sequence Diagram — Frame Pipeline](#12-sequence-diagram--frame-pipeline)
13. [State Diagram — Engine States](#13-state-diagram--engine-states)
14. [Interface Design](#14-interface-design)
15. [Folder Structure](#15-folder-structure)
16. [Pseudocode — Core Algorithms](#16-pseudocode--core-algorithms)
17. [API Contract](#17-api-contract)
18. [Dependency Graph](#18-dependency-graph)
19. [Advantages and Disadvantages](#19-advantages-and-disadvantages)
20. [Risk](#20-risk)
21. [Mitigation](#21-mitigation)
22. [Future Improvement](#22-future-improvement)

---

## 1. Tujuan (Purpose)

This document defines the complete software architecture of COGENT ENGINE. It serves as the authoritative reference for:

- How the engine is structured at macro and micro levels
- How subsystems communicate and depend on each other
- Design patterns and principles governing all implementations
- Module boundaries and interface contracts

All engineering decisions MUST align with the architecture defined here.

---

## 2. Scope

This document covers the architectural design of all systems across Phase 0–11:

| Layer | Systems |
|-------|---------|
| Foundation | NMM, CTS, Resource Manager, Asset Streaming, Logger, Crash Reporter, Profiler |
| Rendering | Vulkan Backend, NX Render, NX Graph, Deferred Pipeline, Forward+, Shadows, TAA, HDR |
| Optimization | NVS (Visibility), AOS (Optimizer), NDR, NSR, NRT, NFG |
| AI | NAI (ONNX, TensorRT, Inference Scheduler) |
| Scripting | NVS Studio (Nodes, Compiler, VM) |
| Telemetry | NX Insight, NX Compare, HAS |
| Editor | EditorUI (ImGui, ImGuizmo) |

---

## 3. Dependency

### Third-Party Dependency Map

```mermaid
graph LR
    subgraph "COGENT ENGINE"
        ENGINE[Engine Core]
        RENDER[Renderer]
        AI_SYS[AI System]
        EDITOR_SYS[Editor]
    end

    subgraph "Graphics"
        VULKAN[Vulkan SDK 1.3]
        VMA_D[VMA]
        GLSLC[glslc Compiler]
    end

    subgraph "Windowing"
        GLFW_D[GLFW 3.4]
    end

    subgraph "Math"
        GLM_D[GLM]
    end

    subgraph "UI"
        IMGUI_D[Dear ImGui]
        IMGUIZMO_D[ImGuizmo]
    end

    subgraph "Assets"
        TINYOBJ[tinyobjloader]
        STB[stb_image]
    end

    subgraph "AI Libraries"
        ONNX_D[ONNX Runtime]
        TRT[TensorRT]
    end

    ENGINE --> VULKAN
    ENGINE --> GLFW_D
    ENGINE --> GLM_D
    RENDER --> VULKAN
    RENDER --> VMA_D
    RENDER --> GLSLC
    AI_SYS --> ONNX_D
    AI_SYS --> TRT
    EDITOR_SYS --> IMGUI_D
    EDITOR_SYS --> IMGUIZMO_D
    ENGINE --> TINYOBJ
    ENGINE --> STB
```

---

## 4. Requirement

| Requirement | Specification |
|------------|---------------|
| Language Standard | C++17 (C++20 future migration) |
| Build System | CMake 3.15+ |
| Graphics API | Vulkan 1.3+ |
| Target OS | Windows 10/11 (Linux future) |
| Compiler | MSVC 2022 (17.0+) |
| GPU | Vulkan 1.3 capable, 4+ GB VRAM |
| Memory | 16 GB RAM minimum |

---

## 5. Architecture Philosophy

### 5.1 Core Principles

| Principle | Description | Application |
|-----------|-------------|-------------|
| **Single Responsibility** | Each class/module has one reason to change | `GBuffer` only manages render targets, `DeferredLightingPass` only does lighting |
| **Dependency Inversion** | High-level modules don't depend on low-level details | `Renderer` depends on `GraphicsDevice` interface, not Vulkan calls directly |
| **Interface Segregation** | No client should depend on methods it doesn't use | Separate `Allocator` base from specific allocator types |
| **Open/Closed** | Open for extension, closed for modification | Render passes added via `RenderGraph::addPass()`, not by modifying graph internals |
| **Composition over Inheritance** | Prefer composition and aggregation | `CogentEngine` composes `GBuffer`, `RenderGraph`, `TAAPass` etc. as `unique_ptr` members |

### 5.2 Design Patterns Used

| Pattern | Where Used | Rationale |
|---------|-----------|-----------|
| **Singleton** | `Logger`, `Profiler`, `JobSystem`, `ResourceManager` | Global access to engine-wide services |
| **Factory** | `PipelineCache::buildGraphicsPipeline` | Named pipeline creation with caching |
| **Builder** | `DescriptorBuilder` | Fluent API for descriptor set construction |
| **Observer** | `Logger::AddCallback` | EditorUI subscribes to log messages |
| **Strategy** | `Allocator` hierarchy | Swap allocation strategy (Linear/Pool/Stack) |
| **Command** | `RenderPassNode::execute` | Deferred execution of render commands |
| **State** | `AppState` enum | Engine lifecycle state management |
| **Object Pool** | `PoolAllocator`, `DescriptorAllocator` | Recycled fixed-size objects / descriptors |
| **Frame Graph** | `RenderGraph` | Data-driven render pass scheduling |

### 5.3 Architectural Constraints

1. **No raw `new`/`delete`**: All allocations through NMM allocators or `std::unique_ptr`/`std::shared_ptr`
2. **No blocking on main thread**: All heavy work dispatched to CTS worker threads
3. **No hardcoded render passes**: All passes registered through RenderGraph
4. **GPU resources owned by creating module**: Module that creates a `VkImage` is responsible for destroying it
5. **Thread safety via design**: Minimize shared state; where unavoidable, use `std::mutex` or atomics

---

## 6. Layered Architecture

### 6.1 Layer Diagram

```mermaid
graph TB
    subgraph "Layer 5: Application"
        APP[Game Logic]
        EDITOR_L[Editor UI]
    end

    subgraph "Layer 4: Engine Services"
        ENGINE_L[CogentEngine]
        SCENE_L[Scene Graph]
        SCRIPT_L[Visual Scripting VM]
        TELEM[Telemetry]
    end

    subgraph "Layer 3: Rendering & Optimization"
        RENDERER_L[NX Render + NX Graph]
        VIS_L[NVS Visibility]
        OPT_L[AOS Optimizer]
        PERF_L[NDR + NSR + NFG]
        RT_L[NRT Ray Tracing]
    end

    subgraph "Layer 2: Core Systems"
        MEM_L[NMM Memory]
        TASK_L[CTS Task Scheduler]
        RES_L[Resource Manager + Streaming]
        DIAG_L[Logger + Profiler + Crash Reporter]
        HW_L[HAS Hardware Awareness]
    end

    subgraph "Layer 1: Platform Abstraction"
        VK_L[Vulkan Backend]
        WIN_L[Window System - GLFW]
        OS_LAYER[OS Layer - Windows API]
    end

    subgraph "Layer 0: Hardware"
        GPU_HW[GPU]
        CPU_HW[CPU]
        RAM_HW[System Memory]
    end

    APP --> ENGINE_L
    EDITOR_L --> ENGINE_L
    ENGINE_L --> SCENE_L
    ENGINE_L --> SCRIPT_L
    ENGINE_L --> TELEM

    ENGINE_L --> RENDERER_L
    RENDERER_L --> VIS_L
    RENDERER_L --> OPT_L
    OPT_L --> PERF_L
    RENDERER_L --> RT_L

    RENDERER_L --> MEM_L
    RENDERER_L --> TASK_L
    RENDERER_L --> RES_L
    ENGINE_L --> DIAG_L
    ENGINE_L --> HW_L

    MEM_L --> VK_L
    VK_L --> GPU_HW
    TASK_L --> OS_LAYER
    OS_LAYER --> CPU_HW
    WIN_L --> OS_LAYER
    MEM_L --> RAM_HW
```

### 6.2 Layer Communication Rules

| Rule | Description |
|------|-------------|
| **Downward Only** | Layer N may only depend on Layer N-1 or lower |
| **No Upward Calls** | Lower layers never call higher layers directly |
| **Callback Exception** | Lower layers may invoke callbacks registered by higher layers (e.g., Logger callbacks) |
| **Horizontal Allowed** | Modules within the same layer may communicate |
| **Interface Boundary** | Cross-layer communication through defined interfaces only |

---

## 7. Module Architecture

### 7.1 Module Decomposition

```mermaid
graph TD
    subgraph "CogentEngine Module"
        CE[CogentEngine Class]
    end

    subgraph "Graphics Module"
        GD[GraphicsDevice]
        SC[Swapchain]
        DM[DescriptorManager]
        PC[PipelineCache]
        SS[ShaderSystem]
    end

    subgraph "Renderer Module"
        NXR_M[NX Render Coordinator]
        RG_M[RenderGraph]
        GB_M[GBuffer]
        SP_M[ShadowPass]
        DLP_M[DeferredLightingPass]
        SSAO_M[SSAO]
        SSS_M[ScreenSpaceShadows]
        TAA_M[TAAPass]
        HDR_M[HDRPipeline]
        LC_M[LightCulling]
        AE_M[AutoExposure]
    end

    subgraph "Visibility Module"
        VS_M[VisibilitySystem]
        FR_M[Frustum]
        HZB_M[HZB System]
        ML_M[MeshletBuilder]
    end

    subgraph "Optimization Module"
        OPT_M[OptimizerAI]
        SA_M[SceneAnalyzer]
        ANSR_M[ANSRPass]
    end

    subgraph "AI Module"
        NAI_M[AI Runtime]
        ONNX_M[ONNX Backend]
        TRT_M[TensorRT Backend]
    end

    subgraph "Resource Module"
        RM_M[ResourceManager]
        STR_M[Streamer]
        TEX_M[Texture]
        MDL_M[Model]
    end

    subgraph "Memory Module"
        ALLOC_M[Allocator Base]
        LIN_M[LinearAllocator]
        POOL_M[PoolAllocator]
    end

    subgraph "Threading Module"
        JOB_M[JobSystem]
    end

    subgraph "Diagnostics Module"
        LOG_M[Logger]
        PROF_M[Profiler]
        GPROF_M[GpuProfiler]
    end

    CE --> GD
    CE --> RG_M
    CE --> GB_M
    CE --> OPT_M
    CE --> VS_M

    GD --> SC
    GD --> DM
    GD --> PC
    GD --> SS

    RG_M --> GB_M
    RG_M --> SP_M
    RG_M --> DLP_M
    RG_M --> SSAO_M
    RG_M --> TAA_M
    RG_M --> HDR_M
    RG_M --> LC_M

    VS_M --> FR_M
    VS_M --> ML_M

    OPT_M --> SA_M
    OPT_M --> ANSR_M
```

### 7.2 Module Ownership

| Module | Owner Scope | Creates | Destroys |
|--------|------------|---------|----------|
| `GraphicsDevice` | VkInstance, VkDevice, VkQueue, VmaAllocator | On `init()` | On `cleanup()` |
| `Swapchain` | VkSwapchainKHR, VkImageView[], VkFramebuffer[] | On construct/`recreate()` | On destruct |
| `GBuffer` | 6 VkImage + VkImageView + VkDeviceMemory | On `init()` / `resize()` | On `cleanup()` / destruct |
| `ShadowPass` | Shadow VkImage array, VkFramebuffer per cascade | On `init()` | On destruct |
| `TAAPass` | 2 ping-pong VkImage, VkPipeline | On `init()` / `resize()` | On destruct |
| `HDRPipeline` | HDR target, Bloom mip chain, Tonemap target | On `init()` / `resize()` | On destruct |
| `SSAO` | Raw + Blur targets, Noise texture, Kernel buffer | On `init()` / `resize()` | On destruct |
| `ANSRPass` | Display + History VkImage | On construct / `resize()` | On destruct |
| `RayTracer` | Storage image, Uniform + Sphere buffers | On `init()` | On `cleanup()` |

---

## 8. Data Flow

### 8.1 Main Engine Data Flow

```mermaid
flowchart TD
    INPUT[User Input - GLFW] --> CAMERA[Camera Update]
    CAMERA --> UBO[Update Uniform Buffer]
    UBO --> CULL[Visibility Culling]
    CULL --> DRAW_LIST[Visible Object List]
    DRAW_LIST --> CMD[Command Buffer Recording]
    CMD --> GPU_SUB[GPU Submission]
    GPU_SUB --> PRESENT_D[Swapchain Present]
    PRESENT_D --> DISPLAY[Display Output]

    subgraph "Per-Frame Data"
        UBO -->|CameraUBO| SHADER_BIND[Shader Binding]
        DRAW_LIST -->|PushConstants| SHADER_BIND
        SHADER_BIND --> CMD
    end
```

### 8.2 Resource Data Flow

```mermaid
flowchart TD
    REQUEST[Resource Request] --> CACHE_CHECK{In Cache?}
    CACHE_CHECK -->|Yes| RETURN[Return Cached]
    CACHE_CHECK -->|No| CREATE[Create Resource Object]
    CREATE --> STREAM_Q[Queue to Streamer]
    STREAM_Q --> ASYNC_LOAD[Async File Read - Worker Thread]
    ASYNC_LOAD --> GPU_UPLOAD[GPU Upload - Staging Buffer]
    GPU_UPLOAD --> READY[Resource Ready]
    READY --> RETURN
```

### 8.3 Rendering Data Flow

```mermaid
flowchart LR
    subgraph "Input"
        VERTICES[Vertex Data]
        TEXTURES[Textures]
        LIGHTS[Light Data]
        CAMERA_D[Camera UBO]
    end

    subgraph "Processing"
        SHADOW_R[Shadow Rendering]
        GBUFFER_R[G-Buffer Fill]
        SSAO_R[SSAO Generation]
        LIGHTING_R[Deferred Lighting]
        TAA_R[TAA Resolve]
        BLOOM_R[Bloom]
        TONEMAP_R[Tonemapping]
    end

    subgraph "Output"
        SHADOW_MAP[Shadow Maps]
        GBUF_TEX[G-Buffer Textures]
        AO_MASK[AO Mask]
        HDR_OUT[HDR Frame]
        LDR_OUT[LDR Frame]
    end

    VERTICES --> GBUFFER_R
    TEXTURES --> GBUFFER_R
    CAMERA_D --> SHADOW_R
    CAMERA_D --> GBUFFER_R
    LIGHTS --> LIGHTING_R

    SHADOW_R --> SHADOW_MAP
    GBUFFER_R --> GBUF_TEX
    GBUF_TEX --> SSAO_R
    SSAO_R --> AO_MASK
    GBUF_TEX --> LIGHTING_R
    AO_MASK --> LIGHTING_R
    SHADOW_MAP --> LIGHTING_R
    LIGHTING_R --> HDR_OUT
    HDR_OUT --> TAA_R
    TAA_R --> BLOOM_R
    BLOOM_R --> TONEMAP_R
    TONEMAP_R --> LDR_OUT
```

---

## 9. Module Flow

### 9.1 Engine Initialization Flow

```mermaid
flowchart TD
    START[main()] --> CREATE_ENGINE[Create CogentEngine]
    CREATE_ENGINE --> INIT_WINDOW[initWindow - GLFW]
    INIT_WINDOW --> INIT_VULKAN[initVulkan]
    
    INIT_VULKAN --> CREATE_SURFACE[createSurface]
    CREATE_SURFACE --> INIT_DEVICE[GraphicsDevice::init]
    INIT_DEVICE --> CREATE_SWAP[createSwapchain]
    CREATE_SWAP --> CREATE_VIEWS[createSwapchainImageViews]
    CREATE_VIEWS --> CREATE_CMD[createCommandBuffer]
    CREATE_CMD --> CREATE_SYNC[createSyncObjects]
    CREATE_SYNC --> INIT_SUBSYS[Initialize Subsystems]

    INIT_SUBSYS --> INIT_SHADER[ShaderSystem::init]
    INIT_SHADER --> INIT_PIPE[PipelineCache::init]
    INIT_PIPE --> INIT_GBUF[GBuffer::init]
    INIT_GBUF --> INIT_SHADOW[ShadowPass::init]
    INIT_SHADOW --> INIT_SSAO[SSAO::init]
    INIT_SSAO --> INIT_HDR[HDRPipeline::init]
    INIT_HDR --> INIT_TAA[TAAPass::init]
    INIT_TAA --> INIT_DEFERRED[DeferredLightingPass::init]
    INIT_DEFERRED --> INIT_LC[LightCulling::init]
    INIT_LC --> INIT_RES[initResources]

    INIT_RES --> INIT_IMGUI[ImGui::Init]
    INIT_IMGUI --> READY[Engine Ready → mainLoop]
```

### 9.2 Frame Execution Flow

```mermaid
flowchart TD
    LOOP_START[mainLoop - Per Frame] --> POLL[glfwPollEvents]
    POLL --> TIME[Calculate deltaTime]
    TIME --> INPUT_PROC[Process Input]
    INPUT_PROC --> UPDATE_CAM[updateCamera]
    UPDATE_CAM --> UPDATE_UBO[updateUniformBuffer]
    UPDATE_UBO --> DRAW[drawFrame]
    
    DRAW --> WAIT_FENCE[vkWaitForFences]
    WAIT_FENCE --> ACQUIRE[vkAcquireNextImageKHR]
    ACQUIRE --> RESET_FENCE[vkResetFences]
    RESET_FENCE --> BEGIN_CMD[vkBeginCommandBuffer]
    BEGIN_CMD --> RECORD[recordCommandBuffer]

    RECORD --> BUILD_RG[buildRenderGraph]
    BUILD_RG --> EXEC_RG[renderGraph→execute]
    EXEC_RG --> IMGUI_DRAW[ImGui Render]
    IMGUI_DRAW --> END_CMD[vkEndCommandBuffer]
    END_CMD --> SUBMIT[vkQueueSubmit]
    SUBMIT --> PRESENT_F[vkQueuePresentKHR]
    PRESENT_F --> CHECK{Window closed?}
    CHECK -->|No| LOOP_START
    CHECK -->|Yes| CLEANUP[cleanup]
```

---

## 10. UML — Complete Class Hierarchy

### 10.1 Core Systems UML

```mermaid
classDiagram
    class CogentEngine {
        -GLFWwindow* window
        -GraphicsDevice graphicsDevice
        -GBuffer gBuffer
        -RenderGraph* renderGraph
        -TAAPass* taaPass
        -HDRPipeline* hdrPipeline
        -SSAO* ssao
        -ShadowPass shadowPass
        -OptimizerAI optimizerAI
        -EditorUI editorUI
        -Camera mainCamera
        -vector~GameObject~ gameObjects
        +run() void
        -initWindow() void
        -initVulkan() void
        -mainLoop() void
        -cleanup() void
        -drawFrame() void
    }

    class GraphicsDevice {
        -VkInstance instance
        -VkPhysicalDevice physicalDevice
        -VkDevice device
        -VmaAllocator allocator
        +init(surface) void
        +cleanup() void
        +getDevice() VkDevice
        +getAllocator() VmaAllocator
    }

    class Camera {
        +vec3 Position
        +vec3 Front
        +vec3 Up
        +float Yaw
        +float Pitch
        +float Speed
        +float Sensitivity
        +float Zoom
        +GetViewMatrix() mat4
        +ProcessKeyboard(direction, dt) void
        +ProcessMouseMovement(xoff, yoff) void
    }

    class EditorUI {
        -bool showHierarchy
        -bool showInspector
        -bool showConsole
        -bool showProfiler
        +init(device, renderPass, ...) void
        +render(cmd) void
        +cleanup() void
    }

    CogentEngine --> GraphicsDevice
    CogentEngine --> Camera
    CogentEngine --> EditorUI
```

### 10.2 Memory System UML

```mermaid
classDiagram
    class Allocator {
        <<abstract>>
        #void* _start
        #size_t _size
        #size_t _used_memory
        #size_t _num_allocations
        +allocate(size, alignment)* void*
        +deallocate(p)* void
        +clear()* void
        +getSize() size_t
        +getUsedMemory() size_t
    }

    class LinearAllocator {
        -size_t _offset
        +allocate(size, alignment) void*
        +deallocate(p) void
        +clear() void
    }

    class PoolAllocator {
        -size_t _objectSize
        -uint8_t _objectAlignment
        -void** _freeList
        +allocate(size, alignment) void*
        +deallocate(p) void
        +clear() void
    }

    Allocator <|-- LinearAllocator
    Allocator <|-- PoolAllocator

    note for LinearAllocator "Frame scratch memory\nReset every frame\nO(1) alloc, no individual free"
    note for PoolAllocator "Fixed-size objects\nFree list recycling\nO(1) alloc and free"
```

### 10.3 Rendering System UML

```mermaid
classDiagram
    class RenderGraph {
        -vector~RenderPassNode~ passes
        -map~string,RenderGraphResource~ resources
        +registerImage(name, ...) void
        +addPass(node) void
        +compile() void
        +execute(cmd, imageIndex) void
    }

    class GBuffer {
        -FramebufferAttachment position
        -FramebufferAttachment normal
        -FramebufferAttachment albedo
        -FramebufferAttachment material
        -FramebufferAttachment velocity
        -FramebufferAttachment depth
        +getRenderPass() VkRenderPass
        +getFramebuffer() VkFramebuffer
        +resize(w, h) void
    }

    class ShadowPass {
        -Cascade cascades[3]
        -VkImage shadowImage
        +init() void
        +updateCascades(view, proj, lightDir) void
        +getRenderPass() VkRenderPass
    }

    class DeferredLightingPass {
        -VkPipeline pipeline
        +init(globalLayout, clusterLayout) void
        +updateDescriptorSets(gbuffer, ssaoMask) void
        +execute(cmd, globalDesc, clusterDesc) void
    }

    class TAAPass {
        -VkImage taaImages[2]
        -uint32_t currentFrameIdx
        +init(extent) void
        +dispatch(cmd, w, h) void
        +getOutputView() VkImageView
    }

    class HDRPipeline {
        -VkImage hdrImage
        -VkImage bloomImage
        -VkImage tonemappedImage
        +executeBloom(cmd) void
        +executeTonemap(cmd, extent) void
    }

    class SSAO {
        -VkImage ssaoRawImage
        -VkImage ssaoBlurImage
        -vector~vec4~ ssaoKernel
        +executeSSAO(cmd) void
        +executeSSAOBlur(cmd) void
    }

    class LightCulling {
        +init() void
        +execute(cmd) void
        +getClusterDescriptorSet() VkDescriptorSet
    }

    RenderGraph --> GBuffer
    RenderGraph --> ShadowPass
    RenderGraph --> DeferredLightingPass
    RenderGraph --> TAAPass
    RenderGraph --> HDRPipeline
    RenderGraph --> SSAO
    RenderGraph --> LightCulling
```

### 10.4 Resource System UML

```mermaid
classDiagram
    class ResourceManager {
        -map~string, shared_ptr~Texture~~ _textures
        -Streamer* _streamer
        +Get() ResourceManager&
        +Init(device, ...) void
        +GetTexture(path) shared_ptr~Texture~
    }

    class Streamer {
        -vector~StreamRequest~ _pendingRequests
        +registerResource(resource) void
        +requestLoad(resource) void
        +update(cameraPos, deltaTime) void
    }

    class Texture {
        +string path
        +VkImage image
        +VkImageView view
        +VkDeviceMemory memory
        +load(...) void
        +cleanup(device) void
    }

    class Model {
        +vector~Vertex~ vertices
        +vector~uint32_t~ indices
        +VkBuffer vertexBuffer
        +VkBuffer indexBuffer
        +load(path) void
    }

    ResourceManager --> Streamer
    ResourceManager --> Texture
    ResourceManager --> Model
```

---

## 11. Flowchart — Engine Lifecycle

```mermaid
flowchart TD
    BIRTH[Application Start] --> CONSTRUCT[Construct CogentEngine]
    CONSTRUCT --> INIT[Initialize All Systems]
    INIT --> MAIN_LOOP{Main Loop}
    
    MAIN_LOOP -->|Frame| PROCESS[Process Frame]
    PROCESS --> POLL_E[Poll Events]
    POLL_E --> UPDATE[Update Logic]
    UPDATE --> RENDER_F[Render Frame]
    RENDER_F --> PRESENT_L[Present]
    PRESENT_L --> CHECK_CLOSE{Close Requested?}
    CHECK_CLOSE -->|No| MAIN_LOOP
    CHECK_CLOSE -->|Yes| SHUTDOWN

    SHUTDOWN[Shutdown] --> WAIT_GPU[vkDeviceWaitIdle]
    WAIT_GPU --> DESTROY_SUBSYS[Destroy Subsystems - Reverse Order]
    DESTROY_SUBSYS --> DESTROY_VK[Destroy Vulkan Resources]
    DESTROY_VK --> DESTROY_WIN[Destroy Window]
    DESTROY_WIN --> EXIT[Application Exit]
```

---

## 12. Sequence Diagram — Frame Pipeline

### 12.1 Complete Frame Sequence

```mermaid
sequenceDiagram
    participant App as CogentEngine
    participant VK as Vulkan
    participant Swap as Swapchain
    participant RG as RenderGraph
    participant Shadow as ShadowPass
    participant GBuf as GBuffer
    participant SSAO_S as SSAO
    participant SSS_S as ScreenSpaceShadows
    participant LC as LightCulling
    participant DL as DeferredLighting
    participant TAA_S as TAAPass
    participant HDR_S as HDRPipeline
    participant UI as EditorUI

    App->>VK: vkWaitForFences(inFlightFence)
    App->>Swap: acquireNextImage(imageAvailableSem)
    Swap-->>App: imageIndex
    App->>VK: vkResetFences(inFlightFence)
    App->>VK: vkBeginCommandBuffer(cmd)
    
    App->>RG: execute(cmd, imageIndex)
    
    RG->>Shadow: execute(cmd) — 3 cascades
    Shadow-->>RG: Shadow maps written
    
    RG->>GBuf: beginRenderPass + drawGeometry
    GBuf-->>RG: G-Buffer filled (Pos,Norm,Albedo,Mat,Vel,Depth)
    
    RG->>SSAO_S: executeSSAO(cmd)
    SSAO_S-->>RG: Raw AO
    RG->>SSAO_S: executeSSAOBlur(cmd)
    SSAO_S-->>RG: Blurred AO mask
    
    RG->>SSS_S: execute(cmd, view, proj, lightDir)
    SSS_S-->>RG: Contact shadow mask
    
    RG->>LC: execute(cmd)
    LC-->>RG: Cluster-light assignments
    
    RG->>DL: execute(cmd, globalDesc, clusterDesc)
    DL-->>RG: HDR lit output
    
    RG->>TAA_S: dispatch(cmd, w, h)
    TAA_S-->>RG: Anti-aliased output
    
    RG->>HDR_S: executeBloom(cmd)
    HDR_S-->>RG: Bloom applied
    RG->>HDR_S: executeTonemap(cmd, extent)
    HDR_S-->>RG: LDR tonemapped output
    
    App->>UI: render(cmd) — ImGui draw
    
    App->>VK: vkEndCommandBuffer(cmd)
    App->>VK: vkQueueSubmit(graphicsQueue, cmd, fences, sems)
    App->>Swap: present(renderFinishedSem)
```

---

## 13. State Diagram — Engine States

```mermaid
stateDiagram-v2
    [*] --> UNINITIALIZED
    UNINITIALIZED --> INITIALIZING: run()
    INITIALIZING --> LOADING: initVulkan() complete
    LOADING --> RUNNING: initResources() complete
    RUNNING --> PAUSED: Alt-Tab / minimize
    PAUSED --> RUNNING: Focus restored
    RUNNING --> RESIZING: Window resize
    RESIZING --> RUNNING: recreateSwapchain()
    RUNNING --> SHUTTING_DOWN: Close requested
    SHUTTING_DOWN --> [*]: cleanup() complete

    state RUNNING {
        [*] --> FRAME_START
        FRAME_START --> POLL_EVENTS
        POLL_EVENTS --> UPDATE_STATE
        UPDATE_STATE --> RENDER
        RENDER --> PRESENT
        PRESENT --> FRAME_START
    }
```

---

## 14. Interface Design

### 14.1 Core Interfaces

```cpp
// === IAllocator === (Abstract interface for all allocators)
class IAllocator {
public:
    virtual void* allocate(size_t size, uint8_t alignment = 8) = 0;
    virtual void deallocate(void* p) = 0;
    virtual void clear() = 0;
    virtual size_t getUsedMemory() const = 0;
    virtual ~IAllocator() = default;
};

// === IRenderPass === (Interface for render graph passes)
struct IRenderPass {
    std::string name;
    std::vector<RenderPassResourceInfo> inputs;
    std::vector<RenderPassResourceInfo> outputs;
    std::function<void(VkCommandBuffer, uint32_t)> execute;
};

// === IStreamable === (Interface for streamable resources)
class IStreamable {
public:
    virtual void load() = 0;
    virtual void unload() = 0;
    virtual float getPriority(const glm::vec3& cameraPos) const = 0;
    virtual bool isLoaded() const = 0;
    virtual ~IStreamable() = default;
};
```

### 14.2 Subsystem Communication

| Source | Target | Mechanism | Data |
|--------|--------|-----------|------|
| `CogentEngine` | `GraphicsDevice` | Direct member | Vulkan handles |
| `CogentEngine` | `RenderGraph` | `unique_ptr` | Pass nodes |
| `RenderGraph` | Render passes | `std::function` callback | VkCommandBuffer |
| `Logger` | `EditorUI` | Callback registration | Log string |
| `ResourceManager` | `Streamer` | Pointer | Load requests |
| `OptimizerAI` | `SceneAnalyzer` | Singleton access | Scene stats |
| `VisibilitySystem` | `Frustum` | Composition | Culling results |

---

## 15. Folder Structure

```
COGENT_ENGINE/
├── Core/                           # Foundation Layer
│   ├── Memory/                     # NMM — Memory Allocators
│   │   ├── Allocator.hpp           # Abstract base
│   │   ├── LinearAllocator.hpp     # Frame scratch
│   │   └── PoolAllocator.hpp       # Object pool
│   ├── Threading/                  # CTS — Task Scheduler
│   │   └── JobSystem.hpp           # Thread pool
│   ├── Graphics/                   # Vulkan Backend
│   │   ├── GraphicsDevice.hpp/cpp  # Instance, Device, Queues
│   │   ├── Swapchain.hpp/cpp       # Presentation
│   │   ├── ShaderSystem.hpp/cpp    # SPIR-V loading
│   │   ├── PipelineCache.hpp/cpp   # Pipeline management
│   │   └── DescriptorManager.hpp/cpp # Descriptor allocation
│   ├── Diagnostics/                # Profiling & Debugging
│   │   ├── Profiler.hpp            # CPU scoped timers
│   │   ├── GpuProfiler.hpp/cpp     # GPU timestamp queries
│   │   └── CrashReporter.hpp       # Minidump generation
│   ├── Math/                       # Math Utilities
│   │   └── Frustum.hpp             # Frustum planes + tests
│   ├── Logger.hpp                  # Thread-safe logging
│   ├── Camera.hpp                  # FPS camera
│   ├── Types.hpp                   # Vertex, UBO, GameObject
│   └── VulkanUtils.hpp/cpp         # Helper functions
│
├── Renderer/                       # Rendering Layer
│   ├── Graph/                      # NX Graph
│   │   └── RenderGraph.hpp/cpp     # Pass scheduling + barriers
│   ├── GBuffer.hpp/cpp             # G-Buffer (6 attachments)
│   ├── RenderPipeline.hpp/cpp      # G-Buffer fill pipeline
│   ├── ShadowPass.hpp/cpp          # CSM (3 cascades)
│   ├── DeferredLightingPass.hpp/cpp
│   ├── SSAO.hpp/cpp                # Screen Space AO
│   ├── ScreenSpaceShadows.hpp/cpp  # Contact shadows
│   ├── TAAPass.hpp/cpp             # Temporal AA
│   ├── HDRPipeline.hpp/cpp         # Bloom + Tonemap
│   ├── InstanceBuffer.hpp/cpp      # GPU instancing
│   ├── ANSR/                       # Neural Super Resolution
│   │   └── ANSRPass.hpp/cpp
│   ├── Lighting/                   # Light management
│   │   └── LightCulling.hpp/cpp    # Clustered shading
│   ├── PostProcess/                # Post-processing
│   │   └── AutoExposurePass.hpp/cpp
│   └── Visibility/                 # Visibility System
│       └── VisibilitySystem.hpp/cpp
│
├── Optimization/                   # AOS Module
│   ├── OptimizerAI.hpp             # Heuristic optimizer
│   └── SceneAnalyzer.hpp           # Scene stats collector
│
├── RayTracing/                     # NRT Module
│   ├── RayTracer.hpp/cpp           # Compute-based RT
│   └── BVHManager.hpp/cpp          # (Future: Hardware RT)
│
├── Geometry/                       # Mesh Processing
│   ├── PrimitiveMesh.hpp/cpp       # Cube, Sphere, Plane
│   └── Meshlet.hpp                 # Meshlet builder
│
├── Scene/                          # Scene Management
│   └── GameObject.hpp              # Scene entity
│
├── Editor/                         # Editor UI
│   ├── EditorUI.hpp/cpp            # ImGui-based editor
│   └── console_impl.temp           # Console template
│
├── Resources/                      # Asset Management
│   ├── ResourceManager.hpp         # Centralized loader
│   ├── Texture.hpp/cpp             # Texture loading
│   ├── Model.hpp/cpp               # OBJ model loading
│   └── Streaming/
│       └── Streamer.hpp/cpp        # Async streaming
│
├── Shaders/                        # GLSL Shaders
│   ├── gbuffer.vert/frag           # Geometry pass
│   ├── lighting.vert/frag          # Deferred lighting
│   ├── taa.comp                    # TAA compute
│   ├── ssao.comp / ssao_blur.comp  # SSAO
│   ├── sss.comp                    # Screen space shadows
│   ├── light_culling.comp          # Clustered shading
│   ├── bloom_downsample/upsample.comp
│   ├── AutoExposure.comp           # Eye adaptation
│   ├── tonemap.frag                # HDR → LDR
│   ├── ANSR.comp                   # Neural upscaling
│   ├── raytrace.comp               # Compute RT
│   └── *.spv                       # Compiled SPIR-V
│
├── Vendor/                         # Third-party
│   ├── imgui/                      # Dear ImGui
│   ├── ImGuizmo/                   # Transform gizmos
│   └── VMA/                        # Vulkan Memory Allocator
│
├── Engine/                         # Engine Entry Point
│   ├── CogentEngine.hpp            # Main engine class
│   └── CogentEngine.cpp            # Implementation
│
├── Docs/                           # Documentation (this folder)
├── build/                          # CMake build output
├── CMakeLists.txt                  # Build configuration
├── main.cpp                        # Entry point
└── embed_shaders.py                # Shader embedding script
```

---

## 16. Pseudocode — Core Algorithms

### 16.1 Frame Rendering Loop

```
function drawFrame():
    vkWaitForFences(device, inFlightFence)
    
    result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore)
    if result == VK_ERROR_OUT_OF_DATE_KHR:
        recreateSwapchain()
        return
    
    vkResetFences(device, inFlightFence)
    vkResetCommandBuffer(commandBuffer)
    
    recordCommandBuffer(commandBuffer, imageIndex)
    
    submitInfo = {
        waitSemaphores = [imageAvailableSemaphore]
        waitStages = [COLOR_ATTACHMENT_OUTPUT]
        commandBuffers = [commandBuffer]
        signalSemaphores = [renderFinishedSemaphore]
    }
    
    vkQueueSubmit(graphicsQueue, submitInfo, inFlightFence)
    
    presentInfo = {
        waitSemaphores = [renderFinishedSemaphore]
        swapchains = [swapchain]
        imageIndices = [imageIndex]
    }
    
    vkQueuePresentKHR(presentQueue, presentInfo)
```

### 16.2 Render Graph Compilation

```
function compile():
    // Topological sort passes based on resource dependencies
    // Currently linear order as added
    
    for each pass in passes:
        if pass.setup:
            pass.setup(device)

function execute(cmd, imageIndex):
    for each pass in passes:
        for each input in pass.inputs:
            resource = resources[input.name]
            insertBarrier(cmd, resource, input)
        
        for each output in pass.outputs:
            resource = resources[output.name]
            insertBarrier(cmd, resource, output)
        
        pass.execute(cmd, imageIndex)
```

### 16.3 Visibility Culling

```
function cull(allObjects, visibleObjects):
    visibleObjects.clear()
    
    for each obj in allObjects:
        // Test AABB against 6 frustum planes
        if frustum.testAABB(obj.aabbMin, obj.aabbMax) != OUTSIDE:
            visibleObjects.push_back(&obj)
    
    // Sort front-to-back for early-Z optimization
    sort(visibleObjects, by distance to camera)
```

---

## 17. API Contract

### 17.1 GraphicsDevice API

```
GraphicsDevice::init(surface: VkSurfaceKHR) → void
    Precondition: Valid VkSurfaceKHR
    Postcondition: VkInstance, VkDevice, VkQueues, VmaAllocator created
    Throws: std::runtime_error on failure

GraphicsDevice::getDevice() → VkDevice
    Precondition: init() called
    Returns: Valid VkDevice handle

GraphicsDevice::beginSingleTimeCommands() → VkCommandBuffer
    Precondition: init() called
    Returns: Begun command buffer from internal pool
    Note: Must pair with endSingleTimeCommands()

GraphicsDevice::endSingleTimeCommands(cmd: VkCommandBuffer) → void
    Precondition: cmd from beginSingleTimeCommands()
    Postcondition: Command buffer submitted and waited, then freed
```

### 17.2 RenderGraph API

```
RenderGraph::registerImage(name, image, view, format, layout) → void
    Precondition: Valid Vulkan image handles
    Postcondition: Resource tracked with given initial state

RenderGraph::addPass(node: RenderPassNode) → void
    Precondition: node has valid execute function
    Postcondition: Pass appended to execution list

RenderGraph::execute(cmd, imageIndex) → void
    Precondition: Valid command buffer in recording state
    Postcondition: All passes executed with barriers inserted
```

### 17.3 GBuffer API

```
GBuffer::init() → void
    Precondition: GraphicsDevice initialized
    Postcondition: 6 render targets created at specified resolution

GBuffer::resize(width, height) → void
    Precondition: init() previously called
    Postcondition: All targets recreated at new resolution
    Note: Invalidates all descriptor sets referencing G-Buffer views
```

---

## 18. Dependency Graph

### 18.1 Build-Time Dependencies

```mermaid
graph TD
    MAIN[main.cpp] --> ENGINE_H[CogentEngine.hpp]
    ENGINE_H --> GD_H[GraphicsDevice.hpp]
    ENGINE_H --> GB_H[GBuffer.hpp]
    ENGINE_H --> RG_H[RenderGraph.hpp]
    ENGINE_H --> TAA_H[TAAPass.hpp]
    ENGINE_H --> HDR_H[HDRPipeline.hpp]
    ENGINE_H --> SSAO_H[SSAO.hpp]
    ENGINE_H --> SS_H[ScreenSpaceShadows.hpp]
    ENGINE_H --> RT_H[RayTracer.hpp]
    ENGINE_H --> EUI_H[EditorUI.hpp]
    ENGINE_H --> CAM_H[Camera.hpp]
    ENGINE_H --> TYPES_H[Types.hpp]

    GD_H --> VK_H[vulkan/vulkan.h]
    GD_H --> VMA_H[vk_mem_alloc.h]
    GB_H --> GD_H
    RG_H --> GD_H
    TAA_H --> GD_H
    TAA_H --> PC_H[PipelineCache.hpp]
    HDR_H --> GD_H
    HDR_H --> SH_H[ShaderSystem.hpp]
    HDR_H --> PC_H
    SSAO_H --> GD_H
    SSAO_H --> PC_H
    EUI_H --> IMGUI_H[imgui.h]
    TYPES_H --> VK_H
    TYPES_H --> GLM_H[glm/glm.hpp]
```

### 18.2 Runtime Dependency Order

```mermaid
graph LR
    VK_INIT[Vulkan Init] --> DEVICE[GraphicsDevice]
    DEVICE --> SWAP[Swapchain]
    DEVICE --> SHADER[ShaderSystem]
    DEVICE --> PCACHE[PipelineCache]
    DEVICE --> DMGR[DescriptorManager]
    
    DEVICE --> GBUFFER[GBuffer]
    DEVICE --> SHADOWS[ShadowPass]
    GBUFFER --> SSAO_D[SSAO]
    GBUFFER --> SSS_D[ScreenSpaceShadows]
    GBUFFER --> DEFERRED_D[DeferredLighting]
    SHADOWS --> DEFERRED_D
    SSAO_D --> DEFERRED_D
    DEFERRED_D --> TAA_D[TAAPass]
    TAA_D --> HDR_D[HDRPipeline]
    HDR_D --> PRESENT_D2[Present]
```

---

## 19. Advantages and Disadvantages

### 19.1 Deferred Rendering

| Aspect | Detail |
|--------|--------|
| **Advantage** | Decouples geometry from lighting → unlimited lights |
| **Advantage** | Single geometry pass → reduced draw calls |
| **Advantage** | Material data accessible in lighting pass for advanced PBR |
| **Disadvantage** | High VRAM cost (32 bytes/pixel G-Buffer) |
| **Disadvantage** | No native MSAA support (requires TAA or edge AA) |
| **Disadvantage** | Transparent objects require separate Forward pass |
| **Trade-off** | Chose deferred over forward+ for dense lighting scenarios |
| **Alternative** | Visibility Buffer rendering (deferred with single 32-bit ID buffer) — deferred to Phase 2 |
| **Rationale** | Deferred is proven, well-understood, and scales to hundreds of lights |

### 19.2 Render Graph

| Aspect | Detail |
|--------|--------|
| **Advantage** | Automatic barrier insertion eliminates manual synchronization bugs |
| **Advantage** | Modular — add/remove passes without modifying core rendering |
| **Advantage** | Enables future transient resource aliasing (memory savings) |
| **Disadvantage** | Runtime overhead of graph traversal (minimal) |
| **Disadvantage** | Debugging complexity — pass execution order non-obvious |
| **Trade-off** | Accepted small runtime cost for maintainability and correctness |
| **Alternative** | Hardcoded pass ordering — simpler but rigid and error-prone |
| **Rationale** | Industry standard (Frostbite, UE5) — proven scalable architecture |

### 19.3 Compute-Based Post Processing

| Aspect | Detail |
|--------|--------|
| **Advantage** | Async compute potential — overlap with graphics work |
| **Advantage** | No render pass overhead for screen-space effects |
| **Advantage** | Group shared memory for optimization (SSAO kernel, etc.) |
| **Disadvantage** | Requires explicit image barriers (no subpass dependencies) |
| **Trade-off** | Chose compute over fragment shaders for flexibility and performance |
| **Alternative** | Fragment shader post-process (simpler, but less optimal) |

### 19.4 Singleton Pattern (Logger, Profiler, JobSystem)

| Aspect | Detail |
|--------|--------|
| **Advantage** | Global access without parameter threading |
| **Advantage** | Guaranteed single instance |
| **Disadvantage** | Tight coupling to global state |
| **Disadvantage** | Harder to unit test (mock injection difficult) |
| **Trade-off** | Accepted for engine-wide services; game objects use composition |
| **Mitigation** | Limit singletons to infrastructure (Logger, Profiler, JobSystem) |

---

## 20. Risk

| Risk | Category | Severity | Probability |
|------|----------|----------|-------------|
| Vulkan driver bugs across vendors | Technical | High | Medium |
| G-Buffer VRAM pressure at 4K | Memory | High | High |
| Thread contention in job system | Performance | Medium | Low |
| Shader compilation stalls | Performance | Medium | Medium |
| Resource leak on hot-reload | Memory | Medium | Medium |
| Descriptor pool exhaustion | Technical | Medium | Low |

---

## 21. Mitigation

| Risk | Mitigation Strategy |
|------|-------------------|
| Vulkan driver bugs | Enable validation layers in dev; test on NVIDIA, AMD, Intel |
| G-Buffer VRAM | Packed formats (R10G10B10A2), reconstruction from depth |
| Thread contention | Lock-free queue, work-stealing scheduler |
| Shader compilation | Pipeline cache serialization, background compilation |
| Resource leaks | RAII wrappers, VMA statistics tracking |
| Descriptor exhaustion | Dynamic pool growth in DescriptorAllocator |

---

## 22. Future Improvement

- **C++20 Modules**: Replace `#include` with `import` for faster builds
- **Bindless Descriptors**: `VK_EXT_descriptor_indexing` for unlimited textures
- **Async Compute Queue**: Overlap compute and graphics work
- **Multi-Frame-in-Flight**: Double/triple buffering for GPU pipeline saturation
- **Hot Shader Reload**: Recompile shaders without restarting engine
- **Plugin Architecture**: Dynamic loading of engine modules
- **ECS (Entity Component System)**: Replace `GameObject` struct with data-oriented ECS
- **Scene Graph**: Hierarchical transform management with dirty flags

---

*End of Document — 02_ARCHITECTURE_DOCUMENT.md*  
*COGENT ENGINE © 2026 — All Rights Reserved*
