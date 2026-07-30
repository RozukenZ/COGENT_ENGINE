#include <iostream>
#include <chrono>
#include <thread>
#include "AI/Runtime/NAIRuntime.hpp"
#include "AI/Runtime/IInferenceBackend.hpp"
#include "Core/Threading/JobSystem.hpp"

using namespace Cogent::AI::Runtime;

// Mock Backend to simulate TensorRT / ONNX
class MockBackend : public IInferenceBackend {
public:
    MockBackend(const std::string& name) : backendName(name) {}

    bool LoadModel(const std::string& modelPath, TensorPrecision precision) override {
        // Simulate loading time
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return true;
    }

    bool ExecuteAsync(const std::vector<TensorData>& inputs, std::vector<TensorData>& outputs) override {
        // Simulate Matrix Multiplication workload on tensor cores
        volatile double dummy = 0.0;
        for (int i = 0; i < 50000; i++) dummy += 0.5;
        return true;
    }

    void Synchronize() override {
        // Mock sync
    }

    std::string GetBackendName() const override {
        return backendName;
    }

private:
    std::string backendName;
};

int main() {
    std::cout << "--- COGENT ENGINE NAI RUNTIME STRESS TEST ---\n";
    
    Cogent::Threading::JobSystem::Get().Initialize();

    auto& nai = NAIRuntime::Get();
    nai.Initialize();

    // Register Models for FP16 and INT8
    auto trtBackend = std::make_shared<MockBackend>("TensorRT_v8");
    auto onnxBackend = std::make_shared<MockBackend>("ONNX_DirectML");

    nai.RegisterModel("Upscaler_NSR.trt", trtBackend, TensorPrecision::FP16);
    nai.RegisterModel("Denoiser_NDN.onnx", onnxBackend, TensorPrecision::INT8);

    std::cout << "[INFO] Submitting 50 asynchronous inference requests...\n";

    std::vector<std::shared_ptr<InferenceRequest>> requests;
    for (int i = 0; i < 50; i++) {
        auto req = std::make_shared<InferenceRequest>();
        req->modelName = (i % 2 == 0) ? "Upscaler_NSR.trt" : "Denoiser_NDN.onnx";
        requests.push_back(req);
        nai.SubmitInference(req);
    }

    auto start = std::chrono::high_resolution_clock::now();
    
    // Process queue
    nai.FlushQueue();

    // Wait until all requests are marked completed
    bool allDone = false;
    while (!allDone) {
        allDone = true;
        for (const auto& req : requests) {
            if (!req->isCompleted) {
                allDone = false;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto end = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "\n--- NAI RESULTS ---\n";
    std::cout << "Processed 50 inferences in : " << timeMs << " ms\n";
    
    if (timeMs < 100.0) { // Should be very fast due to threading
        std::cout << "[PASS] GPU Inference Scheduler efficiently dispatched all tasks without deadlocks!\n";
    } else {
        std::cerr << "[FAIL] Inference took too long, threading might be blocked.\n";
        return 1;
    }

    nai.Shutdown();
    Cogent::Threading::JobSystem::Get().Shutdown();
    return 0;
}
