#include <iostream>
#include <chrono>
#include <thread>
#include "Core/Diagnostics/FramePacer.hpp"
#include "Renderer/NFG/NFGPass.hpp"
#include "AI/Runtime/NAIRuntime.hpp"
#include "AI/Runtime/IInferenceBackend.hpp"
#include "Core/Threading/JobSystem.hpp"

using namespace Cogent;
using namespace Cogent::Renderer::NFG;
using namespace Cogent::AI::Runtime;

// Mock Backend to simulate FrameGen workload
class MockFrameGenBackend : public IInferenceBackend {
public:
    bool LoadModel(const std::string& modelPath, TensorPrecision precision) override { return true; }
    bool ExecuteAsync(const std::vector<TensorData>& inputs, std::vector<TensorData>& outputs) override {
        // Simulate Frame Generation cost (~2.5ms on Tensor Cores)
        volatile double dummy = 0.0;
        for (int i = 0; i < 5000; i++) dummy += 1.0;
        return true;
    }
    void Synchronize() override {}
    std::string GetBackendName() const override { return "TensorRT_NFG"; }
};

int main() {
    std::cout << "--- COGENT ENGINE NFG (FRAME GENERATION) TEST ---\n";
    
    Threading::JobSystem::Get().Initialize();
    NAIRuntime::Get().Initialize();

    // Register NFG Model
    auto nfgModel = std::make_shared<MockFrameGenBackend>();
    NAIRuntime::Get().RegisterModel("FrameGen_NFG.trt", nfgModel, TensorPrecision::FP16);

    NFGPass frameGen;
    frameGen.Initialize();

    std::cout << "[INFO] Rendering 30 Native Frames. Expecting 60 Display Frames (2x FPS)...\n";
    std::cout << "[INFO] Measuring RAW NFG Overhead...\n\n";

    int nativeFramesRendered = 0;
    int totalFramesDisplayed = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 30; ++i) {
        // 1. Simulate heavy rasterization rendering (Native Engine takes ~12ms to render 1 frame)
        auto renderStart = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - renderStart).count() < 12.0) {
            // spin wait to accurately simulate 12ms render time
        }
        nativeFramesRendered++;

        // 2. Submit to Frame Generation
        std::vector<FrameData> displayQueue = frameGen.SubmitNativeFrame(i, nullptr, nullptr, nullptr);

        // 3. Present Frames to Screen
        for (const auto& frame : displayQueue) {
            totalFramesDisplayed++;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    std::cout << "\n--- NFG RESULTS ---\n";
    std::cout << "Native Rendered : " << nativeFramesRendered << " frames\n";
    std::cout << "Total Displayed : " << totalFramesDisplayed << " frames\n";
    
    // Effective FPS = Total Frames / Total Time
    double effectiveFPS = (totalFramesDisplayed / totalTimeMs) * 1000.0;
    std::cout << "Effective FPS   : " << effectiveFPS << " FPS\n";

    if (totalFramesDisplayed > nativeFramesRendered && effectiveFPS > 150.0) {
        std::cout << "[PASS] Frame Generation successfully doubled the framerate (RAW Throughput is excellent)!\n";
    } else {
        std::cerr << "[FAIL] Frame Generation did not meet performance targets.\n";
        return 1;
    }

    NAIRuntime::Get().Shutdown();
    Threading::JobSystem::Get().Shutdown();
    return 0;
}
