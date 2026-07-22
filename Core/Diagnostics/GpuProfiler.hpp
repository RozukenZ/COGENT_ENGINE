#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "../Graphics/GraphicsDevice.hpp"

namespace Cogent::Diagnostics {

    struct GpuTimerScope {
        std::string name;
        uint32_t startIndex;
        uint32_t endIndex;
    };

    class GpuProfiler {
    public:
        static GpuProfiler& Get() {
            static GpuProfiler instance;
            return instance;
        }

        void Init(GraphicsDevice& device);
        void Cleanup();

        void BeginFrame(VkCommandBuffer cmd);
        void EndFrame();

        void BeginTimestamp(VkCommandBuffer cmd, const std::string& name);
        void EndTimestamp(VkCommandBuffer cmd, const std::string& name);

        // Retrieve results from previous frame
        void RetrieveResults();

    private:
        GpuProfiler() = default;

        GraphicsDevice* _device = nullptr;
        VkQueryPool _queryPool = VK_NULL_HANDLE;
        float _timestampPeriod = 1.0f;

        static const uint32_t MAX_QUERIES = 128; // Max 64 scopes per frame
        uint32_t _currentQuery = 0;

        std::vector<GpuTimerScope> _activeScopes;
        std::unordered_map<std::string, float> _results; // Name -> Time in ms
    };
}
