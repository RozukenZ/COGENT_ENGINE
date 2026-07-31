#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include "Core/Diagnostics/TelemetrySystem.hpp"
#include "Core/Threading/JobSystem.hpp"

using namespace Cogent::Core::Diagnostics;
using namespace Cogent::Threading;

int main() {
    std::cout << "--- COGENT ENGINE NX INSIGHT (TELEMETRY) TEST ---\n";

    JobSystem::Get().Initialize();
    TelemetrySystem::Get().Initialize();

    std::cout << "[INFO] Simulating Multithreaded Draw Call submissions from 4 Rendering Threads...\n";

    TelemetrySystem::Get().ResetPerFrameMetrics();

    auto start = std::chrono::high_resolution_clock::now();

    // Spawn 4 threads that blast draw calls simultaneously
    for (int threadIdx = 0; threadIdx < 4; ++threadIdx) {
        JobSystem::Get().Execute([]() {
            // Each thread submits 25,000 draw calls
            for (int i = 0; i < 25000; i++) {
                // 1 draw call = 1000 triangles, 3000 vertices
                TelemetrySystem::Get().AddDrawCall(1000, 3000);
            }
        });
    }

    // Wait for jobs to finish
    JobSystem::Get().WaitAll();

    auto end = std::chrono::high_resolution_clock::now();
    double timeTakenMs = std::chrono::duration<double, std::milli>(end - start).count();

    // Mock timings and memory
    TelemetrySystem::Get().UpdateTimings(4.5, 1.2, 5.7, 165.0);
    TelemetrySystem::Get().UpdateMemory(1024 * 1024 * 500, 1024 * 1024 * 1500); // 500MB RAM, 1.5GB VRAM

    // Fetch aggregated data
    TelemetryData data = TelemetrySystem::Get().GetCurrentData();

    std::cout << "\n--- NX INSIGHT DASHBOARD ---\n";
    std::cout << "Draw Calls     : " << data.drawCalls << "\n";
    std::cout << "Triangles      : " << data.triangleCount << "\n";
    std::cout << "Vertices       : " << data.vertexCount << "\n";
    std::cout << "GPU Frame Time : " << data.gpuFrameTime << " ms\n";
    std::cout << "Effective FPS  : " << data.effectiveFps << " FPS\n";

    double rawFps = 55.0; // Simulated FPS without AOS/NVS/NFG
    double optFps = data.effectiveFps;
    double gain = TelemetrySystem::Get().CalculateGainPercentage(rawFps, optFps);

    std::cout << "\n--- PERFORMANCE COMPARE ---\n";
    std::cout << "RAW Mode       : " << rawFps << " FPS\n";
    std::cout << "OPTIMIZED Mode : " << optFps << " FPS\n";
    std::cout << "GAIN           : +" << gain << " %\n";
    
    std::cout << "Telemetry Overhead: " << timeTakenMs << " ms to aggregate " << data.drawCalls << " calls.\n";

    if (data.drawCalls == 100000 && data.triangleCount == 100000000) {
        std::cout << "[PASS] Telemetry Data is thread-safe and perfectly accurate!\n";
    } else {
        std::cerr << "[FAIL] Data race detected in atomic counters.\n";
        return 1;
    }

    TelemetrySystem::Get().Shutdown();
    JobSystem::Get().Shutdown();
    return 0;
}
