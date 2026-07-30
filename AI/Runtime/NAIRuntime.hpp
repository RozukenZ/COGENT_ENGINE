#pragma once
#include <memory>
#include <unordered_map>
#include <mutex>
#include <queue>
#include "IInferenceBackend.hpp"
#include "../../Core/Threading/JobSystem.hpp"

namespace Cogent::AI::Runtime {

    struct InferenceRequest {
        std::string modelName;
        std::vector<TensorData> inputs;
        std::vector<TensorData> outputs;
        bool isCompleted = false;
    };

    class NAIRuntime {
    public:
        static NAIRuntime& Get() {
            static NAIRuntime instance;
            return instance;
        }

        void Initialize();
        void Shutdown();

        // Register a model with a specific backend (e.g. ONNX or TRT stub)
        bool RegisterModel(const std::string& modelName, std::shared_ptr<IInferenceBackend> backend, TensorPrecision precision);

        // Submit inference to the async queue
        void SubmitInference(std::shared_ptr<InferenceRequest> request);

        // Processes all queued requests via CTS
        void FlushQueue();

    private:
        NAIRuntime() = default;

        std::unordered_map<std::string, std::shared_ptr<IInferenceBackend>> _activeModels;
        
        std::queue<std::shared_ptr<InferenceRequest>> _requestQueue;
        std::mutex _queueMutex;
    };
}
