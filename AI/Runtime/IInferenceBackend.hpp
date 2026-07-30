#pragma once
#include <string>
#include <vector>

namespace Cogent::AI::Runtime {

    enum class TensorPrecision {
        FP32,
        FP16,   // Half precision for extreme speed (Requires Tensor Cores / Modern GPU)
        INT8    // Quantized
    };

    struct TensorData {
        void* data;
        size_t sizeBytes;
        std::vector<int> dimensions;
    };

    class IInferenceBackend {
    public:
        virtual ~IInferenceBackend() = default;

        // Load model weights from disk (e.g. .onnx, .engine)
        virtual bool LoadModel(const std::string& modelPath, TensorPrecision precision) = 0;

        // Queue inference task asynchronously on the backend
        // In a real implementation this binds to Vulkan Compute / CUDA stream
        virtual bool ExecuteAsync(const std::vector<TensorData>& inputs, std::vector<TensorData>& outputs) = 0;

        // Block until execution is finished
        virtual void Synchronize() = 0;

        virtual std::string GetBackendName() const = 0;
    };
}
