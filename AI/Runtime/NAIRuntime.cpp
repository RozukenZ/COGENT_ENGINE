#include "NAIRuntime.hpp"
#include <iostream>

namespace Cogent::AI::Runtime {

    void NAIRuntime::Initialize() {
        std::cout << "[NAI] Runtime Initialized. Neural Engines Online.\n";
    }

    void NAIRuntime::Shutdown() {
        _activeModels.clear();
        std::cout << "[NAI] Runtime Shutdown.\n";
    }

    bool NAIRuntime::RegisterModel(const std::string& modelName, std::shared_ptr<IInferenceBackend> backend, TensorPrecision precision) {
        if (!backend->LoadModel(modelName, precision)) {
            std::cerr << "[NAI] Failed to load model: " << modelName << "\n";
            return false;
        }
        _activeModels[modelName] = backend;
        
        std::string precStr = (precision == TensorPrecision::FP16) ? "FP16" : ((precision == TensorPrecision::INT8) ? "INT8" : "FP32");
        std::cout << "[NAI] Model Registered: " << modelName << " [" << backend->GetBackendName() << "] Precision: " << precStr << "\n";
        return true;
    }

    void NAIRuntime::SubmitInference(std::shared_ptr<InferenceRequest> request) {
        std::lock_guard lock(_queueMutex);
        _requestQueue.push(request);
    }

    void NAIRuntime::FlushQueue() {
        auto& jobSystem = Threading::JobSystem::Get();

        std::lock_guard lock(_queueMutex);
        while (!_requestQueue.empty()) {
            auto request = _requestQueue.front();
            _requestQueue.pop();

            auto it = _activeModels.find(request->modelName);
            if (it == _activeModels.end()) {
                std::cerr << "[NAI] Warning: Requested model not loaded -> " << request->modelName << "\n";
                continue;
            }

            auto backend = it->second;
            
            // Delegate inference execution to Background Task Scheduler (CTS)
            // so it runs asynchronously without blocking the Main Thread
            jobSystem.Execute([backend, request]() {
                backend->ExecuteAsync(request->inputs, request->outputs);
                backend->Synchronize();
                request->isCompleted = true;
            });
        }
    }
}
